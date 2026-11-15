# Hotbox Agents category and mandatory agent-builder process

## Status and intended outcome

**Plan only.** This change does not add a live menu entry, create agents or VMs,
install schedules, connect a RAG provider, change desktop configuration or enable
model execution. It plans the next product increment using the current Qt UI and
Rust contracts. Agent drafts and authoring should work before real runners exist;
Run/Enable must remain unavailable until their actual prerequisites are verified.

The intended experience is **Hotbox → Agents → + → Agent Builder**. Building an
agent is a versioned, resumable engineering process with mandatory requirements,
not a generic category with a long prompt. Process-launching actions compose
bounded specialist teams from a governed global definition catalog, with all actual
generations and task outcomes in the selected personal environment. See
[the swarm/catalog extension](SWARM_CATALOG_PLAN.md) and proposed
[placement prompt](../templates/prompts/agent-placement-v1.md). Each approved agent joins the same
bounded orchestration loop and is discoverable in a categorized source/file tree.

## Current implementation and integration points

| Current location | What exists | Planned integration |
| --- | --- | --- |
| `src/Hotbox.qml`, `rebuild`/`choose` | Root actions, category children, generic Add category | Agents root, typed navigation, contextual creation routing |
| `src/Main.qml`, `editCategory` and editor | Name/folder/brief dialog | Separate Agent Builder page with saved steps and requirement summary |
| `src/backend.cpp`, `addCategory` | Qt category row and workspace event | Explicit bridge to agent/skill/task IDs; no name-based identity |
| `src/backend.cpp`, `requestContext` | Workspace prompt plus bounded history indexes | Separate builder assistance and runtime prompt assembly |
| `src/backend.h` | QML-facing invokable methods/signals | Bounded named agent-authoring API, not arbitrary command execution |
| `core/src/lineage.rs` | Immutable tool metadata and fork ancestry | Link agent/skill/task versions to tool/source lineage |
| `core/src/environment.rs` | Read-only backpack/environment plans | Compute/storage/network preflight and unresolved requirement display |
| `core/src/engine.rs` | Fixture workflow, leases, budgets and durable checkpoints | Later agent operations through validated adapters |
| `src/segmentstore.cpp`, `src/sessionledger.cpp` | Existing private chat evidence and scoped references | Explicit references/imports into permitted memory/context |

Qt and Rust currently have separate databases. Do not have QML write Rust tables,
copy the Qt database into source, or interpret the current read-only environment
plan as proof that a runner is available. Existing model tools remain read-only.

## Navigation and creation semantics

```text
Hotbox
  Agents
    By purpose
      Research / Monitoring / Ingestion / Engineering / Reporting / Operations
    My agents
    Shared with me
    Drafts
    Runs and reports
    +
      New agent         -> mandatory Agent Builder
      New folder        -> category metadata only
      New skill         -> reusable skill builder contract
      New task template -> reusable task contract
      New workflow      -> composition contract
```

The default landing view is a concise categorized agent tree; other entries are
filters/views over the same IDs, not duplicate definitions. Purpose suggestions
are editable. Unclassified drafts are valid; category choice alone is not a
security or launch requirement. Task/skill/workflow actions can arrive after the
first agent-draft UI, but must be visibly unavailable until their handlers exist.

Create one built-in Agents root using a stable registry key such as
`builtin.agents`, with a unique mapping to a Qt category/view. Seed idempotently;
never assume a numeric category ID or adopt an existing user folder named Agents.
Respect an explicit user-hidden preference across upgrades. Exclude duplicate
rendering when the system root appears among ordinary category children.

Use typed navigation state (`view_kind`, `entity_kind`, `entity_id`, optional parent
reference), rather than adding more negative numeric sentinels to `currentParent`.
The current `-1` Tools route and ordinary category routes need a compatibility
adapter until converted. Activation of `+` releases the overlay and opens the
builder in the main window, retaining the selected folder/project as a suggestion.
It creates or resumes a private draft; it never silently starts a model, worker,
network connection, cron job or VM.

