# System Context and Process Interaction Diagrams

## Subsystem Interaction Diagram

```mermaid
flowchart LR
    classDef host fill:#dfe8f7,stroke:#365a96,stroke-width:1px,color:#10213f;
    classDef plugin fill:#e8f4ea,stroke:#2f7d4c,stroke-width:1px,color:#10321d;
    classDef ext fill:#f8efe1,stroke:#996515,stroke-width:1px,color:#4a3009;
    classDef data fill:#f3e8f8,stroke:#7a3e9d,stroke-width:1px,color:#35124b;

    TH[Thunder Host Process]:::host --> MM[MaintenanceManager Plugin]:::plugin

    MM --> API[Northbound JSON-RPC Clients]:::ext
    MM --> IARM[IARMBus: IARM_BUS_MAINTENANCE_MGR_NAME]:::ext

    MM --> NET[org.rdk.Network(.1)]:::ext
    MM --> AUTH[org.rdk.AuthService(.1)]:::ext
    MM --> SEC[org.rdk.SecManager(.1)]:::ext
    MM --> SA[SecurityAgent]:::ext

    MM --> RFC[RFC API: get/set RFC parameters]:::ext
    MM --> SET[cSettings:/opt/maintenance_mgr_record.conf]:::data

    MM --> TSK[/lib/rdk/Start_MaintenanceTasks.sh]:::ext
    MM --> IMG[/lib/rdk/xconfImageCheck.sh]:::ext
    TSK --> PROC[rfcMgr / rdkvfwupgrader / logupload]:::ext
```

## Process Communication Diagram

```mermaid
flowchart TB
    classDef process fill:#e6eefc,stroke:#315ea8,stroke-width:1px,color:#10284f;
    classDef bus fill:#eef8ea,stroke:#3c8b3c,stroke-width:1px,color:#173d17;
    classDef script fill:#fff2e6,stroke:#b36b00,stroke-width:1px,color:#4a2b00;

    Client[Remote JSON-RPC Client]:::process -->|HTTP JSON-RPC| Thunder[Thunder Process]:::process
    Thunder -->|Dispatch| MM[MaintenanceManager plugin instance]:::process

    MM -->|Subscribe/Invoke| Network[org.rdk.Network.1]:::process
    MM -->|Invoke| SecMgr[org.rdk.SecManager.1]:::process
    MM -->|Query Interface| AuthSvc[org.rdk.AuthService]:::process

    MM -->|Register/Handle Events| Iarm[(IARMBus)]:::bus
    Iarm -->|MAINTENANCEMGR_EVENT_UPDATE| MM

    MM -->|system()| StartScript[/lib/rdk/Start_MaintenanceTasks.sh]:::script
    MM -->|system()| Xconf[/lib/rdk/xconfImageCheck.sh]:::script

    StartScript --> TaskProc[Task executables]:::process
    TaskProc -->|maintenance status events| Iarm
```
