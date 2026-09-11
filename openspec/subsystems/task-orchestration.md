# Task Orchestration Subsystem

## Task inventory and ordering

- Task script root is /lib/rdk.
- Primary task launcher script: /lib/rdk/Start_MaintenanceTasks.sh.
- Task arguments/order:
  1. RFC
  2. SWUPDATE
  3. LOGUPLOAD
- Critical path helper script: /lib/rdk/xconfImageCheck.sh (via startCriticalTasks).

## Execution strategy

- Each task command is executed by system() with trailing background ampersand.
- m_task_map tracks whether each task is active/pending completion event; guarded by m_taskMapMutex (read/written from the worker, IARM-event, and timer threads).
- currentTask holds the name of the in-flight task; guarded by m_currentTaskMutex and snapshotted into each timer arm's immutable callback context.
- task_status_map maps each task command to corresponding completion bit index.

- The worker does not directly inspect child process result after launch success; it relies mainly on IARM events and timeout path.

## Retry and timeout

- One retry is attempted for task invocation failure (TASK_RETRY_COUNT=1).
- Retry delay is TASK_RETRY_DELAY seconds (5).
- Timeout per task defaults to TASK_TIMEOUT=3600 unless compile-time override in CMake.
- Each arm creates an immutable callback context containing its task and generation; callbacks whose context was unregistered or whose generation is stale are ignored.
- A valid timeout marks only its associated task error-complete and notifies the worker.
- Task completion/error paths increment m_taskNotificationGeneration before notification; the worker waits for a generation change or abort, preventing a notification that arrives before wait() from being lost.
- Timer teardown invalidates the active generation and waits for callbacks already counted as in flight before plugin resources are released.

## Abort model

- stopMaintenanceTasks inspects /proc cmdline to locate task PIDs.
- abort signaling policy:
  - rfcMgr: SIGUSR1
  - rdkvfwupgrader: SIGUSR1
  - otherwise: caller/default signal (default SIGABRT)

- SIGUSR1 is interpreted by task binaries as graceful stop path.

## Success and terminal outcome derivation

- g_task_status bitmask fields are updated for success/error/complete/reboot/skipped markers.
- Terminal determination:
  - ALL_TASKS_SUCCESS bits set -> MAINTENANCE_COMPLETE
  - TASKS_COMPLETED set but some success bits missing:
    - MAINTENANCE_TASK_SKIPPED set -> MAINTENANCE_INCOMPLETE
    - else -> MAINTENANCE_ERROR

- Exact contractual mapping between task scripts and emitted IARM statuses is external to this plugin code.
