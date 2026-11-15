# Global specialist catalog and personal swarm execution

## Scope

Planning extension to [the agent builder](AGENT_BUILDER_PLAN.md). It includes a
concrete proposed [placement system prompt](../templates/prompts/agent-placement-v1.md).
The prompt is not wired into Qt or a provider. No swarm dispatcher, global catalog
service, rank evaluator or runtime partition migration is implemented here.

## Product model

Every Hotbox **process-launching action** resolves through one orchestration entry
point. A workflow composes the specialists needed for its task: planning, design,
SQL/data modeling, roadmap, coding, image/video generation, ingestion, organization,
verification or reporting. This includes the Agent Builder itself: it may use a
bounded authoring team while producing one saved draft and reviewable definition.

Menu navigation, opening the builder and viewing reports are UI operations and do
not automatically launch models or workers. A simple folder-opening operation can
be a one-step deterministic workflow. The shared dispatch contract must not require
a costly multi-model team for trivial actions. The task determines composition.

```text
HOTBOX PROCESS ACTION
         |
Task intent + authorized catalog + selected personal environment
         |
Placement/composition proposal -> deterministic eligibility checks
         |
Pinned specialist graph + scope + budgets + output destinations
         |
  Planner -> Designer / SQL / Roadmap
                         |
                   Coder / Image / Video
                         |
                 Independent verifier
                         |
                 Organizer + Reporter
                         |
Personal/project environment: artifacts, outcomes, memory, logs, checkpoints
```

The illustrated specialists are optional branches, not mandatory stages for every
task. SQL and generation paths require the corresponding real adapters and input
policy. The existing Rust fixture stage contract is not renamed into this graph;
production task templates require explicit schema/policy support.

## Global catalog versus localized execution

**Global catalog** means a deployment-wide registry of reusable, versioned agent,
skill, task-template, prompt-template, harness, backpack and swarm-recipe definitions.
It is permission-filtered by owner/group/publication policy. It does not mean all
users share a database full of each other's conversations or generated work.
Global uniqueness and catalog discovery do not grant runtime data access.

| Global definition side | Personal/project environment side |
| --- | --- |
| Stable definition/version IDs and source references | Actual task request and contextual instructions |
| Reusable roles, skills and task contracts | Assigned coordination roles and local fit decisions |
| Generic reviewed system prompt templates | Private prompt customizations and rendered provider context |
| Harness/backpack/environment requirements | Local credentials, compute sessions and resolved data bindings |
| Swarm recipes and typed handoff contracts | Runs, operations, attempts and checkpoint state |
| Category taxonomy and reference edges | Personal views, placement overrides and private labels |
| Synthetic/publication-approved evaluation metadata | Personal task outcomes, generations, usage and error histories |
| Declared licensing and contribution terms | Personal reports, memory, RAG chunks/indexes and logs |

All generations—including plans, roadmaps, SQL, code, images and video—are personal
runtime outputs by default. Publishing reusable code or an improved agent/skill is
a separate scoped contribution action with explicit selected content, provenance,
review and licensing checks. Do not learn global ranks from personal outcomes or
upload private generations by default. Aggregated diagnostics require a separate
opt-in data policy; aggregating does not automatically remove sensitive information.

A private draft or customization stays private until explicit catalog publication.
The global catalog may index the existence of an authorized private definition for
its owner, but must not reveal titles, counts or dependencies to unauthorized users.
No central task history is required to make definitions reusable.

The personal environment is a logical ownership boundary: it may be local storage,
a dedicated VM or the user's cloud namespace. Moving compute does not change data
ownership. Cloud execution resolves destination-local bindings and approved data
transfers; it never exports the entire belongings tree with a catalog push.

Proposed layout:

```text
GLOBAL DEFINITIONS /                       PERSONAL ENVIRONMENT /
  agents/<agent-id>/                          registry/     private run SQL
  skills/<skill-id>/                          requests/
  tasks/<template-id>/                        runs/<run-id>/
  prompts/<prompt-id>/                          plans/ designs/ roadmaps/
  harnesses/<harness-id>/                       sql/ code/ images/ video/
  backpacks/<backpack-id>/                      handoffs/ outcomes/ reports/
  swarms/<recipe-id>/                           logs/ checkpoints/
  taxonomy/                                  agents/<agent-id>/memory/
                                             artifacts/<artifact-id>/
                                             indexes/<index-id>/
```

These are target lazy layouts, not directories created by this document. Definition
SQL and runtime SQL are separate ownership domains; their physical deployment may
be separate databases or scoped services. Agents never mount a whole catalog or
runtime registry database. Local SQL remains private; cloud scale uses an explicit
transactional service adapter, not a multi-host shared SQLite file. Runtime records
pin catalog IDs/versions/digests, with authorized offline definition snapshots when
needed. Catalog services need no private run results to resolve those references.

## Placement, coordination role, fit and authority

Keep four decisions separate:

1. **Placement:** primary purpose/category, optional secondary references and
   personal display choices. This changes navigation, not access.
2. **Coordination role:** coordinator/planner/specialist/reviewer/organizer/reporter
   within a specific run. This defines responsibilities and handoffs, not privilege.
