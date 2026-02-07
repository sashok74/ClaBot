import express, { Request, Response } from 'express';
import cors from 'cors';
import { v4 as uuidv4 } from 'uuid';
import {
  AgentSession,
  AgentEvent,
  CreateAgentRequest,
  CreateAgentResponse,
  QueryRequest,
  QueryResponse,
  StatusResponse,
  SessionResponse,
} from './types';
import { MockAgent } from './mock-agent';
import { RealAgent } from './real-agent';

const app = express();

// Middleware
app.use(cors());
app.use(express.json());

// Check if we should use real agent (ANTHROPIC_API_KEY is set or Claude Code is authenticated)
const USE_REAL_AGENT = process.env.USE_REAL_AGENT === 'true' || process.env.ANTHROPIC_API_KEY;

// In-memory storage for agent sessions
const sessions = new Map<string, AgentSession>();
const agents = new Map<string, MockAgent | RealAgent>();

// Health check
app.get('/health', (_req: Request, res: Response) => {
  res.json({
    status: 'ok',
    mode: USE_REAL_AGENT ? 'real' : 'mock',
    timestamp: new Date().toISOString()
  });
});

// POST /agent/create - Create a new agent session
app.post('/agent/create', (req: Request<{}, {}, CreateAgentRequest>, res: Response<CreateAgentResponse>) => {
  const { name, systemPrompt, model, allowedTools, cwd } = req.body;

  const id = uuidv4();
  const session: AgentSession = {
    id,
    config: {
      id,
      name: name || 'Unnamed Agent',
      systemPrompt,
      model: model || 'sonnet',
      allowedTools,
      cwd,
    },
    status: 'created',
    createdAt: new Date(),
    eventListeners: new Set(),
    // Session tracking fields
    canResume: false,
    inputTokens: 0,
    outputTokens: 0,
    totalCostUsd: 0,
  };

  sessions.set(id, session);

  // Create real or mock agent based on configuration
  if (USE_REAL_AGENT) {
    agents.set(id, new RealAgent(session));
  } else {
    agents.set(id, new MockAgent(session));
  }

  console.log(`[Server] Created agent: ${id} (${name}) [${USE_REAL_AGENT ? 'REAL' : 'MOCK'}]`);

  res.json({ id, status: 'created' });
});

// GET /agent/:id/events - SSE stream of agent events
app.get('/agent/:id/events', (req: Request<{ id: string }>, res: Response) => {
  const { id } = req.params;
  const session = sessions.get(id);

  if (!session) {
    res.status(404).json({ error: 'Agent not found' });
    return;
  }

  // Set SSE headers
  res.setHeader('Content-Type', 'text/event-stream');
  res.setHeader('Cache-Control', 'no-cache');
  res.setHeader('Connection', 'keep-alive');
  res.setHeader('X-Accel-Buffering', 'no');

  console.log(`[Server] SSE connected: ${id.slice(0, 8)}`);

  // Send initial connection event
  res.write(`data: ${JSON.stringify({ type: 'connected', agentId: id })}\n\n`);

  // Event listener for this connection
  const listener = (event: AgentEvent) => {
    res.write(`data: ${JSON.stringify(event)}\n\n`);
  };

  session.eventListeners.add(listener);

  // Cleanup on disconnect
  req.on('close', () => {
    console.log(`[Server] SSE disconnected: ${id.slice(0, 8)}`);
    session.eventListeners.delete(listener);
  });
});

// POST /agent/:id/query - Send a prompt to the agent
app.post('/agent/:id/query', async (req: Request<{ id: string }, {}, QueryRequest>, res: Response<QueryResponse>) => {
  const { id } = req.params;
  const { prompt, resume } = req.body;

  const session = sessions.get(id);
  const agent = agents.get(id);

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

  // Start agent (async, returns immediately)
  // Pass resume flag to runQuery for both RealAgent and MockAgent
  agent.runQuery(prompt, resumeSession);

  res.json({ status: 'processing' });
});

// GET /agent/:id/status - Get agent status
app.get('/agent/:id/status', (req: Request<{ id: string }>, res: Response<StatusResponse | { error: string }>) => {
  const { id } = req.params;
  const session = sessions.get(id);

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

// GET /agent/:id/session - Get session info for resume
app.get('/agent/:id/session', (req: Request<{ id: string }>, res: Response<SessionResponse | { error: string }>) => {
  const { id } = req.params;
  const session = sessions.get(id);

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

// POST /agent/:id/interrupt - Interrupt the agent
app.post('/agent/:id/interrupt', (req: Request<{ id: string }>, res: Response) => {
  const { id } = req.params;
  const session = sessions.get(id);
  const agent = agents.get(id);

  if (!session || !agent) {
    res.status(404).json({ error: 'Agent not found' });
    return;
  }

  console.log(`[Server] Interrupt: ${id.slice(0, 8)}`);

  agent.interrupt();

  res.json({ status: 'interrupted' });
});

// DELETE /agent/:id - Delete agent session
app.delete('/agent/:id', (req: Request<{ id: string }>, res: Response) => {
  const { id } = req.params;
  const agent = agents.get(id);

  if (agent) {
    agent.stop();
  }

  sessions.delete(id);
  agents.delete(id);

  console.log(`[Server] Deleted agent: ${id.slice(0, 8)}`);

  res.json({ status: 'deleted' });
});

// GET /agents - List all agents
app.get('/agents', (_req: Request, res: Response) => {
  const list = Array.from(sessions.values()).map((s) => ({
    id: s.id,
    name: s.config.name,
    status: s.status,
    createdAt: s.createdAt,
  }));

  res.json(list);
});

export { app };
