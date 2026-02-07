# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ClaBot — демонстрационное приложение для работы с Claude Code SDK через визуальный интерфейс RAD Studio C++ Builder.

Два компонента:
- **Orchestrator** (TypeScript/Node.js) — сервер-обёртка над Claude Agent SDK (SDK доступен только для TS/Python)
- **ClaBot UI** (C++ Builder/VCL) — визуальный интерфейс для управления агентами

Демонстрирует:
- Создание и настройку агентов (system prompt, модель, разрешённые tools)
- Мониторинг tool calls в реальном времени через SSE
- Интеграцию с MCP серверами
- Контроль permissions и лимитов агента

## Architecture

```
ClaBot UI (C++ Builder) <--HTTP/SSE--> Orchestrator (TypeScript) <--> Claude Agent SDK
                                              |
                                              +--> MCP Servers (ProjectMemory:8766, DB_MCP:8765)
```

## Project Structure

```
ClaBot/
├── orchestrator/           # TypeScript server
│   └── src/
│       ├── index.ts        # Entry point
│       ├── server.ts       # HTTP/SSE server (Express)
│       ├── agent-manager.ts
│       ├── types.ts
│       └── presets.ts      # Agent profile presets
├── ui/                     # C++ Builder VCL app
│   ├── ClaBot.cbproj
│   ├── uMain.{h,cpp,dfm}   # Main form
│   └── uOrchestratorClient.{h,cpp}  # HTTP/SSE client
└── doc/spec.md             # Full specification
```

## Build Commands

### Orchestrator
```bash
cd orchestrator
npm install
npm run build          # Compile TypeScript
npm run start          # Run server (listens on localhost:3000)
```

### UI (C++ Builder)
Open `ui/ClaBot.cbproj` in RAD Studio 12+ and build from IDE.

## Running (Порядок запуска)

### 1. Запуск Orchestrator (фоновый режим)

**Windows:** Claude Agent SDK запускает Claude Code CLI как subprocess, который требует git-bash. Перед запуском необходимо задать переменную окружения:

```bash
export CLAUDE_CODE_GIT_BASH_PATH="C:\Tools\Git\bin\bash.exe"
cd C:/RADProjects/ClaBot/orchestrator && npm run start
```

> Путь к `bash.exe` может отличаться. Типичные варианты:
> - `C:\Program Files\Git\bin\bash.exe`
> - `C:\Tools\Git\bin\bash.exe`

Сервер слушает на `localhost:3000`. Проверка: `curl http://localhost:3000/health`

### 2. Запуск UI
```bash
"C:\RADProjects\ClaBot\ui\Win64x\Debug\ClaBot.exe" &
```
MCP сервер UI слушает на `localhost:8767`.

### Остановка

```bash
# Остановить UI
powershell -Command "Stop-Process -Name ClaBot -Force -ErrorAction SilentlyContinue"

# Остановить Orchestrator (если запущен в foreground - Ctrl+C)
# Если в background:
powershell -Command "Stop-Process -Name node -Force -ErrorAction SilentlyContinue"
```

## UI MCP Server (порт 8767)

UI содержит встроенный MCP сервер для автоматизации и тестирования.

### Доступные инструменты

| Tool | Параметры | Описание |
|------|-----------|----------|
| `ui_get_status` | — | Статус: connected, agentId, eventsCount, statusText |
| `ui_get_events` | limit, offset | Список событий |
| `ui_get_controls` | — | Состояние контролов и кнопок |
| `ui_get_all` | events_limit | Полное состояние UI (поля, statusBar, кнопки, события) |
| `ui_click_connect` | — | Нажать Connect |
| `ui_click_create_agent` | — | Нажать Create Agent |
| `ui_click_send` | — | Нажать Send |
| `ui_click_stop` | — | Нажать Stop |
| `ui_set_server_url` | url | Установить URL сервера |
| `ui_set_agent_name` | name | Установить имя агента |
| `ui_set_prompt` | text | Установить текст промпта |
| `ui_wait_events` | count, timeout_ms | Ждать N событий |

### Формат вызова MCP

