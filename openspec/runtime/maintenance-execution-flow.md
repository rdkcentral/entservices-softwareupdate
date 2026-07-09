# Maintenance Execution Flow

## Overview

Maintenance execution has two trigger families:

- Unsolicited flow: boot-triggered, starts during plugin initialization path.
- Solicited flow: caller-triggered through startMaintenance JSON-RPC.

Both converge in task_execution_thread() with divergent gating behavior.

## Trigger Paths

## Unsolicited

- Set in maintenanceManagerOnBootup(): g_maintenance_type = UNSOLICITED_MAINTENANCE.
- Boot path evaluates conditional skip before worker creation.
- Skip condition: LastMaintenanceStatus == MAINTENANCE_COMPLETE and maintenance reboot marker is present.
- If skipped, status transitions directly to MAINTENANCE_COMPLETE and g_unsolicited_complete is set true.
- If not skipped, worker starts from bootup path.

- This provides a startup maintenance cycle without explicit northbound request, while avoiding immediate repeat runs after successful maintenance reboot.

## Solicited

- startMaintenance() proceeds only if m_notify_status != MAINTENANCE_STARTED and g_unsolicited_complete is true.
- It resets task status, sets SOLICITED_MAINTENANCE, marks reboot pending true, and starts worker thread.

- Solicited start remains blocked until unsolicited phase is considered complete, including both executed and conditionally skipped unsolicited paths.

## Worker Thread Decision Logic

## Phase 1: pre-start and status handling

- For unsolicited + WhoAmI enabled, MAINTENANCE_STARTED notification may be delayed for power-compliance behavior.
- Otherwise status changes to MAINTENANCE_STARTED immediately.

## Phase 2: network and activation/identity gates

- checkNetwork()/isDeviceOnline() determine internet readiness with retries.
- If WhoAmI disabled and suppress-maintenance enabled, getActivatedStatus() can skip firmware check for activation-connect/ready/not-activated states.
- If WhoAmI enabled and unsolicited, knowWhoAmI() attempts SecManager query and may wait for onDeviceInitializationContextUpdate event before continuing.
- If gating fails with no network, status transitions to MAINTENANCE_ERROR and unsolicited flow may switch to waiting for future network event.

- Control flow is strongly policy-driven around activation and identity readiness, not just raw connectivity.

## Phase 3: task list construction

- Default ordered tasks:
  1. Start_MaintenanceTasks.sh RFC
  2. Start_MaintenanceTasks.sh SWUPDATE
  3. Start_MaintenanceTasks.sh LOGUPLOAD
- If skipFirmwareCheck true in suppress-maintenance path, SWUPDATE is skipped and marked as success+complete bits.

## Phase 4: task execution and waiting

- Tasks are launched via system() with background ampersand.
- Timer starts per task with TASK_TIMEOUT (default 3600 sec unless compile-time override).
- Non-zero task launch status triggers one retry after TASK_RETRY_DELAY, then marks error completion bit for task.
- On successful invocation, worker waits on condition variable for event-handler completion/error signal.

## Phase 5: event-driven completion and finalization

- IARM event handler updates task bits and wakes worker as tasks report complete/error/aborted.
- When TASKS_COMPLETED mask is satisfied:
  - ALL_TASKS_SUCCESS => MAINTENANCE_COMPLETE and LastSuccessfulCompletionTime persisted.
  - MAINTENANCE_TASK_SKIPPED present => MAINTENANCE_INCOMPLETE.
  - otherwise => MAINTENANCE_ERROR.
- Thread is joined at cycle end and unsolicited-complete marker is set for unsolicited path.

## Abort and timeout behavior

- stopMaintenance() routes to stopMaintenanceTasks().
- If status is MAINTENANCE_STARTED, plugin sets abort flag, finds active task process by name in /proc, sends signals (SIGUSR1 for rfcMgr/rdkvfwupgrader, default SIGABRT otherwise), stops timer, notifies worker, joins thread, and sets status MAINTENANCE_ERROR.
- Timer timeout handler marks current task as error-complete and wakes worker.

- Precise semantics of SIGUSR1 handling inside task executables are external and require component-level validation.
