import { AgentStatus, AgentConfig, PermissionMode, McpServerConfig } from './domain';

// API Request/Response types
export interface CreateAgentRequest {
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
