# ClaBot — Архитектура системы

## Общая схема с точками контроля

```mermaid
flowchart TB
    subgraph UI["🖥️ ClaBot UI (C++ Builder / VCL)"]
        direction TB

        subgraph ConfigPanel["Agent Configuration"]
            CP1["🎛️ CONTROL POINT 1<br/>Начальная конфигурация"]
            Config["System Prompt<br/>Model (sonnet/opus/haiku)<br/>Allowed Tools []<br/>MCP Servers []<br/>Limits (turns, budget)"]
        end

        EventsView["📋 Events Monitor<br/>ListView tool calls"]
        ResponseView["💬 Response View"]

        subgraph Controls["Runtime Controls"]
            CP2["🛑 CONTROL POINT 2<br/>Interrupt Button"]
            PromptInput["Prompt Input"]
            SendBtn["Send"]
            StopBtn["Stop"]
        end

        StatusBar["Status: tokens, cost, duration"]
    end

    UI -->|"HTTP POST /agent/create"| Server
    UI -->|"HTTP POST /agent/query"| Server
    UI <-->|"SSE /agent/events/:id"| Server
    UI -->|"HTTP POST /agent/interrupt"| Server

    subgraph Orchestrator["⚙️ Orchestrator (TypeScript / Node.js)"]
        direction TB

        subgraph Server["Express HTTP Server :3000"]
            Routes["REST API Endpoints"]
            SSE["SSE Event Stream"]
        end

        subgraph AgentMgr["AgentManager"]
            Sessions["Map&lt;sessionId, AgentSession&gt;"]
        end

        subgraph Hooks["🔧 SDK Hooks"]
            CP3["⚡ CONTROL POINT 3<br/>PreToolUse Hook<br/><i>block / modify / allow</i>"]
            CP4["📊 CONTROL POINT 4<br/>PostToolUse Hook<br/><i>log / analyze / alert</i>"]
            CP5["🔐 CONTROL POINT 5<br/>canUseTool()<br/><i>custom permission logic</i>"]
        end

        subgraph SDKConfig["SDK Configuration"]
            CP6["📏 CONTROL POINT 6<br/>Runtime Limits<br/><i>maxTurns, maxBudgetUsd</i>"]
        end

        Server --> AgentMgr
        AgentMgr --> Hooks
        Hooks --> SDKConfig
    end

    subgraph External["External Services"]
        Claude["☁️ Claude API<br/>(Anthropic)"]
        MCP1["🗄️ ProjectMemory<br/>:8766"]
        MCP2["🗄️ DB_MCP<br/>:8765"]
    end

    SDKConfig --> Claude
    SDKConfig --> MCP1
    SDKConfig --> MCP2

    style CP1 fill:#ff9800,stroke:#e65100,color:#000
    style CP2 fill:#f44336,stroke:#b71c1c,color:#fff
    style CP3 fill:#4caf50,stroke:#1b5e20,color:#fff
    style CP4 fill:#2196f3,stroke:#0d47a1,color:#fff
    style CP5 fill:#9c27b0,stroke:#4a148c,color:#fff
    style CP6 fill:#607d8b,stroke:#263238,color:#fff
```

---

## Точки контроля (детально)

```mermaid
flowchart LR
    subgraph CP["🎯 Control Points"]
        direction TB

        C1["🎛️ CP1: Initial Config"]
        C2["🛑 CP2: Interrupt"]
        C3["⚡ CP3: PreToolUse"]
        C4["📊 CP4: PostToolUse"]
        C5["🔐 CP5: canUseTool"]
        C6["📏 CP6: Limits"]
    end

    subgraph Actions1["CP1 Actions"]
        A1a["Выбор модели"]
        A1b["System prompt"]
        A1c["Whitelist tools"]
        A1d["Blacklist tools"]
        A1e["Подключение MCP"]
        A1f["Permission mode"]
    end

    subgraph Actions2["CP2 Actions"]
        A2a["Остановить агента"]
        A2b["Abort текущий tool"]
    end

    subgraph Actions3["CP3 Actions"]
        A3a["✅ Разрешить tool"]
        A3b["❌ Заблокировать tool"]
        A3c["✏️ Модифицировать input"]
        A3d["⏸️ Запросить подтверждение у UI"]
    end

    subgraph Actions4["CP4 Actions"]
        A4a["📝 Логировать результат"]
        A4b["🔍 Анализ output"]
        A4c["⚠️ Alert при anomaly"]
        A4d["📊 Собирать статистику"]
    end

    subgraph Actions5["CP5 Actions"]
        A5a["Кастомная логика разрешений"]
        A5b["Проверка по контексту"]
        A5c["Rate limiting по tool"]
    end

    subgraph Actions6["CP6 Actions"]
        A6a["Лимит ходов (maxTurns)"]
        A6b["Лимит бюджета ($)"]
        A6c["Лимит thinking tokens"]
    end

    C1 --> Actions1
    C2 --> Actions2
    C3 --> Actions3
    C4 --> Actions4
    C5 --> Actions5
    C6 --> Actions6

    style C1 fill:#ff9800,stroke:#e65100,color:#000
    style C2 fill:#f44336,stroke:#b71c1c,color:#fff
    style C3 fill:#4caf50,stroke:#1b5e20,color:#fff
    style C4 fill:#2196f3,stroke:#0d47a1,color:#fff
    style C5 fill:#9c27b0,stroke:#4a148c,color:#fff
    style C6 fill:#607d8b,stroke:#263238,color:#fff
```

