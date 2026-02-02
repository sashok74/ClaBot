# ClaBot — Claude Code Agent Orchestrator

## Техническое задание

**Версия:** 1.0
**Дата:** 2026-02-02

---

## 1. Обзор проекта

**ClaBot** — система визуального управления и мониторинга агентов Claude Code. Состоит из двух компонентов:

1. **Orchestrator** (TypeScript/Node.js) — серверная часть на базе Claude Agent SDK
2. **ClaBot UI** (C++ Builder/VCL) — десктопное приложение для визуального контроля

### 1.1 Цели

- Полный контроль над агентами Claude Code (tools, prompts, permissions)
- Визуальный мониторинг в реальном времени (tool calls, диалоги, статусы)
- Возможность создавать специализированных агентов с ограниченными возможностями
- Интеграция с существующими MCP серверами (ProjectMemory, DB_MCP)

### 1.2 Почему два компонента

| Компонент | Язык | Причина |
|-----------|------|---------|
| Orchestrator | TypeScript | Claude Agent SDK доступен только для TypeScript/Python. Полный доступ к hooks, events, tool control |
| UI | C++ Builder | Знакомый стек, нативный Windows UI, интеграция с существующими проектами |

---

## 2. Архитектура

```
┌─────────────────────────────────────────────────────────────────┐
│                    ClaBot UI (C++ Builder/VCL)                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Agent Config    │  Live Events     │  Response View      │  │
│  │  - System Prompt │  - Tool calls    │  - Agent output     │  │
│  │  - Allowed Tools │  - Results       │  - Formatted text   │  │
│  │  - Model select  │  - Errors        │                     │  │
│  └───────────────────────────────────────────────────────────┘  │
│                              │                                   │
│                     HTTP/SSE │ WebSocket                         │
└──────────────────────────────┼───────────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                 Orchestrator (TypeScript/Node.js)               │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  HTTP API Server (Express/Fastify)                        │  │
│  │    POST /agent/create    — создать сессию агента          │  │
│  │    POST /agent/query     — отправить промпт               │  │
│  │    GET  /agent/events    — SSE stream событий             │  │
│  │    POST /agent/interrupt — прервать выполнение            │  │
│  │    GET  /agent/status    — статус агента                  │  │
│  │    DELETE /agent/:id     — завершить сессию               │  │
│  └───────────────────────────────────────────────────────────┘  │
│                              │                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Claude Agent SDK                                         │  │
│  │    - query() с полным контролем                           │  │
│  │    - hooks: PreToolUse, PostToolUse, SessionStart/End     │  │
│  │    - canUseTool() для кастомных разрешений                │  │
│  │    - agents{} для определения субагентов                  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                              │                                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  MCP Connections                                          │  │
│  │    - ProjectMemory (localhost:8766)                       │  │
│  │    - DB_MCP (localhost:8765)                              │  │
│  │    - Custom MCP servers                                   │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Orchestrator (TypeScript)

### 3.1 Зависимости

```json
{
  "dependencies": {
    "@anthropic-ai/claude-agent-sdk": "^latest",
    "express": "^4.18",
    "uuid": "^9.0"
  },
  "devDependencies": {
    "typescript": "^5.0",
    "@types/node": "^20",
    "@types/express": "^4"
  }
}
```

### 3.2 Конфигурация агента (AgentConfig)

```typescript
interface AgentConfig {
  // Идентификация
  id: string;                          // UUID сессии
  name: string;                        // Человекочитаемое имя

  // Claude Agent SDK options
  systemPrompt: string;                // Системный промпт
  model: 'sonnet' | 'opus' | 'haiku'; // Модель
  allowedTools: string[];              // Разрешённые инструменты
  disallowedTools: string[];           // Запрещённые инструменты

  // Лимиты
  maxTurns?: number;                   // Макс. количество ходов
  maxBudgetUsd?: number;               // Макс. бюджет в USD
  maxThinkingTokens?: number;          // Макс. токенов на размышления

  // MCP
  mcpServers: Record<string, McpServerConfig>;

  // Permissions
  permissionMode: 'default' | 'acceptEdits' | 'bypassPermissions' | 'plan';

  // Working directory
  cwd: string;
}
```

### 3.3 События (AgentEvent)

Все события транслируются через SSE в UI:

```typescript
type AgentEvent =
  | { type: 'session_start'; sessionId: string; config: AgentConfig }
  | { type: 'tool_start'; tool: string; input: unknown; toolUseId: string }
  | { type: 'tool_end'; tool: string; output: unknown; toolUseId: string }
  | { type: 'tool_error'; tool: string; error: string; toolUseId: string }
  | { type: 'assistant_message'; content: string; uuid: string }
  | { type: 'user_message'; content: string; uuid: string }
  | { type: 'thinking'; content: string }
  | { type: 'permission_request'; tool: string; input: unknown }
  | { type: 'session_end'; reason: string; usage: UsageStats }
  | { type: 'error'; message: string };

