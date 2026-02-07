import { v4 as uuidv4 } from 'uuid';
import { AgentSession } from '../types/domain';
import { CreateAgentRequest } from '../types/api';
import { MAX_SESSIONS } from '../config';

export class SessionManager {
  private sessions = new Map<string, AgentSession>();

  create(req: CreateAgentRequest): AgentSession | null {
    if (this.sessions.size >= MAX_SESSIONS && !this.findEvictable()) {
      return null;
    }

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

  /**
   * Evict the oldest completed/error session.
   * Returns the evicted session id, or null if nothing to evict.
   */
  evict(): string | null {
    const oldest = this.findEvictable();
    if (oldest) {
      this.sessions.delete(oldest.id);
      console.log(`[SessionManager] Evicted session: ${oldest.id.slice(0, 8)} (${oldest.status})`);
      return oldest.id;
    }
    return null;
  }

  get size(): number {
    return this.sessions.size;
  }

  private findEvictable(): AgentSession | null {
    let oldest: AgentSession | null = null;
    for (const session of this.sessions.values()) {
      if (session.status === 'completed' || session.status === 'error') {
        if (!oldest || session.createdAt < oldest.createdAt) {
          oldest = session;
        }
      }
    }
    return oldest;
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
