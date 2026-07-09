# Runtime Sequence and State Diagrams

## Plugin Lifecycle Sequence

```mermaid
sequenceDiagram
    participant TH as Thunder Host
    participant MM as MaintenanceManager
    participant IARM as IARMBus

    TH->>MM: Constructor
    MM->>MM: Register JSON-RPC methods
    TH->>MM: Initialize(IShell)
    MM->>MM: Detect WHOAMI_SUPPORT
    MM->>IARM: Register maintenance event handler
    MM->>MM: maintenanceManagerOnBootup()
    MM->>MM: Spawn task_execution_thread()

    Note over MM: Runtime: API calls + IARM events + timers

    TH->>MM: Deinitialize(IShell)
    MM->>MM: delete timer + stop tasks
    MM->>IARM: Remove event handler
    MM->>TH: Release interfaces
```

## Initialization Flowchart

```mermaid
flowchart TD
    A[Initialize()] --> B[Store IShell and AddRef]
    B --> C{WHOAMI_SUPPORT true?}
    C -- Yes --> D[subscribeToDeviceInitializationEvent]
    C -- No --> E[Skip pre-subscribe]
    D --> F[InitializeIARM]
    E --> F
    F --> G[Register IARM maintenance event handler]
    G --> H[maintenanceManagerOnBootup]
    H --> I[Set default state and UNSOLICITED type]
    I --> I1{Skip unsolicited?}
    I1 -- Yes --> I2[Post MAINTENANCE_COMPLETE and set unsolicited_complete]
    I1 -- No --> J[Emit MAINTENANCE_IDLE notification]
    J --> K[Start worker thread]
    I2 --> L[Register SIGALRM handler]
    K --> L[Register SIGALRM handler]
    L --> M[Initialize success]
```

## Maintenance Execution Sequence

```mermaid
sequenceDiagram
    participant Trigger as Boot or startMaintenance RPC
    participant MM as MaintenanceManager
    participant NET as org.rdk.Network.1
    participant SEC as org.rdk.SecManager.1
    participant AUTH as org.rdk.AuthService
    participant SCR as Start_MaintenanceTasks.sh
    participant IARM as IARMBus

    Trigger->>MM: Start cycle
    MM->>MM: task_execution_thread()
    MM->>NET: isConnectedToInternet + event subscribe
    alt WhoAmI enabled and unsolicited
        MM->>AUTH: activation status
        MM->>SEC: getDeviceInitializationContext
        SEC-->>MM: context or wait for update event
    end

    MM->>SCR: Run RFC task
    SCR-->>IARM: RFC complete/error event
    IARM-->>MM: Update task bits + notify worker

    MM->>SCR: Run SWUPDATE task
    SCR-->>IARM: FW event
    IARM-->>MM: Update task bits + notify worker

    MM->>SCR: Run LOGUPLOAD task
    SCR-->>IARM: LOGUPLOAD event
    IARM-->>MM: Update task bits + notify worker

    MM->>MM: Derive final outcome from bitmask
    MM-->>Trigger: onMaintenanceStatusChange
```

## IARMBus Event Handling Sequence

```mermaid
sequenceDiagram
    participant Task as Task executable/script
    participant IARM as IARMBus
    participant MM as MaintenanceManager
    participant Worker as task_execution_thread

    Task->>IARM: MAINTENANCEMGR_EVENT_UPDATE(status)
    IARM->>MM: _MaintenanceMgrEventHandler
    MM->>MM: iarmEventHandler parse status
    MM->>MM: Update m_task_map and g_task_status
    MM->>Worker: task_thread.notify_one()
    Worker->>Worker: Continue/next task/finalize
```

## Solicited vs Unsolicited Flow Comparison

```mermaid
flowchart LR
    U[Unsolicited Boot Flow] --> U1[maintenanceManagerOnBootup]
    U1 --> U2[g_maintenance_type=UNSOLICITED]
    U2 --> U3{Last status COMPLETE and maintenance reboot?}
    U3 -- Yes --> U4[Set COMPLETE and unsolicited_complete=true]
    U3 -- No --> U5[May delay STARTED when WhoAmI enabled]
    U5 --> U6[Auto-run worker]

    S[Solicited API Flow] --> S1[startMaintenance]
    S1 --> S2[Requires unsolicited_complete and not STARTED]
    S2 --> S3[g_maintenance_type=SOLICITED]
    S3 --> S4[Set reboot_pending true]
    S4 --> S5[Start worker]
```

## Task-Orchestration State Transition Diagram

```mermaid
stateDiagram-v2
    [*] --> IDLE
    IDLE --> STARTED: boot trigger or startMaintenance
    STARTED --> STARTED: task complete/error events update bitmask
    STARTED --> ERROR: no network / abort / terminal errors
    STARTED --> INCOMPLETE: skipped-task terminal state
    STARTED --> COMPLETE: all success bits set
    COMPLETE --> [*]
    ERROR --> [*]
    INCOMPLETE --> [*]
```
