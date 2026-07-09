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
- Timeout protection using timer and signal handling
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
