---
name: grill-me-with-docs
description: Deep educational walkthrough of code and architecture for developers at any level, using relatable analogies and step-by-step progression from fundamentals to advanced behavior.
license: MIT
compatibility: Requires repository documentation and code access.
metadata:
  author: entservices-maintenancemanager
  version: "1.0"
---

Teach the repository from start to finish using docs-first and code-backed explanations.

## Use Cases

- New engineer onboarding
- Domain handoff between teams
- Preparation for maintenance feature work
- Understanding runtime flow before debugging

## Teaching Contract

- First determine user familiarity level (beginner/intermediate/advanced).
- Explain concepts with analogies mapped to the actual architecture.
- Move from high-level context to concrete function-level behavior.
- Check understanding before moving to deeper layers.

## Source of Truth Order

1. OpenSpec artifacts
2. Repository docs
3. Code and tests

If docs and code disagree, call it out and explain both.

## Walkthrough Structure

1. Big picture (what this system does)
2. Runtime hosting and lifecycle
3. Public interfaces and consumers
4. Internal orchestration and state transitions
5. Failure modes and recovery behavior
6. Test strategy and confidence boundaries
7. Practical developer playbook (where to start, what to avoid)

## Recommended Explanation Style

- Use simple analogies first, then exact terms.
- Use diagrams or sequence narratives when helpful.
- Link each explanation to specific files and symbols.
- Summarize key takeaways at each stage.

## Output Template

- Audience level and assumptions
- Architecture walkthrough
- API walkthrough
- Runtime sequence walkthrough
- Edge cases and pitfalls
- What to read next
- Quick self-check questions

## Guardrails

- Do not oversimplify away critical runtime constraints.
- Keep explanations faithful to code-truth.
- Keep OpenSpec and docs as primary references.
