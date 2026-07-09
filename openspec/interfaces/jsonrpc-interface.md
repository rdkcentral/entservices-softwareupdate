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

- m_callMutex protects API paths for shared mutable state updates/reads.
- m_statusMutex guards status transitions and interacts with event-thread operations.

- Mixed mutex/condition-variable design implies potential contention points between API calls and event handling during active maintenance.

## Security and auth behavior

- Plugin acquires SecurityAgent token to create authenticated JSON-RPC links to other plugins.
- Uses Exchange::IAuthService interface for activation status and partner ID set operations.

- Whether external clients calling this plugin are additionally permission-gated by Thunder ACL/security policy is not determined from plugin code alone.
