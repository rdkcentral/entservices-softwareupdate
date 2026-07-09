# entservices-maintenancemanager

This repository contains Comcast enterprise service plugins for RDK device maintenance and package lifecycle operations.

## Included Plugins

- **MaintenanceManager**: Orchestrates maintenance workflows and task lifecycle for RDK devices. Runs as a Thunder plugin exposing JSON-RPC APIs.
- **Packager**: Handles package and update-related operations.

See [MaintenanceManager/README.md](MaintenanceManager/README.md) and [Packager/README.md](Packager/README.md) for detailed plugin documentation.

Default development branch: develop.

## Plugin Documentation

- MaintenanceManager: [MaintenanceManager/README.md](MaintenanceManager/README.md)
- Packager: [Packager/README.md](Packager/README.md)

## Agents and Skills

- Detailed usage guide: [.github/agents/README.md](.github/agents/README.md)
- Skills documentation: [.github/skills/README.md](.github/skills/README.md)
- OpenSpec directory guide: [openspec/README.md](openspec/README.md)

### Custom Agents

- Maintenance Architect Agent: [.github/agents/maintenance-architect.agent.md](.github/agents/maintenance-architect.agent.md)
- Maintenance Code Implementer: [.github/agents/maintenance-code-implementer.agent.md](.github/agents/maintenance-code-implementer.agent.md)

### Custom Skills

- diagnose: [.github/skills/diagnose/SKILL.md](.github/skills/diagnose/SKILL.md)
- grill-me-with-docs: [.github/skills/grill-me-with-docs/SKILL.md](.github/skills/grill-me-with-docs/SKILL.md)
