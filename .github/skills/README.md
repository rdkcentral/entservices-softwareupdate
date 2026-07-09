# Skills Documentation

This document explains all workspace skills with a focus on OpenSpec lifecycle usage.

## Source of Truth

Use this precedence for all skill outputs:

1. OpenSpec artifacts in openspec/
2. Repository documentation
3. Code and tests
4. Assumptions

If conflict exists, report it and propose which layer must be updated.

## OpenSpec Skills

## 1) openspec-propose

- File: .github/skills/openspec-propose/SKILL.md
- Purpose: create a new OpenSpec change and generate core artifacts.

### When to Use

- Starting a new feature, fix, or behavior change
- Converting rough ideas into a structured change plan

### Expected Outputs

- Change directory under openspec/changes/<change-name>/
- proposal.md
- design.md
- tasks.md

### How to Use

- Ask for a new change by name or provide a short problem statement.
- Example prompts:
  - "Use openspec-propose for add-maintenance-retry-policy"
  - "Propose a change to harden IARM timeout recovery"

## 2) openspec-explore

- File: .github/skills/openspec-explore/SKILL.md
- Purpose: explore design ideas and requirements without implementation.

### When to Use

- Clarifying scope before coding
- Comparing architecture options
- Surfacing risks and unknowns

### How to Use

- Ask exploratory questions and request tradeoff analysis.
- Example prompts:
  - "Use openspec-explore to compare event-driven vs polling recovery"
  - "Explore risks in stopMaintenance abort semantics"

## 3) openspec-apply-change

- File: .github/skills/openspec-apply-change/SKILL.md
- Purpose: implement tasks from an OpenSpec change in sequence.

### When to Use

- After tasks are ready and implementation should begin
- During iterative delivery of remaining tasks

### How to Use

- Provide the change name if known.
- Example prompts:
  - "Use openspec-apply-change for harden-maintenance-timeout"
  - "Continue applying the active OpenSpec change"

### Expected Outputs

- Code/doc updates tied to tasks
- Task checkbox updates in tasks.md
- Progress summary and blockers if any

## 4) openspec-archive-change

- File: .github/skills/openspec-archive-change/SKILL.md
- Purpose: archive a completed OpenSpec change with final checks.

### When to Use

- All implementation tasks are complete
- Change is ready to close and preserve

### How to Use

- Invoke with change name or select from active changes.
- Example prompts:
  - "Use openspec-archive-change for harden-maintenance-timeout"
  - "Archive the completed active OpenSpec change"

## Supporting Skills in This Repo

## diagnose

- File: .github/skills/diagnose/SKILL.md
- Purpose: evidence-based diagnosis from logs/symptoms with OpenSpec alignment.

## grill-me-with-docs

- File: .github/skills/grill-me-with-docs/SKILL.md
- Purpose: onboarding and deep walkthrough using docs-first explanation.

## Recommended End-to-End Workflow

1. Plan with openspec-propose
2. Refine with openspec-explore
3. Implement with openspec-apply-change
4. Diagnose issues using diagnose when needed
5. Educate/handoff using grill-me-with-docs
6. Close with openspec-archive-change

## Quality Checklist for Skill Usage

- OpenSpec artifacts referenced before implementation
- Documentation updates included where behavior changed
- Unknowns captured in openspec/unknowns when unresolved
- Change status is visible and auditable
