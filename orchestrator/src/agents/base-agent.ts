import { v4 as uuidv4 } from 'uuid';
import { AgentSession, AgentEvent, AgentStatus } from '../types/domain';
import { EventBus } from '../services/event-bus';
import { UsageTracker } from '../services/usage-tracker';

export abstract class BaseAgent {
  protected session: AgentSession;
  protected eventBus: EventBus;
  protected usageTracker = new UsageTracker();

  constructor(session: AgentSession, eventBus: EventBus) {
    this.session = session;
    this.eventBus = eventBus;
  }

  abstract runQuery(prompt: string, resume?: boolean): Promise<void>;
  abstract stop(): void;
  abstract interrupt(): void;

  protected emit(event: AgentEvent): void {
    this.eventBus.emit(this.session.id, event);
  }

  protected emitSessionStart(): void {
    this.emit({
      type: 'session_start',
      sessionId: this.session.id,
      config: this.session.config,
    });
  }

  protected emitUserMessage(prompt: string): void {
    this.emit({
      type: 'user_message',
      content: prompt,
      uuid: uuidv4(),
    });
  }

  protected emitSessionInfo(sdkSessionId: string): void {
    this.session.sdkSessionId = sdkSessionId;
    this.session.canResume = true;
    console.log(`[${this.constructor.name}] Session ID captured: ${sdkSessionId}`);
    this.emit({
      type: 'session_info',
      sdkSessionId,
      canResume: true,
    });
  }

  protected complete(status: AgentStatus): void {
    this.session.status = status;
    this.syncUsageToSession();
    const usage = this.usageTracker.getStats();
    this.emit({
      type: 'session_end',
      reason: status,
      usage,
    });
  }

  protected syncUsageToSession(): void {
    this.session.inputTokens = this.usageTracker.inputTokens;
    this.session.outputTokens = this.usageTracker.outputTokens;
    this.session.totalCostUsd = this.usageTracker.totalCostUsd;
  }
}
