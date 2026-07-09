# MaintenanceManager Baseline Architecture for OpenSpec

## Scope and Provenance

- Repository: rdkcentral/entservices-maintenancemanager
- Baseline branch: develop
- Baseline revision: repository state at time of OpenSpec onboarding (see git history for exact commit)
- Analysis scope: MaintenanceManager plugin only
- Out-of-scope by design: Packager plugin internals, except for build/scope boundary context

### Evidence Baseline

- Plugin build and install shape: MaintenanceManager/CMakeLists.txt
- Thunder module wiring: MaintenanceManager/Module.h, MaintenanceManager/Module.cpp
- Plugin runtime and API behavior: MaintenanceManager/MaintenanceManager.h, MaintenanceManager/MaintenanceManager.cpp
- Plugin runtime config metadata: MaintenanceManager/MaintenanceManager.config, MaintenanceManager/MaintenanceManager.conf.in
- Top-level plugin enablement boundaries: CMakeLists.txt, services.cmake
- Test-backed intent and edge behavior: Tests/L1Tests/tests/test_MaintenanceManager.cpp, Tests/L2Tests/tests/MaintenancemanagerL2.cpp

## System Purpose

- MaintenanceManager is implemented as a Thunder/WPEFramework plugin class inheriting PluginHost::IPlugin and PluginHost::JSONRPC.
- It is packaged as a shared library module from MaintenanceManager.cpp and Module.cpp.
- It is configured with callsign org.rdk.MaintenanceManager and autostart true in both MaintenanceManager.config and MaintenanceManager.conf.in.
- It orchestrates maintenance tasks primarily by launching shell scripts under /lib/rdk, including Start_MaintenanceTasks.sh and xconfImageCheck.sh.

- This plugin acts as a control-plane orchestrator for maintenance/update lifecycle, not as the implementation owner of each maintenance subtask.
- Subtasks are delegated to external executables/scripts and report progress via IARMBus events.

- On production targets, the host Thunder process and all dependent plugins/services are managed by standard RDK service supervision.

- Device-specific policy contracts for how often unsolicited maintenance should run are not fully specified in this repository.

## Architectural Overview

MaintenanceManager is a hosted component inside Thunder. It exposes northbound JSON-RPC operations for status, mode control, and execution control; consumes southbound events from IARMBus; queries other Thunder plugins (Network, AuthService, SecManager) through authenticated JSON-RPC/Exchange interfaces; and persists selected state to a local settings file abstraction.

### Primary Responsibilities

- Boot-time maintenance trigger path via maintenanceManagerOnBootup().
- Solicited (manual/API) maintenance trigger path via startMaintenance().
- Maintenance task sequencing in task_execution_thread().
- Status/event publication via onMaintenanceStatusChange() notification.
- Persistence of selected state including LastSuccessfulCompletionTime and mode/opt-out values.

### Service Boundaries and Non-Ownership

- Does not directly implement firmware updater logic; launches Start_MaintenanceTasks.sh SWUPDATE and reacts to events.
- Does not directly implement network stack or activation backend; consumes org.rdk.Network, org.rdk.AuthService, and org.rdk.SecManager.
- Does not own low-level update task binaries (for example rdkvfwupgrader, rfcMgr, logupload) but may terminate them via process signaling on abort.

- Plugin boundary is orchestration and policy enforcement; task correctness and artifact handling are delegated outside this codebase unit.

## Runtime Models

### Thunder Hosting Model

- Service registration macro uses API version constants defined in the implementation.
- Constructor registers JSON-RPC methods:
  - getMaintenanceActivityStatus
  - getMaintenanceStartTime
  - setMaintenanceMode
  - startMaintenance
  - stopMaintenance
  - getMaintenanceMode
- Initialize() stores IShell, checks WhoAmI feature flag from /etc/device.properties, optionally pre-subscribes to SecManager device context event, initializes IARM integration, and registers SIGALRM handler for task timeout.
- Deinitialize() deletes timer, stops maintenance tasks, removes IARM handler, releases IShell/AuthService resources.

- Lifetime is tightly coupled to Thunder plugin activation lifecycle; worker thread and timer are plugin-owned resources that must be cleaned up on deactivation.

