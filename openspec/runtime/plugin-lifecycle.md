# MaintenanceManager Plugin Lifecycle

## Lifecycle Walkthrough

## 1) Build and registration

- Shared library target built from MaintenanceManager.cpp and Module.cpp.
- CMake sets MODULE_NAME compile definition and installs plugin shared object to lib/<namespace-lower>/plugins.
- Thunder registration macro uses API version constants defined in the implementation.

## 2) Activation and constructor path

- Constructor registers all JSON-RPC methods.
- Constructor initializes task map and device initialization context mapping keys:
  - partnerId -> TR181_PARTNER_ID
  - osClass -> TR181_TARGET_OS_CLASS
  - regionalConfigService -> TR181_XCONFURL

- Constructor performs static capability registration and data map pre-wiring before plugin activation callbacks.

## 3) Initialize()

- Stores and AddRef() IShell pointer.
- Reads WHOAMI_SUPPORT from /etc/device.properties.
- If WhoAmI enabled, subscribes to SecManager device context update event.
- Calls InitializeIARM() when IARM support macros are enabled.
- Reopens timer callback acceptance for plugin reactivation; SIGEV_THREAD does not install or modify a process-wide SIGALRM disposition.
- Returns empty string on success.

- ASSERT(timerid != nullptr) is intended as runtime guard but may rely on platform/compiler behavior for timer_t representation.

## 4) InitializeIARM() and boot trigger

- Calls Utils::IARM::init().
- Registers IARM event handler for maintenance manager update event.
- Immediately triggers maintenanceManagerOnBootup().

### Bootup Initialization Behavior

- Sets initial mode and trigger fields.
- Sets maintenance type UNSOLICITED_MAINTENANCE.
- Validates persisted softwareoptout and normalizes invalid/empty to NONE.
- Evaluates conditional unsolicited skip using persisted LastMaintenanceStatus and maintenance reboot marker.
- If skip condition is met, posts MAINTENANCE_COMPLETE, marks unsolicited-complete, and exits boot flow without creating a worker thread.
- Before evaluating the skip condition, boot path resets runtime flags, posts initial onMaintenanceStatusChange(MAINTENANCE_IDLE), and spawns the worker thread when unsolicited maintenance is not skipped.

- Boot-time maintenance is unsolicited by default, with a conditional short-circuit when prior maintenance completed and reboot reason indicates maintenance reboot.

## 5) Steady-state execution resources

- Runtime uses:
  - worker std::thread m_thread
  - one POSIX timer per task arm, using SIGEV_THREAD with an immutable task name and generation so delayed callbacks cannot be attributed to a later task
  - condition variable task_thread for worker/event coordination
  - ten single-purpose mutexes, each guarding one piece of shared state or lifecycle protocol:
    - m_callMutex: g_currentMode/g_triggerMode; also serializes the task_execution_thread() loop
    - m_waiMutex: g_listen_to_deviceContextUpdate
    - m_statusMutex: m_notify_status, g_task_status, g_is_critical_maintenance, g_is_reboot_pending, g_unsolicited_complete, and m_workerJoinInProgress
    - m_networkEventMutex: g_listen_to_nwevents
    - m_taskMapMutex: m_task_map
    - m_abortFlagMutex: m_abort_flag
    - m_maintenanceTypeMutex: g_maintenance_type (via getMaintenanceType()/setMaintenanceType())
    - m_currentTaskMutex: currentTask (written by task_execution_thread() and snapshotted when a timer is armed)
    - m_threadMutex: assignment, joinability checks, and joins of m_thread
    - m_timerCallbackMutex: callback-context registration, global generation allocation, shutdown state, and in-flight callback accounting
  - Lock ordering: m_statusMutex is acquired before m_callMutex when both are needed; task_execution_thread() explicitly releases m_callMutex around status updates, and terminal paths release m_statusMutex before taking m_threadMutex for a join.
  - Every critical section in the source carries a `// critical section start/end: <mutex>` comment at its lock/unlock or lock_guard scope boundary.

## 6) Deinitialize()

- Marks timer callbacks as shutting down, invalidates the active generation, deletes/unregisters the timer context, and waits for the in-flight callback count to reach zero.
- Queued callbacks that have not acquired their registered context return without dereferencing plugin memory; callbacks already in flight retain shared context ownership and finish before teardown continues.
- Terminal completion/stop sets m_workerJoinInProgress under m_statusMutex, releases the status lock, joins the worker, then publishes final status and clears the transition flag; concurrent start/stop is rejected during the join.
- Calls stopMaintenanceTasks() before IARM deinit (under IARM build).
- Removes IARM event handler and nulls singleton instance in deinit path.
- Releases IShell and IAuthService interface references.

- Deinitialize is intended to be defensive cleanup even during in-progress maintenance.

## Failure and recovery behavior

- Thread creation failures in boot and startMaintenance paths are caught and converted to MAINTENANCE_ERROR or failed RPC response.
- Signal handler registration failure fails Initialize().
- Timer operation failures are logged and can degrade timeout enforcement.

- There is no explicit health watchdog for permanently blocked thread waits if no IARM completion/error event arrives and timer path is disabled/failing.