---

## Поток выполнения с точками контроля

```mermaid
sequenceDiagram
    participant UI as 🖥️ UI
    participant Orch as ⚙️ Orchestrator
    participant SDK as 📦 Claude SDK
    participant API as ☁️ Claude API

    Note over UI: 🎛️ CP1: User configures agent
    UI->>Orch: POST /agent/create<br/>{tools, model, prompt, limits}
    Orch->>SDK: new Session(config)
    Orch-->>UI: {sessionId}

    UI->>Orch: GET /agent/events/:id (SSE)
    UI->>Orch: POST /agent/query {prompt}
    Orch->>SDK: claude.query(prompt)
    SDK->>API: API Request
    API-->>SDK: Response + Tool Call

    rect rgb(200, 230, 200)
        Note over Orch,SDK: ⚡ CP3: PreToolUse Hook
        SDK->>Orch: PreToolUse(tool, input)
        alt Block tool
            Orch-->>SDK: {block: true, message: "..."}
        else Modify input
            Orch-->>SDK: {modifiedInput: {...}}
        else Request permission
            Orch-->>UI: SSE: permission_request
            UI-->>Orch: User decision
            Orch-->>SDK: {continue: true/false}
        else Allow
            Orch-->>SDK: {continue: true}
        end
    end

    Orch-->>UI: SSE: tool_start
    SDK->>SDK: Execute Tool

    rect rgb(200, 200, 240)
        Note over Orch,SDK: 📊 CP4: PostToolUse Hook
        SDK->>Orch: PostToolUse(tool, output)
        Orch->>Orch: Log, analyze, collect stats
        Orch-->>SDK: {continue: true}
    end

    Orch-->>UI: SSE: tool_end

    rect rgb(240, 200, 200)
        Note over UI: 🛑 CP2: User can interrupt anytime
        UI->>Orch: POST /agent/interrupt
        Orch->>SDK: session.abort()
        Orch-->>UI: SSE: session_end {reason: interrupted}
    end

    rect rgb(220, 220, 220)
        Note over SDK: 📏 CP6: Auto-stop on limits
        SDK->>SDK: Check maxTurns, maxBudget
        SDK-->>Orch: Session ended (limit reached)
        Orch-->>UI: SSE: session_end {reason: limit}
    end
```

---

## Thinking Process (Extended Thinking)

**Thinking происходит на серверах Anthropic — это "чёрный ящик".**

```mermaid
sequenceDiagram
    participant UI as 🖥️ UI
    participant Orch as ⚙️ Orchestrator
    participant SDK as 📦 SDK
    participant API as ☁️ Claude API<br/>(Anthropic)

    Note over UI: 🎛️ maxThinkingTokens<br/>(единственный pre-контроль)

    UI->>Orch: POST /agent/query
    Orch->>SDK: claude.query(prompt)
    SDK->>API: API Request

    rect rgb(255, 245, 200)
        Note over API: 🧠 THINKING<br/>Происходит ЗДЕСЬ<br/>(серверы Anthropic)<br/>❌ Нельзя вмешаться

        loop Streaming chunks
            API-->>SDK: thinking chunk
            SDK-->>Orch: thinking event
            Orch-->>UI: SSE: {type: "thinking"}
            Note over UI: 👁️ Только наблюдение
        end
    end

    Note over API: ✅ Thinking завершён

    API-->>SDK: Tool Call decision

    rect rgb(200, 255, 200)
        Note over Orch,SDK: ⚡ PreToolUse Hook<br/>✅ ПЕРВАЯ точка вмешательства!
        SDK->>Orch: PreToolUse(tool, input)
        Orch-->>SDK: block / modify / allow
    end
```

### Контроль над Thinking

