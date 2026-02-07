# Multi-Agent Architecture

Концептуальная архитектура ClaBot для запуска произвольного числа агентов
с маршрутизацией потоков данных между ними.

---

## 1. Общая схема

```mermaid
graph TB
    subgraph UI ["UI Layer (C++ Builder)"]
        AgentTabs["Agent Tabs<br/>(по вкладке на агента)"]
        PipelineEditor["Pipeline Editor<br/>(визуальный граф потоков)"]
        GlobalLog["Global Event Log<br/>(агрегированный поток)"]
    end

    subgraph GW ["API Gateway"]
        REST["REST API"]
        SSE_MUX["SSE Multiplexer<br/>(один поток на UI,<br/>события всех агентов)"]
    end

    subgraph ORCH ["Orchestration Layer"]
        AgentPool["Agent Pool<br/>(создание / удаление /<br/>lifecycle агентов)"]
        PipeMgr["Pipe Manager<br/>(маршруты данных<br/>между агентами)"]
        Scheduler["Scheduler<br/>(очередь задач,<br/>DAG-исполнение)"]
    end

    subgraph BUS ["Event Bus"]
        PubSub["Pub/Sub<br/>(topic = agentId | pipe | global)"]
    end

    subgraph AGENTS ["Agent Runtime Pool"]
        A1["Agent A<br/>role: researcher"]
        A2["Agent B<br/>role: coder"]
        A3["Agent C<br/>role: reviewer"]
        AN["Agent N<br/>..."]
    end

    subgraph TOOLS ["Tool / MCP Layer"]
        SDK["Claude Agent SDK"]
        MCP1["MCP: ProjectMemory"]
        MCP2["MCP: DB_MCP"]
        MCP3["MCP: Custom..."]
        SharedFS["Shared Workspace<br/>(файлы, артефакты)"]
    end

    subgraph STORE ["Persistence"]
        SessionDB["Session Store<br/>(агенты, конфиги, resume)"]
        EventLog["Event Log<br/>(все события для replay)"]
        ArtifactStore["Artifact Store<br/>(результаты, файлы)"]
    end

    UI -->|HTTP| GW
    GW -->|SSE multiplex| UI
    REST --> AgentPool
    REST --> PipeMgr
    REST --> Scheduler
    SSE_MUX ---|подписка| PubSub

    AgentPool --> A1 & A2 & A3 & AN
    PipeMgr -->|route events| PubSub
    Scheduler -->|launch / chain| AgentPool

    A1 & A2 & A3 & AN -->|emit| PubSub
    A1 & A2 & A3 & AN --> SDK
    A1 & A2 & A3 & AN --> MCP1 & MCP2 & MCP3
    A1 & A2 & A3 & AN --> SharedFS

    PubSub -->|deliver| A1 & A2 & A3 & AN
    PubSub --> EventLog
    AgentPool --> SessionDB
    Scheduler --> ArtifactStore
```

---

## 2. Pipe Manager — маршрутизация потоков

Ключевая новая абстракция: **Pipe** — направленный канал данных между агентами.

```mermaid
graph LR
    subgraph "Pipe: A → B (assistant_message → prompt)"
        A_out["Agent A<br/>emit: assistant_message"]
        PIPE_AB["Pipe<br/>filter: assistant_message<br/>transform: → prompt"]
        B_in["Agent B<br/>receive: prompt"]
    end

    A_out -->|event| PIPE_AB -->|inject| B_in

    subgraph "Pipe: B → C (tool_end → prompt)"
        B_out["Agent B<br/>emit: tool_end(Write)"]
        PIPE_BC["Pipe<br/>filter: tool_end<br/>tool=Write<br/>transform: → review prompt"]
        C_in["Agent C<br/>receive: prompt"]
    end

    B_out -->|event| PIPE_BC -->|inject| C_in

    subgraph "Pipe: C → A (assistant_message → prompt)"
        C_out["Agent C<br/>emit: assistant_message"]
        PIPE_CA["Pipe<br/>filter: has 'fix request'<br/>transform: → fix prompt"]
        A_in["Agent A<br/>receive: prompt"]
    end

    C_out -->|event| PIPE_CA -->|inject| A_in
```

