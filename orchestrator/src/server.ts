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
} from './types';
import { MockAgent } from './mock-agent';

const app = express();

// Middleware
app.use(cors());
app.use(express.json());

// In-memory storage for agent sessions
const sessions = new Map<string, AgentSession>();
const mockAgents = new Map<string, MockAgent>();

// Health check
app.get('/health', (_req: Request, res: Response) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
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
  };

  sessions.set(id, session);
  mockAgents.set(id, new MockAgent(session));

  console.log(`[Server] Created agent: ${id} (${name})`);

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
  const { prompt } = req.body;

  const session = sessions.get(id);
  const mockAgent = mockAgents.get(id);

  if (!session || !mockAgent) {
    res.status(404).json({ status: 'error', message: 'Agent not found' });
    return;
  }

  if (session.status === 'running') {
    res.status(400).json({ status: 'error', message: 'Agent is already running' });
    return;
  }

  console.log(`[Server] Query: ${id.slice(0, 8)} - "${prompt.slice(0, 50)}..."`);

  // Start mock agent (async, returns immediately)
  mockAgent.runQuery(prompt);

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

// POST /agent/:id/interrupt - Interrupt the agent
app.post('/agent/:id/interrupt', (req: Request<{ id: string }>, res: Response) => {
  const { id } = req.params;
  const session = sessions.get(id);
  const mockAgent = mockAgents.get(id);

  if (!session || !mockAgent) {
    res.status(404).json({ error: 'Agent not found' });
    return;
  }

  console.log(`[Server] Interrupt: ${id.slice(0, 8)}`);

  mockAgent.interrupt();

  res.json({ status: 'interrupted' });
});

// DELETE /agent/:id - Delete agent session
app.delete('/agent/:id', (req: Request<{ id: string }>, res: Response) => {
  const { id } = req.params;
  const mockAgent = mockAgents.get(id);

  if (mockAgent) {
    mockAgent.stop();
  }

  sessions.delete(id);
  mockAgents.delete(id);

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