| Момент | ✅ Можно | ❌ Нельзя |
|--------|----------|-----------|
| **До запроса** | `maxThinkingTokens` — ограничить объём | Задать направление мыслей |
| **Во время** | Получать stream, отображать в UI | Модифицировать, направлять, фильтровать |
| **Прервать** | Abort всего запроса (interrupt) | Откатить только thinking |

### Где находятся точки контроля относительно Thinking

```mermaid
flowchart LR
    subgraph Before["ДО Thinking"]
        CP1["🎛️ CP1: Config<br/>maxThinkingTokens"]
    end

    subgraph During["ВО ВРЕМЯ Thinking"]
        TH["🧠 Thinking<br/>(чёрный ящик)"]
        CP2["🛑 CP2: Interrupt<br/>(abort всего)"]
    end

    subgraph After["ПОСЛЕ Thinking"]
        CP3["⚡ CP3: PreToolUse"]
        CP4["📊 CP4: PostToolUse"]
        CP5["🔐 CP5: canUseTool"]
    end

    CP1 --> TH
    TH -.->|"только stream"| CP2
    TH --> CP3
    CP3 --> CP4
    CP3 --> CP5

    style TH fill:#fff3cd,stroke:#ffc107,color:#000
    style CP1 fill:#ff9800,stroke:#e65100,color:#000
    style CP2 fill:#f44336,stroke:#b71c1c,color:#fff
    style CP3 fill:#4caf50,stroke:#1b5e20,color:#fff
    style CP4 fill:#2196f3,stroke:#0d47a1,color:#fff
    style CP5 fill:#9c27b0,stroke:#4a148c,color:#fff
```

**Вывод:** Реальный контроль над действиями агента начинается **после thinking**, когда агент принял решение вызвать tool.

---

## Таблица точек контроля

| # | Точка | Где | Когда | Возможные действия |
|---|-------|-----|-------|-------------------|
| 🎛️ CP1 | Initial Config | UI | До создания агента | Выбор tools, model, prompt, limits, MCP |
| 🛑 CP2 | Interrupt | UI | В любой момент | Остановить агента |
| ⚡ CP3 | PreToolUse | Orchestrator | Перед каждым tool | Block, Modify, Allow, Request Permission |
| 📊 CP4 | PostToolUse | Orchestrator | После каждого tool | Log, Analyze, Alert |
| 🔐 CP5 | canUseTool | Orchestrator | При проверке tool | Custom permission logic |
| 📏 CP6 | Limits | SDK | Автоматически | Auto-stop при превышении |

---

## Компоненты

### Orchestrator (TypeScript)

| Файл | Назначение |
|------|------------|
| `src/index.ts` | Entry point, инициализация и запуск сервера |
| `src/server.ts` | Express routes, SSE endpoints, middleware |
| `src/agent-manager.ts` | Класс AgentManager — управление сессиями агентов |
| `src/types.ts` | TypeScript интерфейсы (AgentConfig, AgentEvent, etc.) |
| `src/presets.ts` | Предустановленные профили агентов |

### UI (C++ Builder)

| Файл | Назначение |
|------|------------|
| `uMain.cpp/h/dfm` | Главная форма TfrmMain с UI компонентами |
| `uOrchestratorClient.cpp/h` | Класс TOrchestratorClient — HTTP/SSE клиент |
| `uAgentConfig.cpp/h` | Класс TAgentConfig — конфигурация агента |
| `uEventParser.cpp/h` | Парсинг JSON событий из SSE stream |

---

## Типы событий SSE

| Тип | Описание | Данные |
|-----|----------|--------|
| `session_start` | Сессия агента создана | `sessionId`, `config` |
| `tool_start` | Начало выполнения tool | `tool`, `input`, `toolUseId` |
| `tool_end` | Tool завершён успешно | `tool`, `output`, `toolUseId` |
| `tool_error` | Ошибка tool | `tool`, `error`, `toolUseId` |
| `assistant_message` | Текстовый ответ агента | `content`, `uuid` |
| `thinking` | Размышления агента (stream) | `content` |
| `permission_request` | Запрос разрешения | `tool`, `input` |
| `session_end` | Сессия завершена | `reason`, `usage` |
| `error` | Ошибка системы | `message` |

---

## Технический стек

| Компонент | Технологии |
|-----------|------------|
| **Orchestrator** | Node.js 18+, TypeScript, Express, Claude Agent SDK |
| **UI** | RAD Studio 12+, C++ Builder, VCL, Indy (HTTP/SSE) |
| **Протоколы** | HTTP REST, Server-Sent Events (SSE), JSON |
| **MCP** | HTTP transport на localhost |