Normal categories keep their existing editor. Agents have explicit actions:
**Open, Edit draft, Versions, Test, Run, Pause, Reports, Fork, Archive**. Each action
shows its actual eligibility. Agent display names can change without moving files
or changing IDs. Dragging an item changes categorization, not ownership or grants.
Folders are navigation; My agents/Shared with me are permission-filtered views.

## Mandatory builder requirements

Each requirement has `unresolved`, `declared`, `verified`, `not_applicable` or
`blocked` state, supporting evidence and a concise explanation. `not_applicable`
requires a reason and is only allowed where the template permits it. A model's
assertion cannot change a requirement to verified. Disabled/none is a deliberate
configuration, not a missing field. Templates populate defaults to avoid requiring
a dozen manual dialogs for a simple agent.

| Requirement | Required declaration | Default / activation gate |
| --- | --- | --- |
| Identity and purpose | Stable agent ID, owner, purpose, category refs, accountable operator; proposed coordination role and fit evidence | Private draft; purpose suggestions cannot change scope |
| Scope and authority | Principal/group/project, who can invoke/edit/administer; requested resource/actions | Minimum explicit grants; current authority checked independently |
| Task contract | Accepted inputs, outputs, success/failure tests, escalation conditions | One bounded manual task first |
| Prompt and behavior | Versioned system instructions, model/provider policy, prompt hash, prohibited actions | Template prompt; no authority inferred from prose |
| Skills and tools | Pinned skill versions, tool schemas, inputs/outputs, capability and licensing requirements | No ambient tool access or floating latest dependencies |
| Harness and loop | Harness/recipe version, specialist handoffs, total swarm budgets, retries, cancellation, independent verifiers | Bounded fixture/simulation until a runner is ready |
| Compute and backpack | Backend, platform, private/pool/dedicated placement, image/rootfs refs, mounts, egress | Private logical instance on a scoped pool; offline where possible |
| Persistence and recovery | Durable checkpoints, state ownership, ephemeral versus retained data, recovery/backup | SQL checkpoints and scoped artifacts, separate from instance disks |
| Memory and RAG | Permitted collections, personal/shared policy, ingestion provenance, retrieval/provider budget | No retrieval unless explicitly enabled; local summaries first |
| Scheduling and triggers | Manual/interval/cron/event mode, timezone, concurrency, misfire/expiry policy | Manual only; schedules disabled |
| Reporting and observability | Local report schema, evidence refs, usage/status, retention, escalation route | Local run summary; external delivery disabled |
| Verification and release | Contract/evaluation results, unresolved items, reviewed revision, approver policy | Immutable release required before Enable; missing runner blocks execution |

A minimal agent can explicitly choose no long-term memory, no RAG, no schedule and
local reporting. It must still declare task limits, permissions, storage/recovery,
harness behavior and verification. Missing compute may allow saving a draft or
checking contracts, but must not produce a misleading Ready/Running state.

## Specialized builder hooks

Hooks are versioned core operations, not arbitrary scripts triggered by clicking
`+`. The interface supplies a draft ID and expected row revision; the trusted host
derives the principal and scope. Hooks produce typed proposals or evidence records.
Models may help suggest classifications, task breakdowns and prompt text, but
cannot approve their own grants, select a stronger identity or bypass validators.

```text
begin_or_resume_draft
    -> propose_category_coordination_role_and_evidence_based_fit
    -> compose_minimal_specialist_team_and_personal_output_bindings
    -> resolve_owner_scope_and_review_requested_permissions
    -> define_task_inputs_outputs_and_acceptance
    -> compile_prompt_and_pin_skills
    -> select_harness_and_plan_environment
    -> declare_state_memory_rag_schedule_and_reporting
    -> validate_contracts_and_detect_conflicts
    -> evaluate_in_fixture_or_attested_test_environment
    -> record_review_of_exact_revision
    -> publish_immutable_agent_release
    -> instantiate_and_enable_explicitly
```