```bash
curl -s -X POST http://localhost:8767/mcp \
  -H "Content-Type: application/json" \
  -H "Accept: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"TOOL_NAME","arguments":{...}},"id":1}'
```

### Типичный workflow тестирования UI

```bash
# 1. Подключиться к оркестратору
curl -s -X POST http://localhost:8767/mcp -H "Content-Type: application/json" -H "Accept: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"ui_click_connect"},"id":1}'

# 2. Создать агента
curl -s -X POST http://localhost:8767/mcp -H "Content-Type: application/json" -H "Accept: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"ui_click_create_agent"},"id":2}'

# 3. Установить промпт
curl -s -X POST http://localhost:8767/mcp -H "Content-Type: application/json" -H "Accept: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"ui_set_prompt","arguments":{"text":"Hello"}},"id":3}'

# 4. Отправить
curl -s -X POST http://localhost:8767/mcp -H "Content-Type: application/json" -H "Accept: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"ui_click_send"},"id":4}'

# 5. Дождаться событий
curl -s -X POST http://localhost:8767/mcp -H "Content-Type: application/json" -H "Accept: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"ui_wait_events","arguments":{"count":10,"timeout_ms":15000}},"id":5}'

# 6. Получить полное состояние
curl -s -X POST http://localhost:8767/mcp -H "Content-Type: application/json" -H "Accept: application/json" \
  -d '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"ui_get_all","arguments":{"events_limit":20}},"id":6}'
```

### Пример ответа ui_get_all

```json
{
  "fields": {
    "serverUrl": "http://localhost:3000",
    "agentName": "Test Agent",
    "agentId": "abc123...",
    "prompt": ""
  },
  "statusBar": ["Completed", "Agent: abc123", "Events: 14"],
  "buttons": {
    "connect": false,
    "createAgent": true,
    "send": true,
    "stop": false
  },
  "state": {
    "connected": true,
    "eventsCount": 14
  },
  "recentEvents": [
    {"time": "12:00:01", "type": "session_start", "data": "Session started"},
    {"time": "12:00:02", "type": "thinking", "data": "Анализирую запрос..."},
    ...
  ]
}
```

## Key APIs

### HTTP Endpoints (Orchestrator)
- `POST /agent/create` - Create agent session with config
- `POST /agent/query` - Send prompt to agent
- `GET /agent/events/{agentId}` - SSE stream for real-time events
- `POST /agent/interrupt` - Stop agent execution
- `GET /agent/status/{agentId}` - Get agent status
- `DELETE /agent/{agentId}` - End session

### Event Types (SSE)
`tool_start`, `tool_end`, `tool_error`, `assistant_message`, `user_message`, `thinking`, `permission_request`, `session_start`, `session_end`, `error`

## Agent Configuration

Key fields in AgentConfig:
- `systemPrompt`, `model` (sonnet/opus/haiku), `allowedTools`, `disallowedTools`
- `maxTurns`, `maxBudgetUsd`, `maxThinkingTokens`
- `mcpServers`, `permissionMode`, `cwd`

## Available Tool Categories

For the agent tool selector UI:
- **Files**: Read, Write, Edit, NotebookEdit
- **Search**: Glob, Grep
- **Commands**: Bash
- **Agents**: Task
- **Network**: WebFetch, WebSearch
- **UI**: AskUserQuestion

## Requirements

- Orchestrator: Node.js 18+, Anthropic API key
- UI: RAD Studio 12+ (C++ Builder), Windows 10/11

## C++ Builder Compiler Limitations (bcc64x)

**Нельзя смешивать C++ try/catch и SEH __try/__finally в одной функции.**

```cpp
// WRONG - compiler error
void foo() {
    try {
        // ...
    }
    __finally {  // Error: cannot use C++ 'try' with SEH '__try'
        // ...
    }
}

// CORRECT - use RAII or explicit cleanup
void foo() {
    std::unique_ptr<Resource> res(new Resource());
    try {
        // ...
    }
    catch (...) {
        // handle error
    }
    // cleanup happens automatically via RAII
}
```
