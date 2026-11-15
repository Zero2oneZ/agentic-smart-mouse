# Standalone orchestration implementation plan

## Contract

Keep the core in Rust. SQL is the durable source of state. Every build is segmented
and pinned to a versioned architecture and workflow template. Scripts and agents
use the same validated operations. UI, desktop integration and model providers are
adapters. No product name is assigned by this plan.

The app's current Qt backend remains working during migration. Its architecture,
session and segment stores are integration inputs, not databases to rewrite in
place. This increment adds a separate `core/` crate and no runtime activation.

## Mandatory execution stages

| Stage | Required evidence before advancing |
| --- | --- |
| Discover | Selected tool/version, user intent, authorized tree inventory |
| Document | Relevant architecture, documentation and capability references |
| Plan | Concrete changes, permissions, inputs, outputs, risks and resource budget |
| Prepare | Isolated source branch, declared storage bindings and runner specification |
| Build | Source changes and reproducible build artifacts |
| Verify | Independent contract checks and test results tied to artifact digests |
| Release | Version manifest, source commit, tool lineage and contribution eligibility |
| Activate | Verified release pointer, runtime compatibility and rollback target |

Architecture and policy versions must be pinned alongside the template. Agents
cannot create a new executor, approve their own authority, skip verification or
replace a failed check with a narrative. Human approval is a durable workflow gate
where required by the operation or group policy, not an informal chat message.

## Increment 1: local kernel — implemented here

Versioned JSON templates, validated dependency graph and mandatory stage barriers;
SQLite state and events; exclusive expiring attempts; retries and operation/cost/time
bounds; immutable start requests; deterministic fixture output validation; local
resource grants, bounded delegation and transitive revocation. Scripted tests cover
restart/resume, stale claims, exhausted budgets, cancellation, invalid authority,
revocation and preservation of independent grants.

The fixture stages represent protocol transitions only. They do not perform the
real actions in the evidence table. Cost units are abstract reservations, not token
or money accounting. Caller identity is a trusted library argument, not authentication.

## Increment 2: tool lineage and reviewed plans — registry slice implemented

Implemented: the local registry and CLI persist immutable tool IDs, versions,
source commits, originating-chat references, architecture versions and fork ancestry.
A live source-read grant is checked before reading or forking; the destination must
be a distinct caller-owned resource. Forks copy metadata only. Tests cover independent
edits/access, denied forks, immutable history, restart and schema-1 migration.

Remaining in this increment: reviewed plan revisions, contribution agreements,
actual Git fork/copy operations, and creating/opening focused chats in the UI.
Chat IDs and commit hashes are validated references, not verified external objects.

The complete increment must add immutable tool IDs, source-version references, originating chats, architecture
versions, plan approval records and fork ancestry. Edit opens the originating tool's
focused development context. Fork creates a new identity and focused chat from a
selected version. Git refs, runtime instances, user libraries and grants remain
separate entities. Forks do not inherit credentials or data access automatically.

Pin explicit contribution agreements when sharing a branch. The right to receive
and merge eligible technology is distinct from rights over local group data or
administration. Never transfer another party's private keys. Verify agreement scope
and licensing before upstream contributions are made eligible for merge.

Acceptance: a fork can change independently while its original code, history and
runtime bindings remain unchanged. Changes in template or plan require a new
reviewed revision rather than rewriting an active run.

## Increment 3: authenticated gateway and local governance

Bind people, groups, service accounts and devices to verified principals. Authenticate
each request before selecting an actor, project or group. Implement resource-creation
authority, group roles, explicit onward-sharing bounds, approvals and durable allow/
deny audit records. Separate repository access, application use, runtime administration,
data access and upstream contribution permissions. Extend actions beyond the v1
fixture build vocabulary through an explicit policy/schema migration.

Use revocable per-device credentials and short-lived scoped execution credentials.
SSH handles Git/admin transport; service-to-service identity needs its own protocol.
Design online revocation, offline expiry, clock assumptions and cross-device conflict
handling before enabling remote use. A possession check alone does not prove
authority: recheck current grant ancestry at each protected operation.

Acceptance: Alice → Bob → Carol grants cannot escalate or cross resources. Alice's
revocation blocks dependent future access and active result acceptance, while Carol's
independent direct grant remains usable. Downloads cannot be remotely erased.

## Increment 4: real isolated execution and release control

Add Git and container-runner adapters with explicit executable allowlists, clean
environments, container image digests, private workspaces, network rules, bounded
mounts, timeouts and process/resource limits. Model outputs and stored scripts are
untrusted. A worker cannot mount arbitrary host paths or accept a fixture executor
as proof of a real build. Supply verifiers separately from builders.

Provision user data through the existing architecture registry. Keep user belongings
outside pushed source trees. A tool manifest declares capability requirements; local
bindings resolve them. Symlinks are only for tool-owned release pointers inside
validated roots, not a substitute for data authorization. Activate by atomic pointer
switch after verification; retain the previous release and check rollback compatibility
before any data migration. Refresh Hotbox from the tool registry.

