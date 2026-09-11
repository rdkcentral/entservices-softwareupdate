# JSON-RPC Interface Architecture

## Service Namespace and Activation

- Callsign is org.rdk.MaintenanceManager.
- Versioned namespace used by clients/tests is org.rdk.MaintenanceManager.1.
- README demonstrates Controller.1.activate flow before invoking methods.

## Methods

## getMaintenanceActivityStatus

- Returns:
  - maintenanceStatus (string enum)
  - LastSuccessfulCompletionTime (integer epoch, 0 if absent/invalid)
  - isCriticalMaintenance (bool)
  - isRebootPending (bool)
  - success (bool)

- This method is authoritative status projection of internal runtime and persisted completion time.

## getMaintenanceStartTime

- Computes next maintenance start epoch from /opt/rdk_maintenance.conf.
- Supports tz_mode values Local time and UTC.
- Returns maintenanceStartTime (int, -1 on parse/open errors) and success true.

- The persistence and ownership lifecycle of /opt/rdk_maintenance.conf is outside this plugin.

## setMaintenanceMode

- Requires maintenanceMode and optOut; optional triggerMode.
- Accepted maintenance modes: FOREGROUND, BACKGROUND.
- Valid optOut values: ENFORCE_OPTOUT, BYPASS_OPTOUT, IGNORE_UPDATE, NONE.
- Persists background flag and softwareoptout values via cSettings.
- During active maintenance, attempts IARM mode broadcast to RdkvFWupgrader and conditionally updates mode.

## getMaintenanceMode

- Returns maintenanceMode, triggerMode, optOut, success.
- Reads optOut from persistent record file and validates accepted values.

## startMaintenance

- Allowed only when maintenance not currently started and unsolicited cycle complete.
- Unsolicited completion includes conditional boot skip path where unsolicited is marked complete without launching worker tasks.
- Sets SOLICITED_MAINTENANCE, clears critical flag, sets reboot pending true, spawns worker thread.

## stopMaintenance

- Delegates to stopMaintenanceTasks and returns success based on stop result.

## Notifications

## onMaintenanceStatusChange

- Emits maintenanceStatus string values from enum mapping:
  - MAINTENANCE_IDLE
  - MAINTENANCE_STARTED
  - MAINTENANCE_ERROR
  - MAINTENANCE_COMPLETE
  - MAINTENANCE_INCOMPLETE

## Concurrency and request serialization

- JSON-RPC handlers run on Thunder-managed dispatch thread(s), concurrently with the IARM event thread (`iarmEventHandler()`), the worker thread (`task_execution_thread()`, `m_thread`), and the SIGEV_THREAD callback (`timerThreadCallback()` -> `handleTaskTimeout()`).
- Each piece of shared state has exactly one dedicated mutex:
  - `m_callMutex`: g_currentMode/g_triggerMode, and serializes the task-execution loop.
  - `m_statusMutex`: m_notify_status, g_task_status, critical/reboot/unsolicited flags, and m_workerJoinInProgress.
  - `m_networkEventMutex`: g_listen_to_nwevents across worker and network-event callbacks.
  - `m_taskMapMutex`: m_task_map, read/written from API, IARM-event, worker, and timer paths.
  - `m_abortFlagMutex`: m_abort_flag.
  - `m_waiMutex`: g_listen_to_deviceContextUpdate.
  - `m_maintenanceTypeMutex`: g_maintenance_type.
  - `m_currentTaskMutex`: currentTask, written by the worker thread and snapshotted when arming a timer.
  - `m_threadMutex`: assignment, joinability checks, and joins of m_thread.
  - `m_timerCallbackMutex`: timer-context registration, globally unique generation allocation, shutdown state, and in-flight callback accounting.
- Lock ordering: `m_statusMutex` is always acquired before `m_callMutex` when both are needed (see startMaintenance()); the worker thread never holds both simultaneously (it releases `m_callMutex` around any `m_statusMutex` acquisition), avoiding a lock-order inversion between API calls and the worker loop.
- IARM completion derives final status and marks the join transition under `m_statusMutex`, releases it, joins the worker, then reacquires it to publish final status and clear the transition; stopMaintenance follows the same ordering.

- Mixed mutex/condition-variable design implies potential contention points between API calls and event handling during active maintenance.

## Security and auth behavior

- Plugin acquires SecurityAgent token to create authenticated JSON-RPC links to other plugins.
- Uses Exchange::IAuthService interface for activation status and partner ID set operations.

- Whether external clients calling this plugin are additionally permission-gated by Thunder ACL/security policy is not determined from plugin code alone.
