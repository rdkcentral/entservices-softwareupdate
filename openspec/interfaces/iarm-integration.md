# IARM Integration and Eventing Model

## Registration Model

- Under IARM build macros, plugin initializes IARM and registers _MaintenanceMgrEventHandler for:
  - owner: IARM_BUS_MAINTENANCE_MGR_NAME
  - event: IARM_BUS_MAINTENANCEMGR_EVENT_UPDATE
- Deinitialize removes the same handler.

## Event Dispatch

- Static _MaintenanceMgrEventHandler forwards events to singleton instance iarmEventHandler().
- iarmEventHandler processes module status from IARM_Bus_MaintMGR_EventData_t.

## Processed module statuses

- Completion/status events mapped in switch include:
  - MAINT_RFC_COMPLETE / MAINT_RFC_ERROR
  - MAINT_FWDOWNLOAD_COMPLETE / MAINT_FWDOWNLOAD_ERROR / MAINT_FWDOWNLOAD_ABORTED
  - MAINT_LOGUPLOAD_COMPLETE / MAINT_LOGUPLOAD_ERROR
  - MAINT_REBOOT_REQUIRED
  - MAINT_CRITICAL_UPDATE
  - MAINT_*_INPROGRESS variants
- Handler updates g_task_status bitmask and m_task_map, and signals worker via condition variable.

## Control impact on workflow

- Task completion/error events are the primary unblock mechanism for worker thread wait points.
- Final maintenance status is derived after g_task_status indicates all tasks accounted for.

- IARM event timing/order correctness is a critical runtime dependency for maintenance determinism.

## Additional IPC usage

- During active maintenance mode changes, plugin broadcasts IARM event to owner RdkvFWupgrader with mode integer.

- Event contract details for the mode-broadcast channel are not documented in this plugin repository.

## Test-backed intent

- L1 test suite validates IARM registration expectation at plugin initialization and exercises maintenance JSON-RPC interactions with mocked bus behavior.

- L1 contains expectation hooks for an additional event ID (IARM_BUS_DCM_NEW_START_TIME_EVENT) not visibly handled in current production plugin source, requiring branch alignment check.
