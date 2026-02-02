import { query, Query } from '@anthropic-ai/claude-agent-sdk';
import { v4 as uuidv4 } from 'uuid';
import { AgentEvent, AgentSession, UsageStats } from './types';

export class RealAgent {
  private session: AgentSession;
  private abortController: AbortController | null = null;
  private queryInstance: Query | null = null;
  private startTime: number = 0;
  private inputTokens: number = 0;
  private outputTokens: number = 0;
  private totalCostUsd: number = 0;
  // Track tool_use_id -> tool_name for proper tool_end events
  private activeTools: Map<string, string> = new Map();

  constructor(session: AgentSession) {
    this.session = session;
  }

  async runQuery(prompt: string): Promise<void> {
    this.startTime = Date.now();
    this.session.status = 'running';
    this.abortController = new AbortController();

    // Emit session start
    this.emit({
      type: 'session_start',
      sessionId: this.session.id,
      config: this.session.config,
    });

    // Emit user message
    this.emit({
      type: 'user_message',
      content: prompt,
      uuid: uuidv4(),
    });

    try {
      // Map our model names to SDK model names
      const modelMap: Record<string, string> = {
        'sonnet': 'claude-sonnet-4-20250514',
        'opus': 'claude-opus-4-20250514',
        'haiku': 'claude-haiku-3-20250514',
      };

      const model = modelMap[this.session.config.model || 'sonnet'] || 'claude-sonnet-4-20250514';

      // Build options for SDK
      const options: any = {
        model,
        cwd: this.session.config.cwd || process.cwd(),
        abortController: this.abortController,
        permissionMode: 'bypassPermissions' as const,
        allowDangerouslySkipPermissions: true,
      };

      if (this.session.config.systemPrompt) {
        options.systemPrompt = this.session.config.systemPrompt;
      }

      if (this.session.config.allowedTools && this.session.config.allowedTools.length > 0) {
        options.allowedTools = this.session.config.allowedTools;
      }

      if (this.session.config.maxTurns) {
        options.maxTurns = this.session.config.maxTurns;
      }

      // Run the agent query
      this.queryInstance = query({
        prompt,
        options,
      });

      for await (const message of this.queryInstance) {
        this.processMessage(message);
      }

      // If we completed normally
      this.complete('completed');
    } catch (error: any) {
      if (error.name === 'AbortError') {
        // Interrupted by user
        this.complete('interrupted');
      } else {
        // Real error
        console.error('[RealAgent] Error:', error);
        this.emit({
          type: 'error',
          message: error.message || 'Unknown error',
        });
        this.complete('error');
      }
    } finally {
      this.queryInstance = null;
    }
  }

  private processMessage(message: any): void {
    // Track usage from result messages
    if (message.type === 'result') {
      if (message.usage) {
        this.inputTokens = message.usage.inputTokens || 0;
        this.outputTokens = message.usage.outputTokens || 0;
      }
      if (message.total_cost_usd) {
        this.totalCostUsd = message.total_cost_usd;
      }
      return;
    }

    switch (message.type) {
      case 'assistant':
        this.processAssistantMessage(message);
        break;

      case 'user':
        // Tool results come as user messages
        this.processUserMessage(message);
        break;

      case 'system':
        // System messages (init, status, etc.)
        this.processSystemMessage(message);
        break;

      case 'tool_progress':
        // Tool is still running
        break;

      default:
        // Log unknown message types for debugging
        console.log(`[RealAgent] Message type: ${message.type}`);
    }
  }

  private processAssistantMessage(message: any): void {
    if (!message.message?.content) return;

    for (const block of message.message.content) {
      if (block.type === 'thinking') {
        // Extended thinking block
        this.emit({
          type: 'thinking',
          content: block.thinking || '',
        });
      } else if (block.type === 'text') {
        // Could be thinking-like or actual response
        const text = block.text || '';
        if (this.looksLikeThinking(text)) {
          this.emit({
            type: 'thinking',
            content: text,
          });
        } else {
          this.emit({
            type: 'assistant_message',
            content: text,
            uuid: message.uuid || uuidv4(),
          });
        }
      } else if (block.type === 'tool_use') {
        // Track tool name for later tool_end event
        this.activeTools.set(block.id, block.name);

        // Tool call starting
        this.emit({
          type: 'tool_start',
          tool: block.name,
          input: block.input,
          toolUseId: block.id,
        });
      }
    }
  }

  private processUserMessage(message: any): void {
    if (!message.message?.content) return;

    const content = message.message.content;
    if (!Array.isArray(content)) return;

    for (const block of content) {
      if (block.type === 'tool_result') {
        // Tool completed
        const toolUseId = block.tool_use_id || '';
        const toolName = this.activeTools.get(toolUseId) || 'unknown';
        this.activeTools.delete(toolUseId);

        const output = typeof block.content === 'string'
          ? block.content
          : JSON.stringify(block.content);

        this.emit({
          type: 'tool_end',
          tool: toolName,
          output: block.is_error ? { error: output } : { result: output.substring(0, 500) },
          toolUseId: toolUseId || uuidv4(),
          durationMs: 0, // SDK doesn't provide duration
        });
      }
    }
  }

  private processSystemMessage(message: any): void {
    if (message.subtype === 'init') {
      console.log(`[RealAgent] Initialized with model: ${message.model}`);
    }
  }

  private looksLikeThinking(text: string): boolean {
    // Heuristic: if it starts with common thinking patterns
    const thinkingPatterns = [
      /^(Let me|I'll|I need to|First|Now|Looking at|Analyzing|Checking)/i,
      /^(Hmm|Okay|Alright|So,)/i,
    ];
    return thinkingPatterns.some(p => p.test(text.trim()));
  }

  private complete(reason: string): void {
    this.session.status = reason === 'completed' ? 'completed' :
                          reason === 'interrupted' ? 'interrupted' : 'error';

    const usage: UsageStats = {
      inputTokens: this.inputTokens,
      outputTokens: this.outputTokens,
      totalCostUsd: this.totalCostUsd || this.estimateCost(),
      durationMs: Date.now() - this.startTime,
    };

    this.emit({
      type: 'session_end',
      reason,
      usage,
    });
  }

  private estimateCost(): number {
    // Rough cost estimation (sonnet pricing)
    const inputCost = (this.inputTokens / 1000000) * 3;  // $3 per 1M input
    const outputCost = (this.outputTokens / 1000000) * 15; // $15 per 1M output
    return inputCost + outputCost;
  }

  stop(): void {
    if (this.queryInstance) {
      this.queryInstance.close();
    }
  }

  interrupt(): void {
    if (this.queryInstance) {
      this.queryInstance.interrupt().catch(err => {
        console.error('[RealAgent] Interrupt error:', err);
      });
    }
  }

  private emit(event: AgentEvent): void {
    console.log(`[${this.session.id.slice(0, 8)}] Event: ${event.type}`);
    for (const listener of this.session.eventListeners) {
      try {
        listener(event);
      } catch (err) {
        console.error('Error in event listener:', err);
      }
    }
  }
}
