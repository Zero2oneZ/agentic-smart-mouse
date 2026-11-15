# Rust core documentation

These documents describe the implementation in `core/` and the agreed direction
for the larger system. **Implemented** means code exists in this crate. **Target**
means an intended capability with no claim of runtime availability. The Qt app and Rust helper use separate databases; a bounded bridge connects
the fixed local notes recipe. General orchestration adapters remain separate.

| Start here | Contents |
| --- | --- |
| [Build ledger API](BUILD_LEDGER_API.md) | Implemented SQL roadmap, versioned documents, scoped evidence and checkpoint/subtree forks |
| [Build roadmap](BUILD_ROADMAP.md) | Ordered delivery stages, dependencies, runtime/data ownership and acceptance gates |
| [Planned build swarm](PLANNED_SWARM_AGENTS.md) | Nine disabled/planned roles, capability boundaries, handoffs and parent-budget rules |
| [Whole-product release review](../../docs/PRODUCT_REVIEW.md) | Consolidated user-facing priorities, clean UI, deterministic setup and supervised background work |
| [Connected local tasks](LOCAL_TASKS.md) | Notes-to-brief UI, durable checkpoints, private results, controls and CLI operations |
| [Core README](../README.md) | Scope, prerequisites, build commands and quick orientation |
| [Architecture](ARCHITECTURE.md) | Current components, data ownership, graphs, context routing and target integrations |
| [System model and catalog](SYSTEM_MODEL.md) | Default categories, IDs, connectivity, agents/harnesses, logs and future ingestion/data engineering |
| [Portable environments](ENVIRONMENTS.md) | Local/cloud plans, capability backpacks, distinct sandbox backends and the ingestion boundary |
| [Contracts](CONTRACTS.md) | Template validation, limits, state transitions, leases, hashes and retry behavior |
| [CLI reference](CLI.md) | Every command, request and response shape, plus an executable walkthrough |
| [Security and governance](SECURITY.md) | Delegation rules, revocation, trust boundaries and sharing requirements |
| [Operations](OPERATIONS.md) | Local setup, database handling, backup, recovery and troubleshooting |
| [Development and tests](DEVELOPMENT.md) | Source map, verification commands, coverage and extension requirements |
| [Decision record](DECISIONS.md) | Settled design choices, deferred choices and questions still requiring a decision |
| [Agents category and builder plan](AGENT_BUILDER_PLAN.md) | Contextual + flow, mandatory requirements, skills/tasks, compute, RAG, schedules and reports |
| [Swarm catalog and placement plan](SWARM_CATALOG_PLAN.md) | Global reusable specialists, placement/roles, bounded teams and personal runtime outcomes |
| [Orchestration loop planner](ORCHESTRATION_LOOP_PLANNER.md) | Initial planning, event-driven decisions, bounded replanning, verification and stopping |
| [Implementation plan](PLAN.md) | Ordered increments and acceptance criteria for the remaining work |

For the running desktop app, see the [project README](../../README.md). For code
behavior, the authority is [model.rs](../src/model.rs), [engine.rs](../src/engine.rs),
[permissions.rs](../src/permissions.rs) and their [tests](../tests/kernel.rs).

Suggested reading paths:

- **Run the local fixture:** Core README → CLI walkthrough → Operations.
- **Implement another deterministic component:** Architecture → Contracts → Development → Plan.
- **Design group sharing:** Security → Decision record → Plan increments 2 and 3.
- **Connect agents later:** Contracts → Security → Plan increment 5. PTC is not implemented.

Update the relevant contract and tests together when behavior changes. Preserve
the distinction between executable fixtures, real builds and proposed services.
