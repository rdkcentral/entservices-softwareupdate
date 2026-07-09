# Unknowns and Manual Validation Checklist

## High-priority unknowns requiring on-device validation

1. Task/event contract integrity
- Unknown: Exact mapping guarantees between each script/task executable and emitted IARM status sequence.
- Validate: Run full maintenance cycle and capture IARM events with timestamps; verify one-to-one task completion/error signaling and ordering.

2. Timeout behavior under real task hangs
- Unknown: Reliability of SIGALRM timer and handler under production load and signal-mask context.
- Validate: Induce controlled hangs in each task and verify timeout handling, status bit updates, and cycle finalization.

3. Abort semantics for SIGUSR1
- Unknown: Whether rfcMgr and rdkvfwupgrader always implement graceful SIGUSR1 handling.
- Validate: Trigger stopMaintenance during each active task and confirm process exit mode, cleanup quality, and event side effects.

4. Security and permission boundaries
- Unknown: Client authz requirements for invoking MaintenanceManager methods in deployed Thunder security policy.
- Validate: Exercise APIs from authorized/unauthorized clients and verify expected allow/deny behavior.

5. WhoAmI/SecManager race paths
- Unknown: Behavior when SecManager activation/context update is delayed or never delivered.
- Validate: Boot and runtime scenarios with delayed SecManager availability and missing device context update event.

6. Network subscription recovery
- Unknown: Robustness of onInternetStatusChange subscription lifecycle across plugin reloads and network plugin restart.
- Validate: Restart Network plugin and verify re-subscription and resumed critical-task trigger logic.

## Medium-priority unknowns

1. Persisted file integrity
- Unknown: Behavior with corrupted /opt/maintenance_mgr_record.conf entries beyond optOut validation path.
- Validate: Inject malformed persisted values and verify API behavior and recovery.

2. Clock/timezone edge cases
- Unknown: Full matrix behavior for timezone/localtime parsing across all device variants.
- Validate: Device matrix with different /etc/device.properties values and timezone data states.

3. Multi-client concurrency
- Unknown: User-visible behavior when multiple clients issue mode/start/stop calls in close succession.
- Validate: Concurrent API fuzzing with thread-safe and timing assertions.

## Test-derived discrepancies to reconcile

1. IARM event coverage mismatch
- Observation: L1 test setup expects registration of IARM_BUS_DCM_NEW_START_TIME_EVENT in addition to maintenance update event.
- Unknown: Whether this reflects legacy behavior, test drift, or conditional compile path not active in current source.
- Action: Reconcile with product requirements and either update tests or implementation.

2. L2 environmental assumptions
- Observation: L2 tests modify real paths (/etc/device.properties, /opt/rdk_maintenance.conf) and depend on activated companion services.
- Unknown: Whether these assumptions hold in CI/device images used for release gating.
- Action: Document required L2 harness preconditions and isolate side effects.

## Suggested validation evidence package

- Thunder logs for MaintenanceManager with timestamps
- IARMBus event trace for full cycle
- Process table snapshots during start/stop
- RFC read/write audit values for device context fields
- JSON-RPC request/response transcript for all methods and notification stream
- Reboot-required and critical-update scenario capture