### Pipe Definition

```mermaid
classDiagram
    class Pipe {
        +string id
        +string sourceAgentId
        +string targetAgentId
        +string[] filterEventTypes
        +FilterFn filterFn
        +TransformFn transformFn
        +boolean enabled
        +int maxMessages
        +PipeStatus status
    }

    class PipeManager {
        +Map~string,Pipe~ pipes
        +createPipe(config) Pipe
        +deletePipe(id)
        +enablePipe(id)
        +disablePipe(id)
        +listPipes() Pipe[]
        +getPipesForAgent(agentId) Pipe[]
        -routeEvent(agentId, event)
    }

    class EventBus {
        +subscribe(topic, listener)
        +unsubscribe(topic, listener)
        +publish(topic, event)
    }

    PipeManager --> EventBus : subscribes to agent topics
    PipeManager --> Pipe : manages
    Pipe --> EventBus : publishes to target agent
```

---

## 3. Agent Lifecycle (расширенный)

```mermaid
stateDiagram-v2
    [*] --> Created : POST /agents
    Created --> Idle : init complete
    Idle --> Running : receive prompt<br/>(manual or via pipe)
    Running --> Idle : session_end
    Running --> Interrupted : interrupt
    Interrupted --> Idle : reset
    Idle --> WaitingForInput : pipe connected,<br/>awaiting upstream
    WaitingForInput --> Running : pipe delivers data
    Running --> Running : resume (next turn)
    Idle --> Terminated : DELETE
    Interrupted --> Terminated : DELETE
    Terminated --> [*]

    note right of WaitingForInput
        Агент подключён к pipe
        и ждёт данных от другого
        агента перед запуском
    end note
```

---

## 4. Паттерны композиции агентов

### 4.1. Chain (цепочка)

```mermaid
graph LR
    User([User Prompt]) --> A["Agent A<br/>Research"]
    A -->|pipe: result → prompt| B["Agent B<br/>Code"]
    B -->|pipe: result → prompt| C["Agent C<br/>Review"]
    C -->|pipe: result → response| User2([Final Response])

    style A fill:#e1f5fe
    style B fill:#f3e5f5
    style C fill:#e8f5e9
```

### 4.2. Fan-out / Fan-in (параллельная обработка)

```mermaid
graph TB
    User([User Prompt]) --> Splitter["Splitter Agent<br/>(разбивает задачу)"]
    Splitter -->|"subtask 1"| W1["Worker 1"]
    Splitter -->|"subtask 2"| W2["Worker 2"]
    Splitter -->|"subtask 3"| W3["Worker 3"]
    W1 -->|result| Aggregator["Aggregator Agent<br/>(собирает результаты)"]
    W2 -->|result| Aggregator
    W3 -->|result| Aggregator
    Aggregator --> User2([Combined Response])

    style Splitter fill:#fff3e0
    style Aggregator fill:#fff3e0
    style W1 fill:#e1f5fe
    style W2 fill:#e1f5fe
    style W3 fill:#e1f5fe
```

### 4.3. Supervisor (контроль качества)

```mermaid
graph TB
    User([User Prompt]) --> Supervisor["Supervisor Agent<br/>(планирует, оценивает)"]
    Supervisor -->|task| Worker["Worker Agent<br/>(исполняет)"]
    Worker -->|result| Supervisor
    Supervisor -->|"quality OK"| User2([Response])
    Supervisor -->|"retry with feedback"| Worker

    style Supervisor fill:#fce4ec
    style Worker fill:#e1f5fe
```

### 4.4. Debate (состязательная проверка)

