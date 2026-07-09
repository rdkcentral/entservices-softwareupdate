---
name: diagnose
description: Diagnose issues from logs, traces, or runtime symptoms with OpenSpec and repository docs as source of truth. Use when debugging defects, validating incident hypotheses, or preparing a fix plan.
license: MIT
compatibility: Requires repository access and OpenSpec artifacts.
metadata:
  author: entservices-maintenancemanager
  version: "1.0"
---

Diagnose issues using an OpenSpec-first, code-truth workflow.

## Use Cases

- Service failures during maintenance lifecycle
- Unexpected JSON-RPC behavior
- IARMBus event-order or completion-state anomalies
- Timeout, abort, or stuck-thread symptoms
- Regression triage after a recent change

## Inputs

Any subset is acceptable:

- Logs or log snippets
- Error messages
- Reproduction steps
- Suspected commit/change area
- Device/runtime context

## Source of Truth Order

1. OpenSpec artifacts in openspec/
2. Repository docs
3. Code and tests
4. User-provided assumptions

## Workflow

1. Identify scope and affected subsystem.
2. Gather evidence from logs, code, and tests.
3. Reconstruct timeline and state transitions.
4. Map findings against intended behavior from OpenSpec/docs.
5. Classify each statement as Fact, Inference, Assumption, or Unknown.
6. Provide root cause hypothesis with confidence level.
7. Recommend remediation options (minimal-risk first).
8. Propose OpenSpec/documentation updates if drift is found.

## Output Template

- Problem summary
- Impact scope
- Evidence table
- Root cause analysis
- Remediation options
- Recommended fix path
- Validation checklist
- OpenSpec/docs sync actions

## Guardrails

- Do not jump to conclusions without evidence.
- Explicitly separate confirmed behavior from assumptions.
- Keep OpenSpec aligned with diagnosis outcome.
- Prefer reproducible, testable hypotheses.
