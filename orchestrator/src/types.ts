// Agent Configuration
export interface AgentConfig {
  id: string;
  name: string;
  systemPrompt?: string;
  model?: 'sonnet' | 'opus' | 'haiku';
  allowedTools?: string[];
  disallowedTools?: string[];
  maxTurns?: number;
  maxBudgetUsd?: number;
  cwd?: string;
}

// Agent Status
export type AgentStatus = 'created' | 'idle' | 'running' | 'interrupted' | 'completed' | 'error';

// Agent Session
export interface AgentSession {
  id: string;
  config: AgentConfig;
  status: AgentStatus;
  createdAt: Date;
  eventListeners: Set<(event: AgentEvent) => void>;
  // Session tracking for resume support
  sdkSessionId?: string;
  canResume: boolean;
  inputTokens: number;
  outputTokens: number;
  totalCostUsd: number;
}

// SSE Events
export type AgentEvent =
  | { type: 'session_start'; sessionId: string; config: AgentConfig }
  | { type: 'session_info'; sdkSessionId: string; canResume: boolean }
  | { type: 'thinking'; content: string }
  | { type: 'tool_start'; tool: string; input: unknown; toolUseId: string }
  | { type: 'tool_end'; tool: string; input: unknown; output: unknown; toolUseId: string; durationMs: number }
  | { type: 'tool_error'; tool: string; error: string; toolUseId: string }
  | { type: 'assistant_message'; content: string; uuid: string }
  | { type: 'user_message'; content: string; uuid: string }
  | { type: 'permission_request'; tool: string; input: unknown; requestId: string }
  | { type: 'session_end'; reason: string; usage: UsageStats }
  | { type: 'error'; message: string };

// Usage Statistics
export interface UsageStats {
  inputTokens: number;
  outputTokens: number;
  totalCostUsd: number;
  durationMs: number;
}

// API Request/Response types
export interface CreateAgentRequest {
  name: string;
  systemPrompt?: string;
  model?: 'sonnet' | 'opus' | 'haiku';
  allowedTools?: string[];
  cwd?: string;
}

export interface CreateAgentResponse {
  id: string;
  status: AgentStatus;
}

export interface QueryRequest {
  prompt: string;
  resume?: boolean;  // Continue existing session instead of starting new
}

export interface QueryResponse {
  status: 'processing' | 'error';
  message?: string;
}

export interface StatusResponse {
  id: string;
  status: AgentStatus;
  config: AgentConfig;
}

export interface SessionResponse {
  id: string;
  sdkSessionId?: string;
  canResume: boolean;
  inputTokens: number;
  outputTokens: number;
  totalCostUsd: number;
}
