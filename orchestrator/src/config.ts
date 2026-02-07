import 'dotenv/config';

export const MAX_SESSIONS = 100;

export interface AppConfig {
  port: number;
}

export function loadConfig(): AppConfig {
  return {
    port: parseInt(process.env.PORT || '3000', 10),
  };
}
