import { loadConfig } from './config';
import { createApp } from './server';

const config = loadConfig();
const app = createApp();

app.listen(config.port, () => {
  console.log('========================================');
  console.log('  ClaBot Orchestrator');
  console.log('========================================');
  console.log(`  Server running on http://localhost:${config.port}`);
  console.log('');
  console.log('  Endpoints:');
  console.log('    POST   /agent/create       - Create agent');
  console.log('    GET    /agent/:id/events   - SSE events');
  console.log('    POST   /agent/:id/query    - Send prompt');
  console.log('    GET    /agent/:id/status   - Get status');
  console.log('    POST   /agent/:id/interrupt- Interrupt');
  console.log('    DELETE /agent/:id          - Delete');
  console.log('    GET    /agents             - List all');
  console.log('    GET    /health             - Health check');
  console.log('========================================');
});
