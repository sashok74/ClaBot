import { v4 as uuidv4 } from 'uuid';
import { AgentSession } from '../types/domain';
import { CreateAgentRequest } from '../types/api';
import { MAX_SESSIONS } from '../config';

export class SessionManager {
  private sessions = new Map<string, AgentSession>();

  create(req: CreateAgentRequest): AgentSession {
    this.evictIfNeeded();

    const id = uuidv4();
    const session: AgentSession = {
      id,
      config: {
        id,
        name: req.name || 'Unnamed Agent',
        systemPrompt: req.systemPrompt,
        model: req.model || 'sonnet',
        allowedTools: req.allowedTools,
        disallowedTools: req.disallowedTools,
        maxTurns: req.maxTurns,
        maxBudgetUsd: req.maxBudgetUsd,
        permissionMode: req.permissionMode,
        mcpServers: req.mcpServers,
        cwd: req.cwd,
      },
      status: 'created',
      createdAt: new Date(),
      canResume: false,
      inputTokens: 0,
      outputTokens: 0,
      totalCostUsd: 0,
    };
    this.sessions.set(id, session);
    return session;
  }

  private evictIfNeeded(): void {
    if (this.sessions.size < MAX_SESSIONS) return;

    // Find oldest completed or error session
    let oldest: AgentSession | null = null;
    for (const session of this.sessions.values()) {
      if (session.status === 'completed' || session.status === 'error') {
        if (!oldest || session.createdAt < oldest.createdAt) {
          oldest = session;
        }
      }
    }
    if (oldest) {
      this.sessions.delete(oldest.id);
      console.log(`[SessionManager] Evicted session: ${oldest.id.slice(0, 8)} (${oldest.status})`);
    }
  }

  get(id: string): AgentSession | undefined {
    return this.sessions.get(id);
  }

  delete(id: string): boolean {
    return this.sessions.delete(id);
  }

  list(): AgentSession[] {
    return Array.from(this.sessions.values());
  }
}