interface UsageStats {
  inputTokens: number;
  outputTokens: number;
  totalCostUsd: number;
  durationMs: number;
}
```

### 3.4 HTTP API

#### POST /agent/create
Создать новую сессию агента.

**Request:**
```json
{
  "name": "Code Searcher",
  "systemPrompt": "Ты только ищешь код. Никаких изменений.",
  "model": "sonnet",
  "allowedTools": ["Read", "Glob", "Grep"],
  "cwd": "C:\\RADProjects\\MERP"
}
```

**Response:**
```json
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "status": "created"
}
```

#### POST /agent/query
Отправить промпт агенту.

**Request:**
```json
{
  "agentId": "550e8400-e29b-41d4-a716-446655440000",
  "prompt": "Найди все формы с TDataSet"
}
```

**Response:** Stream через SSE на /agent/events/{agentId}

#### GET /agent/events/{agentId}
SSE stream событий агента.

**Response:** Server-Sent Events
```
data: {"type":"tool_start","tool":"Glob","input":{"pattern":"**/*.dfm"}}

data: {"type":"tool_end","tool":"Glob","output":{"matches":["form1.dfm","form2.dfm"]}}

data: {"type":"assistant_message","content":"Найдено 2 формы..."}
```

#### POST /agent/interrupt
Прервать выполнение агента.

#### GET /agent/status/{agentId}
Получить текущий статус агента.

#### DELETE /agent/{agentId}
Завершить сессию агента.

### 3.5 Hooks Implementation

```typescript
const hooks = {
  PreToolUse: [{
    hooks: [async (input: PreToolUseHookInput) => {
      // Логируем и отправляем в UI
      sendEvent(sessionId, {
        type: 'tool_start',
        tool: input.tool_name,
        input: input.tool_input,
        toolUseId: toolUseId
      });

      // Можно модифицировать input или заблокировать
      return { continue: true };
    }]
  }],

  PostToolUse: [{
    hooks: [async (input: PostToolUseHookInput) => {
      sendEvent(sessionId, {
        type: 'tool_end',
        tool: input.tool_name,
        output: input.tool_response,
        toolUseId: toolUseId
      });
      return { continue: true };
    }]
  }],

  PostToolUseFailure: [{
    hooks: [async (input: PostToolUseFailureHookInput) => {
      sendEvent(sessionId, {
        type: 'tool_error',
        tool: input.tool_name,
        error: input.error,
        toolUseId: toolUseId
      });
      return { continue: true };
    }]
  }]
};
```

---

## 4. ClaBot UI (C++ Builder)

### 4.1 Главная форма (TfrmMain)

```
┌─────────────────────────────────────────────────────────────────────┐
│ ClaBot - Claude Code Agent Monitor                         [─][□][×]│
├─────────────────────────────────────────────────────────────────────┤
│ ┌─────────────────────┐ ┌─────────────────────────────────────────┐ │
│ │ Agent Configuration │ │ Events                                  │ │
│ │                     │ │ ┌─────────────────────────────────────┐ │ │
│ │ Name:               │ │ │ Time    │ Type  │ Tool  │ Status   │ │ │
│ │ [Code Searcher    ] │ │ ├─────────────────────────────────────┤ │ │
│ │                     │ │ │ 12:01:01│ tool  │ Glob  │ started  │ │ │
│ │ System Prompt:      │ │ │ 12:01:02│ tool  │ Glob  │ 15 files │ │ │
│ │ ┌─────────────────┐ │ │ │ 12:01:03│ tool  │ Read  │ started  │ │ │
│ │ │Ты только ищешь  │ │ │ │ 12:01:04│ tool  │ Read  │ 250 lines│ │ │
│ │ │код. Никаких     │ │ │ │ 12:01:05│ msg   │       │ response │ │ │
│ │ │изменений.       │ │ │ └─────────────────────────────────────┘ │ │
│ │ └─────────────────┘ │ │                                         │ │
│ │                     │ └─────────────────────────────────────────┘ │
│ │ Model: [Sonnet ▼]   │                                             │
│ │                     │ ┌─────────────────────────────────────────┐ │
│ │ Allowed Tools:      │ │ Response                                │ │
│ │ [x] Read            │ │                                         │ │
│ │ [x] Glob            │ │ Найдено 15 форм с TDataSet:             │ │
│ │ [x] Grep            │ │                                         │ │
│ │ [ ] Edit            │ │ 1. uCustomers.pas:45 - TClientDataSet   │ │
│ │ [ ] Write           │ │ 2. uOrders.pas:112 - TFDQuery           │ │
│ │ [ ] Bash            │ │ 3. uReports.pas:78 - TDataSet           │ │
│ │ [ ] Task            │ │ ...                                     │ │
│ │                     │ │                                         │ │
│ │ MCP Servers:        │ │                                         │ │
│ │ [x] ProjectMemory   │ │                                         │ │
│ │ [x] DB_MCP          │ │                                         │ │
│ │                     │ └─────────────────────────────────────────┘ │
│ │ [Create Agent]      │                                             │
│ └─────────────────────┘                                             │
│ ┌─────────────────────────────────────────────────────────────────┐ │
│ │ Prompt: [                                              ] [Send] │ │
│ └─────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────┤
│ Agent: Code Searcher │ Status: Running │ Tokens: 1,247 │ $0.003   │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 Компоненты формы

