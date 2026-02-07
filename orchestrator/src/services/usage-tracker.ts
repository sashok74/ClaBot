import { UsageStats } from '../types/domain';

export class UsageTracker {
  inputTokens = 0;
  outputTokens = 0;
  totalCostUsd = 0;
  private startTime = 0;

  start(): void {
    this.startTime = Date.now();
    this.inputTokens = 0;
    this.outputTokens = 0;
    this.totalCostUsd = 0;
  }

  update(input: number, output: number, cost?: number): void {
    this.inputTokens = input;
    this.outputTokens = output;
    if (cost !== undefined) {
      this.totalCostUsd = cost;
    }
  }

  getStats(): UsageStats {
    return {
      inputTokens: this.inputTokens,
      outputTokens: this.outputTokens,
      totalCostUsd: this.totalCostUsd || this.estimateCost(),
      durationMs: Date.now() - this.startTime,
    };
  }

  private estimateCost(): number {
    const inputCost = (this.inputTokens / 1000000) * 3;
    const outputCost = (this.outputTokens / 1000000) * 15;
    return inputCost + outputCost;
  }
}