### Maintenance Execution Model

- Boot path sets maintenance type to UNSOLICITED_MAINTENANCE, evaluates skip criteria, and starts worker thread only when unsolicited execution is required.
- Unsolicited skip criteria: previous persisted status is MAINTENANCE_COMPLETE and current boot reason is a maintenance reboot marker.
- When unsolicited execution is skipped, plugin sets status to MAINTENANCE_COMPLETE and marks unsolicited-complete true for subsequent solicited requests.
- API path startMaintenance() sets maintenance type to SOLICITED_MAINTENANCE (only when unsolicited cycle has completed and status is not STARTED).
- Worker thread computes task list, performs network/activation/whoami gating, launches tasks sequentially, waits on condition variable for module completion events, applies timeout, supports one retry for failed task invocation.
- Task timeout uses POSIX timer APIs and SIGALRM callback to mark task as error-complete and continue orchestration.

- Execution is event-assisted sequential orchestration with a coarse-grained status bitmask used as the finalization gate.

## Interface Summary

### Northbound (JSON-RPC)

- Methods exposed through org.rdk.MaintenanceManager.1 namespace:
  - getMaintenanceActivityStatus
  - getMaintenanceStartTime
  - setMaintenanceMode
  - startMaintenance
  - stopMaintenance
  - getMaintenanceMode
- Notification emitted: onMaintenanceStatusChange with maintenanceStatus string payload.

- API consumers are expected to activate plugin via Controller and then call methods in service namespace.

### Southbound

- IARMBus event source: IARM_BUS_MAINTENANCE_MGR_NAME, event IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE.
- Network dependency: org.rdk.Network(.1), including isConnectedToInternet and onInternetStatusChange subscription.
- Identity/activation dependencies:
  - org.rdk.AuthService via Exchange::IAuthService (GetActivationStatus, SetPartnerId)
  - org.rdk.SecManager.1 via JSON-RPC (getDeviceInitializationContext, onDeviceInitializationContextUpdate)
- Security token dependency via SecurityAgent to build downstream Thunder JSON-RPC link query token.

## Subsystem Responsibilities

- Runtime/lifecycle control: see runtime/plugin-lifecycle.md
- Execution workflow and decision points: see runtime/maintenance-execution-flow.md
- API contract and client semantics: see interfaces/jsonrpc-interface.md
- IARMBus/event IPC model: see interfaces/iarm-integration.md
- Task sequence/abort/timeout logic: see subsystems/task-orchestration.md
- State and persistence model: see subsystems/state-and-persistence.md

## Key Questions Answered (Baseline)

1. Hosting/activation/invocation
- Hosted as Thunder plugin shared library with SERVICE_REGISTRATION and org.rdk.MaintenanceManager callsign/autostart.

2. True northbound APIs and notifications
- Six JSON-RPC methods and one maintenance status notification as listed above.

3. Downstream systems coordinated
- IARMBus maintenance event provider, Network plugin, AuthService, SecManager, SecurityAgent, RFC APIs, shell task scripts/processes.

4. Start decision logic
- Split between unsolicited boot path and solicited API path, both gated by network and activation/whoami logic variants.

5. Task sequencing/tracking
- Sequential script execution with condition-variable waits on IARMBus completion/error events and bitmask completion aggregation.

6. IARMBus impact on control flow
- Event handler sets per-task and aggregate status bits, triggers worker wakeups, derives final MAINTENANCE_COMPLETE/ERROR/INCOMPLETE outcome.

7. Persistent state location and updates
- cSettings over /opt/maintenance_mgr_record.conf for mode/optout and LastSuccessfulCompletionTime; RFC writes for initialization context values.

8. Success/incomplete/error/reboot-required derivation
- Derived from g_task_status bitmask and event-driven flags, with reboot-required and critical-maintenance tracked as separate state fields.

## Risks and Validation Gaps

- On-device timing/signal behavior with real scripts and IARM event ordering remains environment-sensitive.
- Security token and downstream plugin availability/retry behaviors need runtime validation in integrated stack.
- Some L1 expectations reference additional IARM event IDs not directly evident in current implementation and should be reconciled.

See unknowns/manual-validation.md for the full manual validation checklist.