Acceptance: a tiny fixture application completes a real container build, test, install,
upgrade and rollback. Failures and cancellation terminate workers, invalidate leases
and leave the previous working release and belongings intact. No desktop input injection.

## Increment 5: bounded context and PTC adapter

Build a permission-filtered context assembler using summaries and immutable artifact
references before embeddings. Add ingestion, content hashes, provenance, chunking,
embedding-provider interfaces, retrieval filters and revocation-aware cache invalidation.
Retrieved documents are evidence, never privileged instructions. Do not send content
to an external embedding/model provider without an explicit data-processing policy.

Connect Anthropic PTC through the authenticated tool gateway. Bind each provider
session to a run and current segment, enforce action/iteration/token/cost bounds,
and permit only the tools authorized for that segment. Tool arguments cannot choose
their own principal, grant, approval status or verifier identity. Resume through SQL
checkpoints and idempotent operations instead of provider-side conversation memory.

Acceptance: deterministic scripts and PTC complete the same approved workflow under
the same constraints; attempts to skip stages or escalate access fail identically.

## Increment 6: adapters and deployment packaging

Expose state through an authenticated API, with GraphQL as an optional query adapter.
Render web UI from server-owned SQL state. Keep Qt/Hotbox and Omarchy integration
optional. Add Redis for coordination/cache only when cross-process requirements
justify it; it must not become a second authority for permission or workflow state.
Package deployment manifests, migrations, templates, backup/restore and upgrade
checks so the platform can be cloned into larger desktop or web builds.

Acceptance: the core runs without Omarchy or Qt, personal runtime data is excluded
from distributable code, and a second installation can start from the templates
without inheriting the original installation's identities, credentials or belongings.


## Cross-cutting system catalog

The [system model](SYSTEM_MODEL.md) and bundled catalog specify the expanded
folder/entity taxonomy, identity mapping, connectivity graphs, agents/harnesses,
sessions, logs, decisions, monitors and data pipelines. Read-only catalog validation
is implemented. It does not enable any new runtime adapter or permission action.

Before connecting these categories to agents or collectors, implement the model's
entity/edge SQL registry with scoped traversal and legacy-ID mappings. Follow with
reviewed plans, operation/session/event contracts and a deterministic local-file
ingestion fixture. Authentication and network policy must precede live monitors;
verified isolated execution must precede real release activation. Preserve the
mandatory workflow stages and the existing Qt media layout during these additions.


## Environment/backpack contract slice — implemented planning only

[Portable environments](ENVIRONMENTS.md) now specifies the reusable capability
backpack, distinct backend isolation requirements, local/cloud placement and a
paired ingress/quarantine/admission boundary. Read-only validation/planning commands
and source-only CLI image packaging inputs exist. Plans remain nonlaunchable;
example artifact hashes are placeholders and the image build is unverified.

Persist backpack/environment/boundary/receipt identities through a new catalog and
entity-registry revision. Finish reviewed plans and authenticated scope resolution,
then test a deterministic local-file boundary and crash-safe publication before
adding real sandbox/network workers. A cloud backend must attest required controls
and refuse unsupported features rather than silently weakening isolation. Retain
the same operation protocol with destination-local secrets and bindings. No shared
multi-host SQLite file or implicit export of user belongings is part of this plan.


## Product integration: Agents category and builder — planned

The [agent-builder plan](AGENT_BUILDER_PLAN.md) defines the next UI/product increment:
Hotbox Agents navigation, contextual + creation, saved drafts, mandatory requirement
hooks and pinned skill/task/harness/environment references. It includes compute
ownership, prompt persistence, scoped memory/RAG/logs, scheduling, reporting and the
bounded orchestrator loop. No Agents menu or runtime service is implemented by the
planning document. Start with the scoped authoring registry/validators, then the Qt
builder, and a deterministic local-report fixture before enabling real runners.


## Specialist swarms and global definitions — planned extension

The [swarm/catalog plan](SWARM_CATALOG_PLAN.md) extends agent authoring with a
versioned placement/composition prompt, evidence-based eligibility/fit and task-scoped
coordination roles. Hotbox process actions will compose bounded specialist workflows.
Reusable definitions are globally discoverable only under catalog policy; personal
requests, generations, task outcomes, memory/RAG and logs remain in the runtime
owner's environment. Implement typed proposal validation and the data partition
before routing actions to swarms. The prompt template is not wired into live chats.


## Orchestration loop planner — planned

The [loop-planner specification](ORCHESTRATION_LOOP_PLANNER.md) defines a separate
planning specialist, deterministic controller, scheduler, workers and verifiers.
Start with typed plan/decision records, scoped SQL checkpoint/revision checks and a
deterministic private-output fixture. Add model assistance and UI inspection only
after proposal validation, bounded replanning, wait/stop behavior and acceptance
checks exist. The proposed prompt and JSON example are not installed runtime code.
