import { AgentEvent } from '../types/domain';

export type EventListener = (event: AgentEvent) => void;

export class EventBus {
  private listeners = new Map<string, Set<EventListener>>();

  subscribe(agentId: string, listener: EventListener): void {
    let set = this.listeners.get(agentId);
    if (!set) {
      set = new Set();
      this.listeners.set(agentId, set);
    }
    set.add(listener);
  }

  unsubscribe(agentId: string, listener: EventListener): void {
    const set = this.listeners.get(agentId);
    if (set) {
      set.delete(listener);
      if (set.size === 0) {
        this.listeners.delete(agentId);
      }
    }
  }

  emit(agentId: string, event: AgentEvent): void {
    console.log(`[${agentId.slice(0, 8)}] Event: ${event.type}`);
    const set = this.listeners.get(agentId);
    if (!set) return;
    for (const listener of set) {
      try {
        listener(event);
      } catch (err) {
        console.error('Error in event listener:', err);
      }
    }
  }

  close(agentId: string): void {
    this.emit(agentId, {
      type: 'session_end',
      reason: 'deleted',
      usage: { inputTokens: 0, outputTokens: 0, totalCostUsd: 0, durationMs: 0 },
    });
    this.listeners.delete(agentId);
  }

  removeAll(agentId: string): void {
    this.listeners.delete(agentId);
  }
}
