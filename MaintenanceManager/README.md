# MaintenanceManager

**Version:** 1.15.3

## Overview

MaintenanceManager is a Thunder plugin that orchestrates maintenance workflows for RDK devices.
It is not a standalone daemon. It runs inside the Thunder host process and exposes JSON-RPC APIs
under the plugin callsign.

Core responsibilities:

- Trigger boot-time (unsolicited) and caller-driven (solicited) maintenance cycles
- Sequence maintenance tasks and track completion/error state
- Integrate with IARMBus maintenance events
- Query activation, identity, and network readiness from companion services
- Persist selected maintenance state (mode, opt-out, last successful completion time)

## Plugin Identity and Runtime Model

- Callsign: org.rdk.MaintenanceManager
- Versioned interface namespace: org.rdk.MaintenanceManager.1
- Hosting model: Thunder plugin shared library
- Autostart: enabled by default in plugin config

High-level runtime flow:

1. Plugin initializes and registers JSON-RPC methods.
2. Plugin initializes IARMBus event handling.
3. Boot path starts unsolicited maintenance flow.
4. Worker thread performs network and activation/context gating.
5. Worker executes maintenance task scripts and waits for IARMBus task status events.
6. Plugin emits onMaintenanceStatusChange notifications and final status.

## External Dependencies

MaintenanceManager depends on:

- Thunder framework plugin runtime
- IARMBus (maintenance event integration)
- RFC APIs (read/write selected TR-181 parameters)
- SecurityAgent token flow for downstream Thunder calls
- org.rdk.Network plugin
- org.rdk.AuthService plugin and Exchange interface
- org.rdk.SecManager plugin (device initialization context)
- Runtime task scripts under /lib/rdk

## Orchestration Model

MaintenanceManager coordinates tasks through script execution and event-driven completion.

Primary scripts:

- /lib/rdk/Start_MaintenanceTasks.sh RFC
- /lib/rdk/Start_MaintenanceTasks.sh SWUPDATE
- /lib/rdk/Start_MaintenanceTasks.sh LOGUPLOAD
- /lib/rdk/xconfImageCheck.sh (critical task path)

Behavioral highlights:

- Separate unsolicited and solicited maintenance flow paths
- Timeout protection using a per-arm POSIX timer with an immutable task/generation context delivered by SIGEV_THREAD; the plugin does not change the process-wide SIGALRM disposition
- Retry support for failed task invocation attempts
- Abort path that signals active task processes and transitions to error status
- Final status derivation from task completion/success/skipped bitmasks

## Persisted and Runtime State

Persistent file (via cSettings):

- /opt/maintenance_mgr_record.conf

Persisted values include:

- softwareoptout
- background_flag
- LastSuccessfulCompletionTime

Runtime status includes:

- current maintenance status
- trigger mode and maintenance mode
- critical maintenance flag
- reboot pending flag
- solicited/unsolicited cycle state
- task status bitmask

## Concurrency Model

### Threads

- JSON-RPC dispatch thread(s): Thunder-managed thread(s) that invoke the registered method handlers (getMaintenanceActivityStatus, setMaintenanceMode, startMaintenance, stopMaintenance, getMaintenanceMode, getMaintenanceStartTime).
- IARM event thread: invokes `_MaintenanceMgrEventHandler()` / `iarmEventHandler()` when a maintenance module posts a status update.
- Worker thread (`m_thread`): runs `task_execution_thread()`, sequences and launches the maintenance task scripts.
- Timer thread: POSIX `timer_create()` is configured with `SIGEV_THREAD`, so a per-arm OS thread invokes `timerThreadCallback()` with that arm's immutable task/generation context and then calls `handleTaskTimeout()`; this is not a SIGALRM signal handler.

### Mutexes

| Mutex | Protects |
|---|---|
| `m_callMutex` | `g_currentMode` and `g_triggerMode`; also serializes the task-execution loop in `task_execution_thread()` |
| `m_waiMutex` | `g_listen_to_deviceContextUpdate` (read/written by `task_execution_thread()` and `deviceInitializationContextEventHandler()`) |
| `m_statusMutex` | `m_notify_status`, `g_task_status`, `g_is_critical_maintenance`, `g_is_reboot_pending`, `g_unsolicited_complete`, and `m_workerJoinInProgress` |
| `m_networkEventMutex` | `g_listen_to_nwevents` across the worker and network-event threads |
| `m_taskMapMutex` | `m_task_map` (read/written from the JSON-RPC, IARM event, task-execution, and timer threads) |
| `m_abortFlagMutex` | `m_abort_flag` (read/written from the JSON-RPC and task-execution threads) |
| `m_maintenanceTypeMutex` | `g_maintenance_type` (via `getMaintenanceType()`/`setMaintenanceType()`) |
| `m_currentTaskMutex` | `currentTask` (written by `task_execution_thread()`, snapshotted when arming and by the direct-test-compatible `timer_handler()`) |
| `m_threadMutex` | Assignment, joinability checks, and joins of `m_thread` |
| `m_timerCallbackMutex` (static) | Guards timer-context registration, generation/shutdown state, and in-flight callback accounting |

Every critical section in the source is bracketed with a `// critical section start/end: <mutex>` comment at the lock/unlock (or `lock_guard` scope) to make the boundary explicit.

