const ORPHAN_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes

export class ToolTracker {
  private activeTools = new Map<string, string>();
  private activeToolInputs = new Map<string, unknown>();
  private toolStartTimes = new Map<string, number>();

  trackStart(toolUseId: string, toolName: string, input: unknown): void {
    this.activeTools.set(toolUseId, toolName);
    this.activeToolInputs.set(toolUseId, input);
    this.toolStartTimes.set(toolUseId, Date.now());
  }

  resolve(toolUseId: string): { toolName: string; input: unknown; durationMs: number } {
    const toolName = this.activeTools.get(toolUseId) || 'unknown';
    const input = this.activeToolInputs.get(toolUseId);
    const startTime = this.toolStartTimes.get(toolUseId) || Date.now();
    const durationMs = Date.now() - startTime;

    this.activeTools.delete(toolUseId);
    this.activeToolInputs.delete(toolUseId);
    this.toolStartTimes.delete(toolUseId);

    return { toolName, input, durationMs };
  }

  cleanup(): void {
    const now = Date.now();
    for (const [id, startTime] of this.toolStartTimes) {
      if (now - startTime > ORPHAN_TIMEOUT_MS) {
        this.activeTools.delete(id);
        this.activeToolInputs.delete(id);
        this.toolStartTimes.delete(id);
      }
    }
  }
}