3. **Fitness:** evidence-backed task suitability after eligibility gates. Evaluate
   input/output match, skills, tool availability, permitted scope, environment,
   verified reliability and budget. Unknown evidence stays unknown.
4. **Authority:** current resource/action grants and enforced harness/backend policy.
   This comes from the trusted host and cannot be assigned by a placement prompt.

Use explicit criteria rather than an unexplained universal numeric rank. If a UI
needs a sortable score, pin a scoring-policy revision, publish component weights and
evidence, and mark it task-specific. A failed scope, environment, dependency or
budget gate makes the candidate ineligible regardless of score. Personal outcome
history can improve local selection under its owner's policy; only approved
synthetic/shared evaluations contribute to global published capability metadata.

Readiness (draft, contract-checked, simulation-tested, runtime-verified, retired)
is also distinct from coordination role and fit. Simulation success cannot certify
real SQL access, image/video generation, OS isolation or external side effects.
A coordinator cannot promote itself, approve its own outputs, widen a grant or
reclassify private outcomes into global evidence.

## System prompt and builder hooks

Add the placement prompt as a versioned dependency of the builder/composer harness,
not a new clause silently appended to all workspace chat prompts. Supply only the
catalog entries the current principal can discover and the task input the selected
environment permits. The model returns a proposed or blocked structured composition.
It never returns authoritative approved/running state.

Extend the builder sequence:

```text
save private draft
  -> propose category + run role + evidence-based fit
  -> select minimal eligible specialist team
  -> pin versions and typed handoffs
  -> declare personal runtime output destinations
  -> check grants / harness / backend / budget / independence
  -> evaluate synthetic task
  -> review exact revision
  -> publish reusable definition only
  -> instantiate private run explicitly
```

Required new records: composition proposal/revision, specialist assignment,
coordination role, selection-policy/evidence refs, handoff contract, recipe version,
output destination and optional local override. Introduce them with an explicit
catalog/schema revision; do not repoint system-catalog-v1 IDs or conflate an agent
version with a session/attempt. Fit reasoning is a short decision summary and
evidence references, not raw private model chain-of-thought.

Prompt output needs a typed core schema with unknown-field rejection, bounded
members/handoffs, known references and immutable dependency hashes. The prompt
alone enforces none of this. Core checks current scope/action grants, dependency
compatibility, permitted endpoints and artifact bindings, verifier independence,
per-run concurrency and total budgets, then records the reviewed decision.

## Swarm execution and handoffs

The parent run owns one total budget and cancellation domain. Child operations
reserve from it; delegated specialists cannot reset cost/time limits by spawning
new runs. Pin maximum members, parallelism, depth, iterations and report-trigger
hops. Reject cycles in the within-iteration dependency graph. Iterative redesign is
an explicit bounded continuation with new evidence and checkpoints.

Each assignment has agent/version, role, task/skill/harness/backpack refs, scope,
input artifact refs, output contract, acceptance checks and one private environment
binding. Handoffs transfer immutable artifact references and concise summaries,
not shared mutable transcripts or an unrestricted filesystem. Producer and consumer
both require live access. Admission rules still apply to untrusted external input.

Run child operations in private instances on a scoped pool by default. Shared
compute does not imply shared credentials, scratch, memory or RAG. A specialist
receives the minimal relevant task context; SQL helpers do not automatically see
image-generation prompts, and media organizers do not automatically access database
credentials. Assign dedicated environments or microVMs according to workload policy.

A required reviewer is independent from the producing assignment and cannot merely
repeat its assertion. High-risk outputs need deterministic contract tests or a
separately authorized verifier, not only another model's agreement. Failures produce
local evidence and either a bounded retry/replan or a blocked result. Resume checks
current grants and expired leases. Final outcomes and reports remain personal.

## Migration and acceptance additions

Extend the existing agent-builder milestones with:

- Idempotent action-to-workflow routing; navigation/+ does not run a swarm.
- Core placement/composition schema and validation before any model-assisted rank.
- Global definition versus private runtime partition, with private-draft publication.
- Permission-filtered catalog discovery and no leakage through names/counts/scores.
- Pinned recipe/agent/skill/prompt/harness versions across restart and offline use.
- Unknown/failed eligibility gates, forged role/score, unavailable generation/SQL
  adapters and self-review attempts are rejected without changing authority.
- Bound total child budgets, fan-out/depth, loop count and event-trigger recursion.
- Cross-specialist handoff authorization and cancellation/revocation during work.
- No personal task outcomes, generations, prompts, memory or logs in catalog export.
- Separate explicit contribution action for selected reusable source improvements.
- Initial end-to-end fixture: planner + deterministic summarizer + independent
  check + organizer/report, using synthetic local inputs and private outputs.

Documentation/template changes only in this increment. No prompt is installed in
live chats and no current Hotbox command is rerouted until these adapters and tests
are implemented.


## Orchestration loop planner

The [loop-planner plan](ORCHESTRATION_LOOP_PLANNER.md) and proposed
[system prompt](../templates/prompts/orchestration-loop-planner-v1.md) define initial
graph planning, event-driven reassessment, ready-work dispatch proposals, bounded
replanning, waiting, escalation and evidence-based completion. The model planner is
separate from the deterministic controller/scheduler, worker and verifier. Its
actual plan revisions and outcomes stay private; only reusable definitions are global.