Draft autosave is transactional and resumable. A hook retry uses an idempotency key;
stale UI edits fail with a conflict rather than overwriting newer work. External
preflight results expire and are rechecked when used. Changing a prompt, skill,
harness, scope, network endpoint, schedule, provider policy or input/output contract
creates a new revision and invalidates dependent verification/review. Cosmetic
labels can change separately with history. Unrelated verified requirements need not
be recomputed; dependency hashes determine which evidence is stale.

Review binds the normalized draft, dependency digests, requested permissions,
architecture/policy versions and evaluation evidence. Execution still rechecks live
authority and backend controls. Approval is not permanent access. Human review is
required when the applicable operation/group policy calls for it; routine draft
editing and deterministic contract checks need no extra approval ritual.

Record concise rationale, assumptions and evidence for significant choices. Do not
require raw private chain-of-thought. User classification text, retrieved documents,
logs and model suggestions stay distinguishable from trusted policy.

## First-class entities and filesystem organization

Keep these identities separate:

```text
Agent -> AgentVersion -> AgentRelease
                   |-> PromptRevision
                   |-> SkillVersion references
                   |-> TaskTemplateVersion references
                   |-> HarnessVersion + Backpack digest
                   |-> Memory/RAG/Schedule/Report policy revisions

AgentRelease -> Instance -> Session
                         -> Run -> Task -> Operation -> Attempt
                                                |-> Events / artifacts / reports
```

An agent describes a role. A skill describes a reusable capability implementation
with a contract. A task template describes an objective and acceptance criteria.
A workflow composes tasks and dependencies. A harness controls execution. A model
is a provider configuration, not any of these identities. Reusing a skill references
its pinned version; it does not copy its code, grant authority or share agent memory.

Proposed lazy layout:

```text
SOURCE ROOT /                            PRIVATE BELONGINGS ROOT /
  agents/<agent-id>/                        scopes/<scope-id>/
    agent.json                               agents/<agent-id>/
    prompts/system.md                          state/ memory/ reports/
    skills.lock.json                           instances/<instance-id>/
    tasks.lock.json                              scratch/ checkpoints/ logs/
    evaluations/                             artifacts/<artifact-id>/
  skills/<skill-id>/                          registry/       authoritative SQL
    skill.json src/ tests/                   indexes/<index-id>/  derived RAG
  tasks/<task-template-id>/                   schedules/      private checkpoints
    task.json fixtures/                      secrets/        protected service refs
  workflows/<workflow-id>/
  harnesses/<harness-id>/
  catalog/agents/categories.json
```

The displayed category tree references canonical ID-based locations; an agent can
appear in several permitted views without copied folders. Version manifests pin
source commits; not every historical version needs its own physical source folder.
User files, memory, personal labels, schedules, chats, logs and generated results
stay outside pushed source. Public templates contain neither absolute private paths
nor credentials. Source fixtures contain only synthetic sample data.

Resolve shared libraries and memory through explicit bindings. Do not create this
whole tree on startup. New schema/catalog revisions must preserve current media
bindings and legacy Qt IDs. Renaming or archiving an agent never deletes belongings.
A fork copies eligible definition references and records ancestry; credentials,
private memory, enabled schedules and runtime sessions are not inherited.

## Personal versus shared computers

Default to **private agent state + private per-instance workspace on a scoped worker
pool**. The pool can use a shared host, but agents must not share a writable home,
credential cache, browser profile, terminal history or unrestricted database mount.
Only explicit libraries/corpora are shared. Pool placement is not itself a security
boundary; isolation comes from a validated backend and enforceable bindings.

Offer three deployment policies:

| Policy | Intended use | Requirements |
| --- | --- | --- |
| Ephemeral isolated instance on a pool | Default short tasks | Fresh workspace, bounded scratch, scoped inputs/outputs, cleanup |
| Dedicated long-lived environment | Interactive/stateful agent workstation | Exclusive assignment, idle/lease policy, updates, persistent external state |
| Dedicated microVM/VM | Workload or tenant policy requires guest-kernel separation | Supported host, attested controls, network/device limits and revocation delivery |

