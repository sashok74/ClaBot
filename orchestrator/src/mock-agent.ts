import { v4 as uuidv4 } from 'uuid';
import { AgentEvent, AgentSession, UsageStats } from './types';

// Mock event template type
interface MockEventTemplate {
  type: string;
  content?: string;
  tool?: string;
  input?: unknown;
  output?: unknown;
  durationMs?: number;
}

// Mock events sequence for demonstration
const MOCK_SEQUENCE: MockEventTemplate[] = [
  { type: 'thinking', content: 'Анализирую запрос пользователя...' },
  { type: 'thinking', content: 'Определяю необходимые инструменты...' },
  { type: 'tool_start', tool: 'Glob', input: { pattern: '**/*.ts' } },
  { type: 'tool_end', tool: 'Glob', output: { files: ['index.ts', 'server.ts', 'types.ts'] }, durationMs: 150 },
  { type: 'thinking', content: 'Найдено 3 TypeScript файла. Читаю содержимое...' },
  { type: 'tool_start', tool: 'Read', input: { file_path: 'index.ts' } },
  { type: 'tool_end', tool: 'Read', output: { lines: 25, content: '// Entry point...' }, durationMs: 50 },
  { type: 'tool_start', tool: 'Grep', input: { pattern: 'export', path: '.' } },
  { type: 'tool_end', tool: 'Grep', output: { matches: 12 }, durationMs: 200 },
  { type: 'assistant_message', content: 'Анализ завершён. Найдено 3 TypeScript файла с 12 экспортами.' },
];

export class MockAgent {
  private session: AgentSession;
  private intervalId: NodeJS.Timeout | null = null;
  private eventIndex = 0;
  private startTime: number = 0;

  constructor(session: AgentSession) {
    this.session = session;
  }

  // Start generating mock events
  async runQuery(prompt: string, resume: boolean = false): Promise<void> {
    this.eventIndex = 0;
    this.startTime = Date.now();
    this.session.status = 'running';

    // Send session start
    this.emit({
      type: 'session_start',
      sessionId: this.session.id,
      config: this.session.config,
    });

    // Generate mock SDK session ID (simulates what real SDK does)
    if (!this.session.sdkSessionId) {
      const mockSdkSessionId = `mock-session-${uuidv4()}`;
      this.session.sdkSessionId = mockSdkSessionId;
      this.session.canResume = true;
      console.log(`[MockAgent] Session ID captured: ${mockSdkSessionId}`);
      this.emit({
        type: 'session_info',
        sdkSessionId: mockSdkSessionId,
        canResume: true,
      });
    } else if (resume) {
      console.log(`[MockAgent] Resuming session: ${this.session.sdkSessionId}`);
    }

    // Send user message
    this.emit({
      type: 'user_message',
      content: prompt,
      uuid: uuidv4(),
    });

    // Start mock event sequence
    this.intervalId = setInterval(() => {
      this.nextEvent();
    }, 500);
  }

  // Generate next mock event
  private nextEvent(): void {
    if (this.eventIndex >= MOCK_SEQUENCE.length) {
      this.complete();
      return;
    }

    const template = MOCK_SEQUENCE[this.eventIndex];
    let event: AgentEvent;

    switch (template.type) {
      case 'thinking':
        event = { type: 'thinking', content: template.content || '' };
        break;

      case 'tool_start':
        event = {
          type: 'tool_start',
          tool: template.tool || 'unknown',
          input: template.input,
          toolUseId: uuidv4(),
        };
        break;

      case 'tool_end':
        event = {
          type: 'tool_end',
          tool: template.tool || 'unknown',
          input: template.input,
          output: template.output,
          toolUseId: uuidv4(),
          durationMs: template.durationMs || 0,
        };
        break;

      case 'assistant_message':
        event = {
          type: 'assistant_message',
          content: template.content || '',
          uuid: uuidv4(),
        };
        break;

      default:
        this.eventIndex++;
        return;
    }

    this.emit(event);
    this.eventIndex++;
  }

  // Complete the mock session
  private complete(): void {
    this.stop();
    this.session.status = 'completed';

    const usage: UsageStats = {
      inputTokens: 150 + Math.floor(Math.random() * 100),
      outputTokens: 80 + Math.floor(Math.random() * 50),
      totalCostUsd: 0.001 + Math.random() * 0.002,
      durationMs: Date.now() - this.startTime,
    };

    this.emit({
      type: 'session_end',
      reason: 'completed',
      usage,
    });
  }

  // Stop the mock agent
  stop(): void {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
  }

  // Interrupt the mock agent
  interrupt(): void {
    this.stop();
    this.session.status = 'interrupted';

    const usage: UsageStats = {
      inputTokens: 50 + Math.floor(Math.random() * 50),
      outputTokens: 20 + Math.floor(Math.random() * 20),
      totalCostUsd: 0.0005 + Math.random() * 0.001,
      durationMs: Date.now() - this.startTime,
    };

    this.emit({
      type: 'session_end',
      reason: 'interrupted',
      usage,
    });
  }

  // Emit event to all listeners
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