| Компонент | Тип | Назначение |
|-----------|-----|------------|
| `edtAgentName` | TEdit | Имя агента |
| `memoSystemPrompt` | TMemo | Системный промпт |
| `cmbModel` | TComboBox | Выбор модели (Sonnet/Opus/Haiku) |
| `clbTools` | TCheckListBox | Список разрешённых tools |
| `clbMcpServers` | TCheckListBox | Список MCP серверов |
| `lvEvents` | TListView | Таблица событий |
| `memoResponse` | TMemo | Ответ агента |
| `edtPrompt` | TEdit | Поле ввода промпта |
| `btnCreateAgent` | TButton | Создать агента |
| `btnSend` | TButton | Отправить промпт |
| `StatusBar` | TStatusBar | Статус, токены, стоимость |

### 4.3 HTTP/SSE клиент

```cpp
class TOrchestratorClient {
private:
    TIdHTTP* FHttp;
    TIdEventSource* FEventSource;  // или TThread для SSE
    UnicodeString FBaseUrl;
    UnicodeString FCurrentAgentId;

    TNotifyEvent FOnToolStart;
    TNotifyEvent FOnToolEnd;
    TNotifyEvent FOnMessage;
    TNotifyEvent FOnError;

public:
    __fastcall TOrchestratorClient(const UnicodeString& baseUrl);

    // API methods
    UnicodeString CreateAgent(TAgentConfig* config);
    void SendQuery(const UnicodeString& agentId, const UnicodeString& prompt);
    void Interrupt(const UnicodeString& agentId);
    TAgentStatus* GetStatus(const UnicodeString& agentId);
    void DeleteAgent(const UnicodeString& agentId);

    // SSE connection
    void ConnectEvents(const UnicodeString& agentId);
    void DisconnectEvents();

    // Events
    __property TNotifyEvent OnToolStart = {read=FOnToolStart, write=FOnToolStart};
    __property TNotifyEvent OnToolEnd = {read=FOnToolEnd, write=FOnToolEnd};
    __property TNotifyEvent OnMessage = {read=FOnMessage, write=FOnMessage};
    __property TNotifyEvent OnError = {read=FOnError, write=FOnError};
};
```

### 4.4 Обработка SSE событий

```cpp
void __fastcall TfrmMain::OnSSEEvent(TJSONObject* event) {
    UnicodeString type = event->GetValue<UnicodeString>("type");

    if (type == "tool_start") {
        // Добавить строку в ListView
        TListItem* item = lvEvents->Items->Add();
        item->Caption = Now().FormatString("hh:nn:ss");
        item->SubItems->Add("tool");
        item->SubItems->Add(event->GetValue<UnicodeString>("tool"));
        item->SubItems->Add("started");
        item->Data = (void*)event->Clone();
    }
    else if (type == "tool_end") {
        // Обновить строку в ListView
        UpdateEventItem(event->GetValue<UnicodeString>("toolUseId"),
                       "completed");
    }
    else if (type == "assistant_message") {
        // Добавить в memo ответа
        memoResponse->Lines->Add(event->GetValue<UnicodeString>("content"));
    }
    else if (type == "session_end") {
        // Обновить статус бар
        TJSONObject* usage = event->GetValue<TJSONObject*>("usage");
        StatusBar->Panels->Items[1]->Text = "Completed";
        StatusBar->Panels->Items[2]->Text =
            Format("Tokens: %d", ARRAYOFCONST((
                usage->GetValue<int>("inputTokens") +
                usage->GetValue<int>("outputTokens"))));
        StatusBar->Panels->Items[3]->Text =
            Format("$%.4f", ARRAYOFCONST((
                usage->GetValue<double>("totalCostUsd"))));
    }
}
```

---

## 5. Доступные инструменты Claude Code

Полный список tools для отображения в UI:

