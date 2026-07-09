# State and Persistence Model

## Runtime state variables

- Mode and trigger:
  - g_currentMode (FOREGROUND/BACKGROUND)
  - g_triggerMode
- Lifecycle/state:
  - m_notify_status
  - g_maintenance_type (SOLICITED/UNSOLICITED)
  - g_unsolicited_complete
  - m_abort_flag
- Outcome flags:
  - g_is_critical_maintenance
  - g_is_reboot_pending
- Task state:
  - g_task_status bitmask
  - m_task_map active-task map
- Integration flags:
  - g_listen_to_nwevents / g_subscribed_for_nwevents
  - g_listen_to_deviceContextUpdate / g_subscribed_for_deviceContextUpdate
  - g_whoami_support_enabled
  - g_suppress_maintenance_enabled

## Persisted state

- cSettings is bound to /opt/maintenance_mgr_record.conf.
- Persisted keys include:
  - softwareoptout
  - background_flag
  - LastSuccessfulCompletionTime
  - LastMaintenanceStatus

- Boot reason marker file used during startup skip decision:
  - /opt/secure/reboot/maintenance_reboot

- Device initialization context values are written as RFC/TR-181 parameters:
  - Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.PartnerName
  - Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.OsClass
  - Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Bootstrap.XconfUrl
- Partner ID is also pushed to AuthService via SetPartnerId.

## Initialization defaults

- Boot path initializes/normalizes:
  - mode to FOREGROUND
  - triggerMode empty
  - notify status to IDLE
  - critical/reboot flags false
  - task status reset

## Bookkeeping semantics

- LastSuccessfulCompletionTime is stored only when all tasks succeed.
- LastMaintenanceStatus is used at boot to determine whether unsolicited execution can be skipped after a maintenance-triggered reboot.
- Reboot pending flag set on MAINT_REBOOT_REQUIRED event and also set true at solicited start path.

- isRebootPending in API is not purely event-derived; solicited flow pre-sets it optimistically.

## Concurrency aspects

- m_callMutex, m_statusMutex, and condition variable interactions protect state mutation/read paths.

- Formal lock ordering policy is not documented, so deadlock freedom depends on implementation discipline and runtime coverage.
