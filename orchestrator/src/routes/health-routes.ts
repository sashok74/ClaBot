import { Router, Request, Response } from 'express';

export function registerHealthRoutes(router: Router): void {
  router.get('/health', (_req: Request, res: Response) => {
    res.json({
      status: 'ok',
      timestamp: new Date().toISOString(),
    });
  });
}
