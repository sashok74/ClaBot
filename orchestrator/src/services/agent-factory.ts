import { AgentSession } from '../types/domain';
import { BaseAgent } from '../agents/base-agent';
import { RealAgent } from '../agents/real-agent';
import { EventBus } from './event-bus';

export class AgentFactory {
  private agents = new Map<string, BaseAgent>();

  constructor(private eventBus: EventBus) {}

  create(session: AgentSession): BaseAgent {
    const agent = new RealAgent(session, this.eventBus);
    this.agents.set(session.id, agent);
    return agent;
  }

  get(id: string): BaseAgent | undefined {
    return this.agents.get(id);
  }

  delete(id: string): void {
    const agent = this.agents.get(id);
    if (agent) {
      agent.stop();
    }
    this.agents.delete(id);
  }
}