| Tool | Описание | Категория |
|------|----------|-----------|
| `Read` | Чтение файлов | Файлы |
| `Write` | Запись файлов | Файлы |
| `Edit` | Редактирование файлов | Файлы |
| `Glob` | Поиск файлов по паттерну | Поиск |
| `Grep` | Поиск по содержимому | Поиск |
| `Bash` | Выполнение команд | Команды |
| `Task` | Запуск субагентов | Агенты |
| `WebFetch` | Загрузка веб-страниц | Сеть |
| `WebSearch` | Поиск в интернете | Сеть |
| `AskUserQuestion` | Вопросы пользователю | UI |
| `NotebookEdit` | Редактирование Jupyter | Файлы |

### 5.1 Предустановленные профили агентов

```typescript
const agentPresets = {
  "searcher": {
    name: "Code Searcher",
    systemPrompt: "Ты агент-поисковик. Только ищи, не предлагай изменений.",
    allowedTools: ["Read", "Glob", "Grep"],
    model: "haiku"
  },
  "analyst": {
    name: "Code Analyst",
    systemPrompt: "Анализируй код и объясняй. Не меняй файлы.",
    allowedTools: ["Read", "Glob", "Grep", "WebSearch"],
    model: "sonnet"
  },
  "editor": {
    name: "Code Editor",
    systemPrompt: "Редактируй код по инструкциям. Минимальные изменения.",
    allowedTools: ["Read", "Glob", "Grep", "Edit", "Write"],
    model: "sonnet"
  },
  "full": {
    name: "Full Agent",
    systemPrompt: "Полный доступ к Claude Code.",
    allowedTools: [], // все tools
    model: "opus"
  }
};
```

---

## 6. MCP интеграция

### 6.1 Подключение существующих MCP серверов

```typescript
const mcpServers = {
  projectMemory: {
    type: "http",
    url: "http://localhost:8766/mcp",
    description: "Хранение знаний о MERP"
  },
  dbMcp: {
    type: "http",
    url: "http://localhost:8765/mcp",
    description: "Доступ к MSSQL базам"
  }
};
```

### 6.2 UI для управления MCP

В UI отображать:
- Статус подключения (connected/failed/pending)
- Список доступных tools от каждого MCP
- Возможность включить/выключить MCP для агента

---

## 7. Структура проекта

```
ClaBot/
├── doc/
│   └── spec.md                 # Это ТЗ
├── orchestrator/               # TypeScript часть
│   ├── package.json
│   ├── tsconfig.json
│   ├── src/
│   │   ├── index.ts           # Entry point
│   │   ├── server.ts          # HTTP/SSE server
│   │   ├── agent-manager.ts   # Управление агентами
│   │   ├── types.ts           # TypeScript типы
│   │   └── presets.ts         # Предустановленные профили
│   └── dist/                  # Compiled JS
├── ui/                        # C++ Builder часть
│   ├── ClaBot.cbproj
│   ├── ClaBot.cpp             # WinMain
│   ├── uMain.h
│   ├── uMain.cpp
│   ├── uMain.dfm
│   ├── uOrchestratorClient.h  # HTTP/SSE клиент
│   └── uOrchestratorClient.cpp
└── README.md
```

---

## 8. Этапы реализации

### Этап 1: Orchestrator MVP
- [ ] Настройка TypeScript проекта
- [ ] HTTP API (create, query, status)
- [ ] SSE streaming событий
- [ ] Базовая интеграция Claude Agent SDK
- [ ] Hooks для PreToolUse/PostToolUse

### Этап 2: UI MVP
- [ ] Создание VCL проекта
- [ ] Главная форма с базовыми компонентами
- [ ] HTTP клиент для API
- [ ] SSE клиент для событий
- [ ] Отображение событий в ListView

### Этап 3: Полная функциональность
- [ ] Все agent config options в UI
- [ ] Предустановленные профили агентов
- [ ] MCP серверы management
- [ ] Сохранение/загрузка конфигураций
- [ ] История сессий

### Этап 4: Расширенные возможности
- [ ] Множественные агенты одновременно
- [ ] Оркестрация (агент управляет агентами)
- [ ] Визуализация дерева субагентов
- [ ] Экспорт логов сессий

---

## 9. Требования к окружению

### Orchestrator
- Node.js 18+
- npm или yarn
- Anthropic API key (или Claude Pro subscription)

### UI
- RAD Studio 12+ (C++ Builder)
- Windows 10/11
- Indy components (встроены в RAD Studio)

### Сеть
- Orchestrator слушает на localhost:3000
- MCP серверы на localhost:8765, 8766

---

## 10. Ссылки

- [Claude Agent SDK TypeScript](https://platform.claude.com/docs/en/agent-sdk/typescript)
- [Claude Agent SDK Overview](https://platform.claude.com/docs/en/agent-sdk/overview)
- [MCP Protocol](https://modelcontextprotocol.io/)
- [ProjectMemory](../ProjectMemory/doc/spec.md)
