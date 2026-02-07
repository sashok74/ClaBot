import express from 'express';
import cors from 'cors';
import { EventBus } from './services/event-bus';
import { SessionManager } from './services/session-manager';
import { AgentFactory } from './services/agent-factory';
import { registerHealthRoutes } from './routes/health-routes';
import { registerAgentRoutes } from './routes/agent-routes';

export function createApp() {
  const app = express();

  app.use(cors());
  app.use(express.json());

  const eventBus = new EventBus();
  const sessionManager = new SessionManager();
  const agentFactory = new AgentFactory(eventBus);

  registerHealthRoutes(app);
  registerAgentRoutes(app, sessionManager, agentFactory, eventBus);

  return app;
}