A dedicated computer is an optional compute binding, not part of agent identity.
Rebuilding or moving an agent should recover its approved state from private
storage. A shared VM is a host pool unless its policy explicitly gives agents
separate security boundaries; it does not automatically authorize data sharing.
Simulation does not provide OS isolation. Never downgrade to a host shell when a
required backend is unavailable.

Future interactive browser/terminal/desktop adapters operate inside the assigned
guest/session. They must not inject input into the user's active desktop or reuse
personal browser sessions. Remote desktop access is a later adapter, not a side
effect of creating an agent. Local trusted-host operation does not imply secure
remote multi-tenant operation; remote use requires the authenticated gateway.

## Prompt persistence, memory, RAG and logs

Store the agent's system prompt as an immutable reviewed revision with a digest.
An instance uses a pinned prompt/harness version. Runtime memory cannot silently
rewrite the prompt. Proposed instruction changes return to the builder/review flow.
Assemble context in explicit layers: enforced host policy and harness constraints,
agent instructions, task request and permission-filtered evidence. Provider-specific
message roles are adapters; no message role substitutes for external enforcement.

Use four distinct data responsibilities:

| Store | Authority / purpose | Rules |
| --- | --- | --- |
| Operational SQL | Identity, versions, tasks, leases, grants, schedules and status | Durable transactions, scoped queries, migrations and backup |
| Event/trace records | Tool calls, attempts, errors, usage, decisions and transitions | Append with state changes; redact, bound and retain by policy |
| Artifact/blob storage | Documents, terminal output, reports, checkpoints and results | Immutable bytes/digests, provenance and explicit ownership |
| Retrieval index | Searchable chunks/embeddings and summary views | Derived/rebuildable, permission-filtered and revocation-aware |

Do not create a separate physical database for every agent by default. Scoped rows
and artifact bindings provide logical separation; strong tenant isolation may
require separate databases/keys/processes according to deployment policy. Never
mount the whole shared registry inside an agent sandbox. Agents use narrow APIs.

RAG modes: none, approved personal collection, explicitly shared collection, or both.
Every retrieved item carries artifact/version, provenance, scope and citation refs.
Check current access before retrieval, before exporting context to a provider and
before using cached results. Keep cache keys scope/policy aware. External embeddings
and model calls require a declared data-processing policy; no automatic provider
upload. Begin with existing summaries and exact artifact lookup; add embeddings
only after filtered retrieval works. Ingest outside material through the planned
quarantine/admission boundary.

Persistence modes: ephemeral scratch; run/session checkpoints; agent-private memory;
explicit shared project/group memory. Memory writes require a declared destination,
provenance, size/retention limits and write permission. Summaries are derived evidence,
not new privileged instructions. Checkpoint recovery rechecks grants and never
restores stale credentials or reuses expired lease authority.

Reports include run/task/agent release IDs, status, verified outputs, evidence links,
next action or blocker, usage/budget totals and significant decision summaries.
Separate measured tokens/cost from abstract kernel cost reservations. Logs record
code versions, tool calls, network metadata and explicitly captured terminal I/O;
secrets and personal material are redacted before storage. Collection and reporting
skills themselves need contracts, budgets and authorization.

## Orchestrator loop

An approved definition is eligible for orchestration; it is not an always-running
model. A trigger creates a bounded run from pinned versions:

```text
manual / approved schedule / authorized event
             |
resolve release + task + harness + backpack
             |
recheck identity / scope / grants / approvals / budget
             |
reserve run + deduplicate trigger + claim leased operation
             |
assemble permitted context -> execute one declared operation
             |
independent output checks -> checkpoint + event + usage
             |
continue / wait / retry as new attempt / cancel / expire
             |
publish scoped result + local report -> idle
```

