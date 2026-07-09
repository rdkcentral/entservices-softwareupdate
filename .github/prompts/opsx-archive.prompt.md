---
description: Move a completed OpenSpec change into archive with safety checks
---

Provenance note:
- This prompt is repository-local guidance for OpenSpec operations.
- It is tailored for the archive flow used by this project.

Archive an OpenSpec change after validating completion state and sync status.

Input:
- /opsx:archive <change-name> is optional.
- If missing or unclear, require explicit user selection from active changes.

Archive procedure:

1. Select change
- If name is not provided, run:
```bash
openspec list --json
```
- Present active (non-archived) choices with AskUserQuestion.
- Do not auto-pick a change.

2. Check artifact completion
```bash
openspec status --change "<name>" --json
```
- Read schemaName and artifact status values.
- If any artifact is not done, show a warning and ask whether to continue.

3. Check task checklist completion
- Read the change tasks file (typically tasks.md).
- Count unchecked items (- [ ]) and checked items (- [x]).
- If unfinished tasks exist, warn and require confirmation before continuing.
- If no tasks file exists, continue without this warning.

4. Evaluate delta spec sync
- Look for delta specs at openspec/changes/<name>/specs/.
- If none exist, continue without sync operations.
- If present, compare against corresponding main specs in openspec/specs/.
- Summarize adds, edits, removals, and renames before prompting.
- Offer choices based on state:
  - pending sync: Sync now (recommended) or Archive without syncing
  - already aligned: Archive now, Sync anyway, or Cancel
- If user requests sync, invoke the sync skill and continue the archive flow afterward.

5. Archive the change directory
```bash
mkdir -p openspec/changes/archive
```
- Build destination name as YYYY-MM-DD-<name>.
- If destination already exists, stop with a clear error and recovery options.
- Otherwise move:
```bash
mv openspec/changes/<name> openspec/changes/archive/YYYY-MM-DD-<name>
```

6. Print archive result summary
- Include change name, schema name, archive path, and spec sync status.
- Include warnings if artifacts/tasks were incomplete or sync was skipped.

Success format example:
## Archive Complete

Change: <change-name>
Schema: <schema-name>
Archived to: openspec/changes/archive/YYYY-MM-DD-<name>/
Specs: synced to main specs

Warning example:
## Archive Complete (with warnings)

Change: <change-name>
Schema: <schema-name>
Archived to: openspec/changes/archive/YYYY-MM-DD-<name>/
Specs: sync skipped

Warnings:
- incomplete artifacts were present
- incomplete tasks were present

Failure example (target exists):
## Archive Failed

Change: <change-name>
Target: openspec/changes/archive/YYYY-MM-DD-<name>/
Reason: target directory already exists

Options:
1. Rename existing archive
2. Remove duplicate archive if appropriate
3. Retry later with a new date

Rules:
- Require explicit selection when input is missing.
- Use openspec status JSON as the source of completion truth.
- Warnings do not block archive if the user confirms.
- Keep all change files together when moving, including hidden metadata.
