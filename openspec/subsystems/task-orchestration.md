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
- m_task_map tracks whether each task is active/pending completion event.
- task_status_map maps each task command to corresponding completion bit index.

- The worker does not directly inspect child process result after launch success; it relies mainly on IARM events and timeout path.

## Retry and timeout

- One retry is attempted for task invocation failure (TASK_RETRY_COUNT=1).
- Retry delay is TASK_RETRY_DELAY seconds (5).
- Timeout per task defaults to TASK_TIMEOUT=3600 unless compile-time override in CMake.
- Timeout handler marks timed-out task error-complete and notifies worker.

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
