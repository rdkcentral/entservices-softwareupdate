# Custom Agents and Skills Guide

This repository defines custom Copilot agents and skills to support an OpenSpec-first engineering lifecycle.

- Skills reference: [../skills/README.md](../skills/README.md)
- OpenSpec directory guide: [../../openspec/README.md](../../openspec/README.md)

## Source of Truth Policy

All customizations in this folder and under .github/skills must follow this precedence:

1. OpenSpec artifacts in openspec/
2. Repository documentation
3. Code and tests
4. User assumptions

If any conflict is found between these layers, agents and skills must report the conflict and recommend the update path.

## Custom Agents

## 1) Maintenance Architect Agent

- File: .github/agents/maintenance-architect.agent.md
- Purpose:
  - Architecture/code/documentation review
  - Pair-programming advisor for planning and design choices
  - Feature planning with OpenSpec lifecycle enforcement

### Typical Use Cases

- Planning a new maintenance workflow feature
- Reviewing design risks before implementation
- Auditing architecture drift between OpenSpec, docs, and code
- Advising on low-risk incremental migration strategy

### How to Use

- Select this agent when you need guidance and review-first output.
- Ask for one of these output modes:
  - architecture review report
  - design option comparison
  - OpenSpec alignment report
  - documentation drift report

### Expected Outputs

- Findings with severity and impact
- Evidence paths
- Recommended actions
- OpenSpec/doc sync recommendations

## 2) Maintenance Code Implementer

- File: .github/agents/maintenance-code-implementer.agent.md
- Purpose:
  - Diagnose defects and implement fixes/features
  - Keep code changes regression-safe and standardized
  - Keep OpenSpec tasks and documentation updated

### Typical Use Cases

- Defect triage from logs and traces
- Fix implementation for maintenance lifecycle issues
- Feature implementation with OpenSpec task alignment
- Post-fix validation and documentation update

### How to Use

- Select this agent when code changes or concrete defect handling is needed.
- Provide available evidence (logs, errors, repro steps).
- Ask explicitly for:
  - root cause analysis
  - minimal-risk patch plan
  - validation checklist
  - OpenSpec and docs update summary

## Custom Skills

## 1) diagnose

- File: .github/skills/diagnose/SKILL.md
- Purpose:
  - Structured diagnosis from logs/symptoms with OpenSpec-first alignment

### Typical Use Cases

- Runtime incident analysis
- Regression triage
- Event-order and state-transition debugging
- Timeout/abort behavior diagnosis

### How to Use

- Invoke skill and provide any available runtime evidence.
- The skill will produce:
  - problem summary
  - evidence-based analysis
  - root-cause hypothesis with confidence
  - remediation options
  - validation plan
  - OpenSpec/docs sync actions

## 2) grill-me-with-docs

- File: .github/skills/grill-me-with-docs/SKILL.md
- Purpose:
  - Deep teaching walkthrough for onboarding and knowledge transfer

### Typical Use Cases

- New engineer onboarding
- Explaining MaintenanceManager architecture end-to-end
- Preparing engineers for feature or incident work

### How to Use

- Invoke skill and specify audience level:
  - beginner
  - intermediate
  - advanced
- Optionally ask for a focused walkthrough area:
  - lifecycle
  - API
  - IARMBus integration
  - task orchestration
  - persistence/state

### Expected Outputs

- Layered explanation with analogies
- Code/doc references
- Runtime flow narrative
- Pitfalls and self-check questions

## Keeping in Sync with OpenSpec

For any feature/fix workflow:

1. Start with relevant OpenSpec change.
2. Use architect agent for planning/review.
3. Use implementer agent for execution and validation.
4. Use diagnose skill for incident depth.
5. Use grill-me-with-docs for onboarding or handoff.
6. Update OpenSpec tasks/specs and repository docs before closing work.

## Recommended Team Workflow

- Discovery and planning:
  - Maintenance Architect Agent
  - openspec-propose
  - openspec-explore
- Implementation:
  - Maintenance Code Implementer
  - openspec-apply-change
- Validation and closure:
  - diagnose
  - openspec-archive-change

## Quick Invocation Prompts

- Architect:
  - "Review this feature design against current MaintenanceManager runtime architecture and identify risks."
- Implementer:
  - "Diagnose this maintenance failure from logs and implement the minimal-risk fix with OpenSpec updates."
- diagnose skill:
  - "Run diagnose on these logs and provide root cause, confidence, and validation steps."
- grill-me-with-docs skill:
  - "Teach me MaintenanceManager like I am a new engineer, with analogies and code references."