```mermaid
graph TB
    User([Prompt]) --> ProAgent["Pro Agent<br/>(аргументы за)"]
    User --> ConAgent["Con Agent<br/>(аргументы против)"]
    ProAgent -->|arguments| Judge["Judge Agent<br/>(оценивает)"]
    ConAgent -->|arguments| Judge
    Judge -->|verdict| User2([Response])

    style ProAgent fill:#e8f5e9
    style ConAgent fill:#ffebee
    style Judge fill:#fff3e0
```

---

## 5. API (расширение текущего REST)

```mermaid
graph LR
    subgraph "Текущие endpoints"
        E1["POST /agent/create"]
        E2["POST /agent/:id/query"]
        E3["GET  /agent/:id/events"]
        E4["POST /agent/:id/interrupt"]
        E5["GET  /agent/:id/status"]
        E6["DELETE /agent/:id"]
    end

    subgraph "Новые endpoints"
        N1["POST /pipes<br/>создать pipe между агентами"]
        N2["DELETE /pipes/:id<br/>удалить pipe"]
        N3["GET /pipes<br/>список всех pipes"]
        N4["PATCH /pipes/:id<br/>enable/disable pipe"]
        N5["GET /events/global<br/>SSE: все события всех агентов"]
        N6["POST /pipelines<br/>создать pipeline из шаблона"]
        N7["GET /pipelines/:id/status<br/>статус pipeline"]
        N8["POST /agents/:id/inject<br/>вручную подать данные агенту"]
    end

    style N1 fill:#e8f5e9
    style N2 fill:#e8f5e9
    style N3 fill:#e8f5e9
    style N4 fill:#e8f5e9
    style N5 fill:#e1f5fe
    style N6 fill:#fff3e0
    style N7 fill:#fff3e0
    style N8 fill:#f3e5f5
```

---

## 6. Event Bus — центральная шина событий

```mermaid
graph TB
    subgraph Producers
        A1[Agent A] -->|emit| BUS
        A2[Agent B] -->|emit| BUS
        A3[Agent C] -->|emit| BUS
    end

    BUS["Event Bus<br/>(topics: agent.{id}, pipe.{id}, global)"]

    subgraph Consumers
        BUS -->|"topic: agent.A"| SSE_A["SSE → UI Tab A"]
        BUS -->|"topic: agent.B"| SSE_B["SSE → UI Tab B"]
        BUS -->|"topic: global"| SSE_G["SSE → Global Log"]
        BUS -->|"topic: pipe.AB"| PM["Pipe Manager<br/>→ transform<br/>→ inject to B"]
        BUS -->|"topic: global"| LOG["Event Log<br/>(persistence)"]
    end
```

---

## 7. UI — мультиагентный интерфейс

```mermaid
graph TB
    subgraph MainWindow ["ClaBot Main Window"]
        Toolbar["Toolbar: + New Agent | Pipeline Templates | Global Settings"]

        subgraph TabsArea ["Agent Tabs"]
            Tab1["Tab: Agent A (researcher)"]
            Tab2["Tab: Agent B (coder)"]
            Tab3["Tab: Agent C (reviewer)"]
        end

        subgraph TabContent ["Active Tab Content"]
            Config["Config Panel<br/>(model, tools, prompt)"]
            Events["Events ListView"]
            Details["Details Memo"]
        end

        subgraph BottomPanel ["Bottom Panel"]
            PipeView["Pipeline View<br/>(граф агентов и pipes,<br/>статусы в реальном времени)"]
            GlobalEvents["Global Event Stream<br/>(все агенты, цветовая<br/>маркировка по агенту)"]
            Stats["Aggregated Stats<br/>(total tokens, cost,<br/>agents running)"]
        end
    end
```

---

## 8. Модель данных

