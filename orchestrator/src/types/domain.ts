// Permission modes for agent execution
export type PermissionMode = 'default' | 'plan' | 'bypassPermissions';

// MCP server configuration
export interface McpServerConfig {
  url: string;
  [key: string]: unknown;
}

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
  permissionMode?: PermissionMode;
  mcpServers?: Record<string, McpServerConfig>;
  cwd?: string;
}

// Agent Status
export type AgentStatus = 'created' | 'idle' | 'running' | 'interrupted' | 'completed' | 'error';

// Agent Session (no eventListeners — moved to EventBus)
export interface AgentSession {
  id: string;
  config: AgentConfig;
  status: AgentStatus;
  createdAt: Date;
  // Session tracking for resume support
  sdkSessionId?: string;
  canResume: boolean;
  inputTokens: number;
  outputTokens: number;
  totalCostUsd: number;
}

// SSE Events
export type AgentEvent =
  | { type: 'connected'; agentId: string }
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
