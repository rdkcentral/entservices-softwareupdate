---
description: Create an OpenSpec change and produce required artifacts in one pass
---

Provenance note:
- This prompt is maintained locally for this repository workflow.
- Behavior aligns with OpenSpec lifecycle usage in this codebase.

Create a new OpenSpec change and generate the artifacts needed to begin implementation.

Primary deliverables:
- proposal.md for intent and scope
- design.md for technical approach
- tasks.md for execution checklist

After this flow, implementation should be started with /opsx:apply.

Input handling:
- The argument after /opsx:propose may be either:
  - a kebab-case change id, or
  - a natural-language feature/fix description

Execution sequence:

1. Resolve change intent
- If the user gave no input, use AskUserQuestion (freeform) and ask what they want to build or fix.
- Convert the answer to kebab-case when needed.
- Do not proceed if intent is still ambiguous.

2. Initialize the change
```bash
openspec new change "<name>"
```
- This creates openspec/changes/<name>/ with scaffold files.

3. Inspect artifact graph
```bash
openspec status --change "<name>" --json
```
- Read artifact status and dependency order.
- Capture applyRequires so completion is measured correctly.

4. Generate artifacts by dependency order
- Track progress with TodoWrite.
- For each artifact that is currently ready:
```bash
openspec instructions <artifact-id> --change "<name>" --json
```
- Use the returned template for structure.
- Use context and rules only as constraints, never as copied output.
- Read completed dependency artifacts before drafting the current one.
- Write the artifact to outputPath.
- Report concise progress (example: completed design artifact).

5. Re-check readiness after each artifact
```bash
openspec status --change "<name>" --json
```
- Continue until every artifact listed in applyRequires is done.
- If details are missing for a high-quality artifact, ask the user one focused question and continue.

6. Print final status
```bash
openspec status --change "<name>"
```

Completion summary must include:
- Change name and path
- Artifacts created
- Confirmation that apply prerequisites are complete
- Next action: run /opsx:apply

Quality checks:
- Honor artifact-specific guidance from openspec instructions.
- Keep artifact content aligned with completed dependencies.
- Confirm files were written before moving forward.
- If change id already exists, ask whether to continue the existing change or create a different name.
