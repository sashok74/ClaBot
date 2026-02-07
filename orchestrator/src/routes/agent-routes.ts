import { Router, Request, Response } from 'express';
import {
  CreateAgentRequest,
  CreateAgentResponse,
  QueryRequest,
  QueryResponse,
  StatusResponse,
  SessionResponse,
} from '../types/api';
import { AgentEvent } from '../types/domain';
import { MAX_SESSIONS } from '../config';
import { SessionManager } from '../services/session-manager';
import { AgentFactory } from '../services/agent-factory';
import { EventBus } from '../services/event-bus';

export function registerAgentRoutes(
  router: Router,
  sessionManager: SessionManager,
  agentFactory: AgentFactory,
  eventBus: EventBus,
): void {
  // POST /agent/create
  router.post('/agent/create', (req: Request<{}, {}, CreateAgentRequest>, res: Response<CreateAgentResponse | { error: string }>) => {
    const body = req.body;
    const VALID_MODELS = ['sonnet', 'opus', 'haiku'];

    if (!body.name || typeof body.name !== 'string' || body.name.trim().length === 0 || body.name.trim().length > 200) {
      res.status(400).json({ error: 'name is required (string, 1-200 chars)' } as any);
      return;
    }
    if (body.model !== undefined && !VALID_MODELS.includes(body.model)) {
      res.status(400).json({ error: `model must be one of: ${VALID_MODELS.join(', ')}` } as any);
      return;
    }
    if (body.allowedTools !== undefined && (!Array.isArray(body.allowedTools) || !body.allowedTools.every((t: unknown) => typeof t === 'string'))) {
      res.status(400).json({ error: 'allowedTools must be an array of strings' } as any);
      return;
    }
    if (body.disallowedTools !== undefined && (!Array.isArray(body.disallowedTools) || !body.disallowedTools.every((t: unknown) => typeof t === 'string'))) {
      res.status(400).json({ error: 'disallowedTools must be an array of strings' } as any);
      return;
    }
    if (body.maxTurns !== undefined && (typeof body.maxTurns !== 'number' || body.maxTurns <= 0)) {
      res.status(400).json({ error: 'maxTurns must be a number > 0' } as any);
      return;
    }
    if (body.maxBudgetUsd !== undefined && (typeof body.maxBudgetUsd !== 'number' || body.maxBudgetUsd <= 0)) {
      res.status(400).json({ error: 'maxBudgetUsd must be a number > 0' } as any);
      return;
    }

    body.name = body.name.trim();

    // Evict oldest finished session if at capacity
    if (sessionManager.size >= MAX_SESSIONS) {
      const evictedId = sessionManager.evict();
      if (evictedId) {
        agentFactory.delete(evictedId);
        eventBus.removeAll(evictedId);
      } else {
        res.status(503).json({ error: `Maximum sessions (${MAX_SESSIONS}) reached, all still active` } as any);
        return;
      }
    }

    const session = sessionManager.create(body);
    if (!session) {
      res.status(503).json({ error: `Maximum sessions (${MAX_SESSIONS}) reached` } as any);
      return;
    }
    agentFactory.create(session);

    console.log(`[Server] Created agent: ${session.id} (${session.config.name})`);

    res.json({ id: session.id, status: 'created' });
  });

  // GET /agent/:id/events — SSE stream
  router.get('/agent/:id/events', (req: Request<{ id: string }>, res: Response) => {
    const { id } = req.params;
    const session = sessionManager.get(id);

    if (!session) {
      res.status(404).json({ error: 'Agent not found' });
      return;
    }

    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');
    res.setHeader('X-Accel-Buffering', 'no');

    console.log(`[Server] SSE connected: ${id.slice(0, 8)}`);

    const connectedEvent: AgentEvent = { type: 'connected', agentId: id };
    res.write(`data: ${JSON.stringify(connectedEvent)}\n\n`);

    const listener = (event: AgentEvent) => {
      res.write(`data: ${JSON.stringify(event)}\n\n`);
      if (event.type === 'session_end' && event.reason === 'deleted') {
        res.end();
      }
    };

    eventBus.subscribe(id, listener);

    req.on('close', () => {
      console.log(`[Server] SSE disconnected: ${id.slice(0, 8)}`);
      eventBus.unsubscribe(id, listener);
    });
  });

  // POST /agent/:id/query
  router.post('/agent/:id/query', async (req: Request<{ id: string }, {}, QueryRequest>, res: Response<QueryResponse>) => {
    const { id } = req.params;
    const { prompt, resume } = req.body;

    if (!prompt || typeof prompt !== 'string' || prompt.trim().length === 0) {
      res.status(400).json({ status: 'error', message: 'prompt is required (non-empty string)' });
      return;
    }
    if (resume !== undefined && typeof resume !== 'boolean') {
      res.status(400).json({ status: 'error', message: 'resume must be a boolean' });
      return;
    }

    const session = sessionManager.get(id);
    const agent = agentFactory.get(id);

    if (!session || !agent) {
      res.status(404).json({ status: 'error', message: 'Agent not found' });
      return;
    }

    if (session.status === 'running') {
      res.status(400).json({ status: 'error', message: 'Agent is already running' });
      return;
    }

    const resumeSession = resume && session.canResume;
    console.log(`[Server] Query: ${id.slice(0, 8)} - "${prompt.slice(0, 50)}..." (resume: ${resumeSession})`);

    agent.runQuery(prompt, resumeSession);

    res.json({ status: 'processing' });
  });

  // GET /agent/:id/status
  router.get('/agent/:id/status', (req: Request<{ id: string }>, res: Response<StatusResponse | { error: string }>) => {
    const { id } = req.params;
    const session = sessionManager.get(id);

    if (!session) {
      res.status(404).json({ error: 'Agent not found' });
      return;
    }

    res.json({
      id: session.id,
      status: session.status,
      config: session.config,
    });
  });

  // GET /agent/:id/session
  router.get('/agent/:id/session', (req: Request<{ id: string }>, res: Response<SessionResponse | { error: string }>) => {
    const { id } = req.params;
    const session = sessionManager.get(id);

    if (!session) {
      res.status(404).json({ error: 'Agent not found' });
      return;
    }

    res.json({
      id: session.id,
      sdkSessionId: session.sdkSessionId,
      canResume: session.canResume,
      inputTokens: session.inputTokens,
      outputTokens: session.outputTokens,
      totalCostUsd: session.totalCostUsd,
    });
  });

  // POST /agent/:id/interrupt
  router.post('/agent/:id/interrupt', (req: Request<{ id: string }>, res: Response) => {
    const { id } = req.params;
    const session = sessionManager.get(id);
    const agent = agentFactory.get(id);

    if (!session || !agent) {
      res.status(404).json({ error: 'Agent not found' });
      return;
    }

    console.log(`[Server] Interrupt: ${id.slice(0, 8)}`);
    agent.interrupt();
    res.json({ status: 'interrupted' });
  });

  // DELETE /agent/:id
  router.delete('/agent/:id', (req: Request<{ id: string }>, res: Response) => {
    const { id } = req.params;

    agentFactory.delete(id);
    eventBus.close(id);
    sessionManager.delete(id);

    console.log(`[Server] Deleted agent: ${id.slice(0, 8)}`);
    res.json({ status: 'deleted' });
  });

  // GET /agents
  router.get('/agents', (_req: Request, res: Response) => {
    const list = sessionManager.list().map((s) => ({
      id: s.id,
      name: s.config.name,
      status: s.status,
      createdAt: s.createdAt,
    }));
    res.json(list);
  });
}
