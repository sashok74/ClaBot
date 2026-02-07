import { query, Query } from '@anthropic-ai/claude-agent-sdk';
import { v4 as uuidv4 } from 'uuid';
import { AgentSession } from '../types/domain';
import { EventBus } from '../services/event-bus';
import { ToolTracker } from '../services/tool-tracker';
import { BaseAgent } from './base-agent';

export class RealAgent extends BaseAgent {
  private abortController: AbortController | null = null;
  private queryInstance: Query | null = null;
  private toolTracker = new ToolTracker();

  constructor(session: AgentSession, eventBus: EventBus) {
    super(session, eventBus);
  }

  async runQuery(prompt: string, resume: boolean = false): Promise<void> {
    this.usageTracker.start();
    this.session.status = 'running';
    this.abortController = new AbortController();

    this.emitSessionStart();
    this.emitUserMessage(prompt);

    try {
      const modelMap: Record<string, string> = {
        'sonnet': 'claude-sonnet-4-20250514',
        'opus': 'claude-opus-4-20250514',
        'haiku': 'claude-haiku-3-20250514',
      };

      const model = modelMap[this.session.config.model || 'sonnet'] || 'claude-sonnet-4-20250514';

      const permMode = this.session.config.permissionMode || 'bypassPermissions';

      const options: Record<string, unknown> = {
        model,
        cwd: this.session.config.cwd || process.cwd(),
        abortController: this.abortController,
        permissionMode: permMode,
      };

      if (permMode === 'bypassPermissions') {
        options.allowDangerouslySkipPermissions = true;
      }

      if (this.session.config.systemPrompt) {
        options.systemPrompt = this.session.config.systemPrompt;
      }

      if (this.session.config.allowedTools && this.session.config.allowedTools.length > 0) {
        options.allowedTools = this.session.config.allowedTools;
      }

      if (this.session.config.disallowedTools && this.session.config.disallowedTools.length > 0) {
        options.disallowedTools = this.session.config.disallowedTools;
      }

      if (this.session.config.maxTurns) {
        options.maxTurns = this.session.config.maxTurns;
      }

      if (this.session.config.maxBudgetUsd) {
        options.maxBudgetUsd = this.session.config.maxBudgetUsd;
      }

      if (this.session.config.mcpServers) {
        options.mcpServers = this.session.config.mcpServers;
      }

      if (resume && this.session.sdkSessionId) {
        options.resume = this.session.sdkSessionId;
        console.log(`[RealAgent] Resuming session: ${this.session.sdkSessionId}`);
      }

      this.queryInstance = query({
        prompt,
        options,
      });

      for await (const message of this.queryInstance) {
        this.processMessage(message);
      }

      this.complete('completed');
    } catch (error: any) {
      if (error.name === 'AbortError') {
        this.complete('interrupted');
      } else {
        console.error('[RealAgent] Error:', error);
        this.emit({
          type: 'error',
          message: error.message || 'Unknown error',
        });
        this.complete('error');
      }
    } finally {
      this.queryInstance = null;
      this.toolTracker.cleanup();
    }
  }

  private processMessage(message: any): void {
    console.log(`[RealAgent] Message: type=${message.type}, subtype=${message.subtype || '-'}, has_session_id=${!!message.session_id}`);

    // Capture session_id from the FIRST message that has it
    if (message.session_id && !this.session.sdkSessionId) {
      this.emitSessionInfo(message.session_id);
    }

    // Track usage from result messages
    if (message.type === 'result') {
      if (message.usage) {
        this.usageTracker.update(
          message.usage.inputTokens || 0,
          message.usage.outputTokens || 0,
        );
      }
      if (message.total_cost_usd) {
        this.usageTracker.totalCostUsd = message.total_cost_usd;
      }
      this.syncUsageToSession();
      return;
    }

    switch (message.type) {
      case 'assistant':
        this.processAssistantMessage(message);
        break;
      case 'user':
        this.processUserMessage(message);
        break;
      case 'system':
        this.processSystemMessage(message);
        break;
      case 'tool_progress':
        break;
      default:
        console.log(`[RealAgent] Message type: ${message.type}`);
    }
  }

  private processAssistantMessage(message: any): void {
    if (!message.message?.content) return;

    for (const block of message.message.content) {
      if (block.type === 'thinking') {
        this.emit({
          type: 'thinking',
          content: block.thinking || '',
        });
      } else if (block.type === 'text') {
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
        this.toolTracker.trackStart(block.id, block.name, block.input);
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
        const toolUseId = block.tool_use_id || '';
        const { toolName, input: toolInput, durationMs } = this.toolTracker.resolve(toolUseId);

        const output = typeof block.content === 'string'
          ? block.content
          : JSON.stringify(block.content);

        this.emit({
          type: 'tool_end',
          tool: toolName,
          input: toolInput,
          output: block.is_error ? { error: output } : { result: output },
          toolUseId: toolUseId || uuidv4(),
          durationMs,
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
    const thinkingPatterns = [
      /^(Let me|I'll|I need to|First|Now|Looking at|Analyzing|Checking)/i,
      /^(Hmm|Okay|Alright|So,)/i,
    ];
    return thinkingPatterns.some(p => p.test(text.trim()));
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
}