All calls carry instance/session/run/task/operation/attempt IDs and trace references.
The loop has hard operation/iteration/time/resource/token/cost limits, bounded
queues, cancellation and lease fencing. Permission revocation blocks later protected
operations and result acceptance; actual runners also need termination/revocation
delivery. A failed verifier cannot be replaced by the agent's narrative.

No skill may bypass the gateway, mint a grant, change its own harness, read arbitrary
files or execute stored scripts merely because it appears in a manifest. Skill
invocations use typed adapters and the same checks as scripted operations. Keep the
existing mandatory workflow stages for development/build releases. Operational
agents use separately versioned workflow contracts; do not label a poll as a fixture
build or invent production executors inside the current eight-stage template.

## Scheduling, events and reporting delivery

Support manual invocation first. Later add interval, cron-style and event triggers
as SQL records owned by an agent/project, rather than writing arbitrary host crontabs
from a prompt. A dedicated scheduler service must work independently of the Qt
window and reconnect to durable state. Until it exists, the UI reports schedules
unavailable, not enabled-but-inert.

Every enabled schedule declares timezone, pinned release/task, concurrency, quota,
expiry, overlap policy, misfire policy and destination/report policy. Default
concurrency is one; missed ticks are skipped with an event, with optional explicit
bounded coalescing. Define nonexistent/ambiguous DST handling and test with a manual
clock. Occurrence IDs are unique across scheduler restarts and duplicate workers.
Changing an enabled release/policy requires an explicit reviewed revision.

An occurrence rechecks current grants/budgets and uses idempotent operations;
revocation disables future protected work. Crashes can produce retries, so reconcile
external side effects instead of assuming exactly-once execution. Deduplicate event
triggers; include causation depth/hop limits and budgets so an agent's own report
cannot recursively trigger unbounded reporting runs.

Local reports are the default. Email, Slack, webhooks and other deliveries are
separate explicitly configured operations with destination-specific permissions,
data limits and failure handling. Do not send anything as a consequence of drafting
an agent. An agent needs an escalation policy, which may simply be a local blocker
visible to its operator.

Pause prevents new claims, suspends new schedule occurrences and requests runner
cancellation according to policy; it records any still-running attempt. Archive
fences new launches and disables schedules before removing the agent from default
views. Retain required history and belongings. Restore returns it disabled and
requires current preflight before activation. Hiding/reorganizing a menu folder
must not silently leave a supposedly archived agent executing; system agents need
a lifecycle-aware archive path instead of the current category-only SQL update.

## Data model, bridge and migrations

Add the generalized scoped entity/version/edge registry and durable draft revisions
before a runtime agent service. Model prompt/skill/task/harness/backpack/release refs,
requirement evidence, evaluation runs, review records, instance bindings, memory/RAG
policies, schedule occurrences and report refs explicitly. Version definitions are
immutable; draft row revisions support optimistic concurrency. Typed edges preserve
containment, dependency and provenance; category placement never grants access.

Keep Rust authoritative for agent definitions and execution state. Qt owns its
existing chats/categories and stores only a mapping to core entities. A bounded
JSON CLI/service bridge exposes named operations such as list/read draft, save draft,
validate, evaluate, publish and request run; no shell command interpolation or direct
QML SQL writes. The local host derives trusted actor context. A remote adapter must
use authenticated identity and the same core operations.

Qt/core writes cannot be one SQLite transaction. Use operation IDs, a durable
outbox/receipt and replayable mapping reconciliation. A crashed UI link must not
create duplicate agents or silently lose a saved draft. Upgrade both schemas
explicitly, test backups and legacy mappings, and keep existing chats untouched.
If the core is absent/incompatible, show an authoring availability error without
pretending a generic category is a functioning agent. No execution is enabled by
schema migration or by seeding the menu entry.

## Delivery milestones and acceptance