Each timer arm owns an immutable task name and generation. Disarm/delete unregisters that context so queued stale callbacks return without dereferencing it; callbacks that already acquired a shared context are counted, and `Deinitialize()` waits for that count to reach zero before stopping/releasing plugin resources. The plugin never installs a SIGALRM handler because SIGEV_THREAD does not deliver SIGALRM.

### Lock ordering

- Established order: `m_statusMutex` is acquired before `m_callMutex` when both are needed (see `startMaintenance()`).
- `task_execution_thread()` holds `m_callMutex` for its whole loop; it explicitly `unlock()`s/`lock()`s around any `m_statusMutex` acquisition so the two are never held at once, avoiding a lock-order inversion (Coverity `ORDER_REVERSAL`).
- Terminal IARM/stop paths set `m_workerJoinInProgress` under `m_statusMutex`, release that mutex, join the worker exactly once, and only then publish the final notification; concurrent start/stop requests are rejected during this transition.
- `m_networkEventMutex` is released before starting critical tasks, and terminal paths release `m_statusMutex` before taking `m_threadMutex` to join the worker.

## JSON-RPC API

### Methods

- getMaintenanceActivityStatus
- getMaintenanceStartTime
- setMaintenanceMode
- startMaintenance
- stopMaintenance
- getMaintenanceMode

### Events

- onMaintenanceStatusChange

### Status enum values

- MAINTENANCE_IDLE
- MAINTENANCE_STARTED
- MAINTENANCE_ERROR
- MAINTENANCE_COMPLETE
- MAINTENANCE_INCOMPLETE

## API Examples

### Activate plugin

```bash
curl --request POST \
	--header "Content-Type: application/json" \
	--data '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"org.rdk.MaintenanceManager"}}' \
	http://127.0.0.1:9998/jsonrpc
```

### getMaintenanceActivityStatus

Request:

```bash
curl --request POST \
	--header "Content-Type: application/json" \
	--data '{"jsonrpc":"2.0","id":2,"method":"org.rdk.MaintenanceManager.1.getMaintenanceActivityStatus","params":{}}' \
	http://127.0.0.1:9998/jsonrpc
```

Typical response:

```json
{
	"jsonrpc": "2.0",
	"id": 2,
	"result": {
		"maintenanceStatus": "MAINTENANCE_IDLE",
		"LastSuccessfulCompletionTime": 0,
		"isCriticalMaintenance": false,
		"isRebootPending": false,
		"success": true
	}
}
```

### getMaintenanceStartTime

Request:

```bash
curl --request POST \
	--header "Content-Type: application/json" \
	--data '{"jsonrpc":"2.0","id":3,"method":"org.rdk.MaintenanceManager.1.getMaintenanceStartTime","params":{}}' \
	http://127.0.0.1:9998/jsonrpc
```

Typical response:

```json
{
	"jsonrpc": "2.0",
	"id": 3,
	"result": {
		"maintenanceStartTime": 1735702200,
		"success": true
	}
}
```

### setMaintenanceMode

Request:

```bash
curl --request POST \
	--header "Content-Type: application/json" \
	--data '{"jsonrpc":"2.0","id":4,"method":"org.rdk.MaintenanceManager.1.setMaintenanceMode","params":{"maintenanceMode":"BACKGROUND","optOut":"IGNORE_UPDATE","triggerMode":"USER_INITIATED"}}' \
	http://127.0.0.1:9998/jsonrpc
```

Typical response:

```json
{
	"jsonrpc": "2.0",
	"id": 4,
	"result": {
		"success": true
	}
}
```

### getMaintenanceMode

Request:

```bash
curl --request POST \
	--header "Content-Type: application/json" \
	--data '{"jsonrpc":"2.0","id":5,"method":"org.rdk.MaintenanceManager.1.getMaintenanceMode","params":{}}' \
	http://127.0.0.1:9998/jsonrpc
```

Typical response:

```json
{
	"jsonrpc": "2.0",
	"id": 5,
	"result": {
		"maintenanceMode": "BACKGROUND",
		"triggerMode": "USER_INITIATED",
		"optOut": "IGNORE_UPDATE",
		"success": true
	}
}
```

### startMaintenance

Request:

```bash
curl --request POST \
	--header "Content-Type: application/json" \
	--data '{"jsonrpc":"2.0","id":6,"method":"org.rdk.MaintenanceManager.1.startMaintenance","params":{}}' \
	http://127.0.0.1:9998/jsonrpc
```

### stopMaintenance

Request:

```bash
curl --request POST \
	--header "Content-Type: application/json" \
	--data '{"jsonrpc":"2.0","id":7,"method":"org.rdk.MaintenanceManager.1.stopMaintenance","params":{}}' \
	http://127.0.0.1:9998/jsonrpc
```

## Operational Notes

- startMaintenance is intended for solicited execution and can be rejected while another cycle is active.
- stopMaintenance applies to in-progress maintenance only.
- getMaintenanceStartTime depends on /opt/rdk_maintenance.conf content and timezone rules.
- Event-driven task completion relies on IARMBus status notifications from maintenance modules.

## Build and Configuration Notes

From plugin CMake and config:

- Built as a shared plugin module with MaintenanceManager.cpp and Module.cpp
- Linked with pthread and rt (timer/signal support)
- Integrates with IARMBus when available
- Supports TASK_TIMEOUT override at build time (default 3600 seconds)
- Installs plugin library under the Thunder plugins destination