```mermaid
erDiagram
    AGENT {
        string id PK
        string name
        string role
        string model
        string systemPrompt
        string status
        string sdkSessionId
        int inputTokens
        int outputTokens
        float totalCostUsd
    }

    PIPE {
        string id PK
        string sourceAgentId FK
        string targetAgentId FK
        string filterEventTypes
        string transformType
        boolean enabled
        int messageCount
    }

    PIPELINE {
        string id PK
        string name
        string templateType
        string status
    }

    EVENT {
        string id PK
        string agentId FK
        string type
        string data
        datetime timestamp
        string pipeId FK
    }

    PIPELINE ||--|{ AGENT : contains
    PIPELINE ||--|{ PIPE : defines
    AGENT ||--o{ PIPE : "source"
    AGENT ||--o{ PIPE : "target"
    AGENT ||--o{ EVENT : emits
    PIPE ||--o{ EVENT : routes
```

---

## 9. Сценарий: Code Review Pipeline

Пример конкретного pipeline из трёх агентов:

```mermaid
sequenceDiagram
    actor User
    participant UI
    participant Orch as Orchestrator
    participant A as Agent A<br/>(Coder)
    participant B as Agent B<br/>(Reviewer)
    participant C as Agent C<br/>(Fixer)

    User->>UI: "Implement feature X"
    UI->>Orch: POST /pipelines {template: "code-review", prompt: ...}
    Orch->>Orch: Create agents A, B, C
    Orch->>Orch: Create pipes: A→B, B→C, C→B

    Orch->>A: query(prompt)
    A->>A: Write code (tool: Write, Edit)
    A-->>Orch: session_end + assistant_message

    Note over Orch: Pipe A→B triggers
    Orch->>B: query("Review this code: {A.result}")
    B->>B: Read files, analyze
    B-->>Orch: assistant_message with review

    alt Has issues
        Note over Orch: Pipe B→C triggers
        Orch->>C: query("Fix issues: {B.review}")
        C->>C: Edit files
        C-->>Orch: session_end

        Note over Orch: Pipe C→B triggers (re-review)
        Orch->>B: query("Re-review after fixes: {C.result}")
        B-->>Orch: "Approved"
    end

    Orch-->>UI: Pipeline complete
    UI-->>User: Show results from all agents
```

---

## 10. Этапы реализации

```mermaid
gantt
    title Roadmap: Multi-Agent ClaBot
    dateFormat YYYY-MM-DD
    axisFormat %b

    section Phase 1: Foundation
    Event Bus (pub/sub)             :p1a, 2025-07-01, 2w
    Agent Pool (N agents)           :p1b, after p1a, 2w
    SSE Multiplexer                 :p1c, after p1a, 1w
    UI: Agent Tabs                  :p1d, after p1b, 2w

    section Phase 2: Pipes
    Pipe Manager                    :p2a, after p1d, 2w
    Pipe API (CRUD)                 :p2b, after p2a, 1w
    Transform functions             :p2c, after p2a, 2w
    UI: Pipeline view               :p2d, after p2b, 2w

    section Phase 3: Patterns
    Chain pattern                   :p3a, after p2c, 1w
    Fan-out / Fan-in                :p3b, after p3a, 2w
    Supervisor pattern              :p3c, after p3b, 1w
    Pipeline templates              :p3d, after p3c, 1w

    section Phase 4: Production
    Persistence (sessions, events)  :p4a, after p3d, 2w
    Global event log + replay       :p4b, after p4a, 1w
    Cost guardrails (budget limits) :p4c, after p4a, 1w
    UI: Stats dashboard             :p4d, after p4b, 2w
```

---

## Ключевые принципы

1. **Event Bus как ядро** — вся коммуникация через pub/sub, агенты не знают друг о друге напрямую
2. **Pipe = фильтр + трансформ** — гибкая маршрутизация: какие события пропускать, как преобразовать перед доставкой
3. **Агенты stateless** — состояние в Session Store, агент можно пересоздать и возобновить через SDK resume
4. **UI отделён от топологии** — UI подписывается на topics Event Bus, не управляет pipes напрямую
5. **Инкрементальная реализация** — каждая фаза самоценна: N агентов → pipes → паттерны → persistence