| Milestone | Deliverable | Required evidence |
| --- | --- | --- |
| 1. Core authoring contract | Scoped entities, drafts/revisions, requirement validators, immutable references, CLI | Round-trip/restart, stale-edit conflict, missing requirement denial, cross-scope denial and schema upgrade tests |
| 2. Agents menu and builder | Idempotent system category, typed routes, saved builder, categorized views | Offscreen QML tests, existing-category collision/rename/hide behavior, no launch/schedule on +, crash-safe Qt/core mapping |
| 3. Deterministic agent fixture | One task + skill + harness, checkpoint/recovery, local report, explicitly bounded summary lookup | Independent result checks, budgets/revocation/retry tests, no raw script execution or external provider |
| 4. First real environment | One attested runner, private instance state, pinned artifacts, cancellation and limits | Real isolated temporary fixture, blocked network/mount attempts, stale-lease rejection, no user desktop input |
| 5. Governed memory and automation | Scoped RAG/memory writes, durable schedules, report delivery contracts | Retrieval/cache leakage tests, provider-export denial, DST/restart/misfire/overlap/dedup tests, no unauthorized delivery |
| 6. Remote execution and optional dedicated VM | Authenticated worker gateway, scoped pools, shared transactional SQL/blob adapter | Local/cloud protocol equivalence, tenant separation, offline/revocation behavior, backup/restore and supported-backend refusal |

Keep authoring usable with manual-only/no-RAG/no-schedule/local-report defaults.
Do not wait for all VM backends, embeddings or cloud services to ship the Agents
builder. Conversely, publishing a definition does not make absent runtime adapters
available. Show capability and preflight status directly.

Expected implementation files: new `core/src/agent.rs` and registry/approval modules,
CLI commands in `core/src/main.rs`, a new versioned agent-builder template and schema
migration, a bounded `AgentService` adapter for Qt, `src/Agents.qml` and
`src/AgentBuilder.qml`, and updates to `Hotbox.qml`, `Main.qml`, resources,
translations and backend category/entity mapping. Exact APIs follow the reviewed
contract; do not bolt execution onto `addCategory`.

Validation uses temporary databases, directories and repositories, local mock model
endpoints and offscreen Qt rendering. Test all denied/missing prerequisites before
real worker enablement. Source fixture content is synthetic; no runtime data or
credentials enter Git. Reuse `bin/build`, `bin/test` and `core/scripts/check` when
implementation lands. This planning-only increment needs document/link checks,
not a new runtime test run.

## Defaults selected for the first implementation

- Agents root and draft builder; new drafts are private and disabled.
- Reusable pinned skills/task templates; categories are reference views.
- Private per-instance state on a scoped pool; dedicated VM is optional later.
- Manual triggers, no external delivery, no RAG and no network by default.
- Versioned prompt + SQL checkpoints + scoped artifacts; memory and logs are distinct.
- Explicit requirement/evidence review before activation; live permission checks
  remain mandatory even after approval.
- First acceptance agent: summarize a synthetic local artifact into a local report,
  with a deterministic skill/harness fixture and no provider, network or VM required.

These defaults make the first unit reviewable while preserving the complete
requirements for more capable agents. They are planning choices, not changes to
live application settings.


## Global catalog and personal execution clarification

Reusable agent/skill/task/prompt/harness/backpack/swarm definitions live in a governed
global catalog. Actual generations, plans, task outcomes, rendered prompts, memory,
RAG, logs and checkpoints stay in the selected personal/project environment.
Placement and task-specific coordination roles are model proposals validated by the
core; they never confer permissions. See [Swarm catalog plan](SWARM_CATALOG_PLAN.md)
for the system prompt contract, specialist composition and revised acceptance tests.


## Loop-planner dependency

Every process recipe declares a [loop-planner contract](ORCHESTRATION_LOOP_PLANNER.md),
which may be deterministic for a simple task or model-assisted for a larger swarm.
Builder requirements include pinned planner/harness versions, acceptance criteria,
aggregate planning/worker/verifier budgets, retry/replan bounds, wait/wake conditions
and stop rules. The controller validates proposals; the planner cannot execute,
authorize itself or declare verified completion by narrative.
