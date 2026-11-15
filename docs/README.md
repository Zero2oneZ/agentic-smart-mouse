# Project documentation

The project currently contains a working local Qt desktop app and a separate Rust
orchestration foundation. They have different runtime responsibilities and databases.

| Documentation | Use it for |
| --- | --- |
| [Whole-product review](PRODUCT_REVIEW.md) | First-release priorities, plain-language UI, deterministic setup, headless workers, naming and segmentation |
| [Project README](../README.md) | Desktop installation, activation gesture, chats, model setup, Git, session segments and shared storage |
| [Rust core README](../core/README.md) | Standalone deterministic kernel scope and quick start |
| [Rust documentation index](../core/docs/README.md) | Architecture, contracts, all CLI commands, governance, operations and developer references |
| [Agents builder plan](../core/docs/AGENT_BUILDER_PLAN.md) | Planned Hotbox Agents category, mandatory authoring flow and runtime requirements |
| [Implementation plan](../core/docs/PLAN.md) | Delivery sequence for tool lineage, authentication, isolated execution, PTC and adapters |
| [Design decisions](../core/docs/DECISIONS.md) | Settled requirements and production choices still open |
| [Repository contributor instructions](../AGENTS.md) | Local build/testing and preservation rules |

The Rust fixture workflow does not execute real application builds or control the
desktop. The desktop storage architecture does not create container sandboxes.
Multi-user authentication, remote governance, model-driven orchestration and semantic
retrieval remain planned. Each subsystem's documentation states its actual boundary.
