# System organization, identity and connectivity

## Delivery boundary

This is the target system contract, backed by the machine-readable
[system catalog](../templates/system-catalog-v1.json). The Rust `catalog` and
`validate-catalog` commands implement read-only catalog loading, structural
validation and SHA-256 pinning. They do not create folders, register live agents,
change the installed Qt media template, migrate its data, start monitors, capture
terminals, or grant access. This document specifies the next components so their
identities and relationships can be built consistently.

Already implemented: Qt workspaces, session evidence and lazy media storage;
Rust workflow fixtures, grant lineage, immutable tool-version metadata and fork
ancestry. Qt and Rust remain separate. Catalog lifecycle names describe future
entities and do not rename current `RunStatus` or segment states.

## Overall map

```text
Installation + authenticated principal                         [gateway: future]
                 |
       Workspace / project ownership tree
                 |
     +-----------+----------------+-----------------------+
     |                            |                       |
Tool + source versions     Agent + harness versions   Data + web sources
     |                            |                       |
Repository / branch        Workflow / reviewed plan  Monitor / connector
     |                            |                       |
Commit + build contract     Run -> task -> operation  Ingestion / pipeline run
     |                            |                       |
Environment / release       Attempt + lease          Dataset version + checks
     |                            |                       |
Runtime instance            Session / terminal      Curated artifacts / index
     |                            |                       |
     +------------- Typed connectivity graph ------------+
                                  |
                     Event + trace + decision records
                                  |
              Scoped SQL registry + immutable artifact references
                                  |
              +-------------------+-------------------+
              |                                       |
       Versioned source tree                  Private belongings tree
       Definitions + machinery                Content + runtime history
```

The folder hierarchy is a navigation and storage view. The SQL registry is the
source of entity identity, status and relationships. Moving a permitted storage
location changes its binding, not the identity of a dataset, tool or agent.
Neither graph reachability nor a folder's existence grants access.

## Default folder categories

All categories are **available definitions**, not eagerly created directories.
They are registered by a future explicit catalog migration/provisioner, then
materialized when an authorized project requests them. The catalog declares
relative paths only. Local bindings select absolute roots and ownership scopes.

| Category | Source-owned definitions | User/runtime-owned content |
| --- | --- | --- |
| Projects | Project manifests, scaffolds | State, preferences, local bindings |
| Tools | Tool manifests, versions, dependencies | Instances and preferences |
| Agents | Agent roles, instructions, capability requests | Sessions, checkpoints, scoped memory |
| Harnesses | Executor, tool, policy and evaluation contracts | Runs and evaluation results |
| Workflows | DAGs, budgets, task contracts | Runs and durable checkpoints |
| Tasks and operations | Request/result schemas | Requests, results, attempts |
| Chats and terminals | Session formats and capture policy | Explicitly captured I/O and handoffs |
| Code and repositories | Repo manifests, source organization | Managed worktrees, patches, code logs |
| Environments | Pinned images and runner specs | Instance state and bounded scratch |
| Networking | Endpoint definitions and network policies | Connection/request metadata |
| Web monitoring | Watches, extraction and diff rules | Snapshots, diffs, conditional-fetch state |
| Connectors | API/file/database adapter definitions | Cursors, receipts and checkpoints |
| Ingestion | Collection and normalization contracts | Raw input, quarantine, dead-letter results |
| Data engineering | Transformation code and quality contracts | Staging, curated outputs, backfill results |
| Datasets | Dataset contracts and provenance rules | Immutable versions, manifests, exports |
| Schemas | Versioned schemas and migration plans | Validation reports |
| Knowledge | Chunking, retrieval and indexing policies | Artifacts, chunks, indexes, summaries |
| Documents | Report and document templates | Originals, derived copies, exports |
| Media | Shared layout definitions | Music, sounds, video, pictures, album covers |
| Experiments | Reproducible experiments and benchmarks | Runs, metrics, comparisons |
| Schedules | Disabled schedule templates | Occurrences and execution checkpoints |
| Releases | Build, verification and deployment contracts | Artifacts, activation history, rollback state |
| Observability | Event schemas, redaction and retention policies | Logs, traces and metrics |
| Identity and governance | Policy, role and agreement schemas | Private identity registry, approvals, audit |
| Backup and recovery | Backup/restore procedures | Private snapshots and recovery evidence |

Example logical layout (target, not a provisioned directory tree):

```text
SOURCE ROOT /                              BELONGINGS ROOT /
  architecture/                              registry/      private SQL + migrations
  catalog/agents/                            apps/<project-id>/
  catalog/harnesses/                           state/ config/ cache/
  catalog/monitoring/                         scopes/<scope-id>/
  catalog/ingestion/                           libraries/agents/<agent-id>/
  catalog/data-engineering/                    libraries/monitoring/<monitor-id>/
  catalog/networking/                          libraries/ingestion/<ingestion-run-id>/
  ...remaining catalog definitions...          libraries/datasets/<dataset-id>/
  templates/                                  libraries/observability/
  schemas/                                   media/         existing explicit bindings
  projects/<project-id>/                      secrets/       protected references only
    src/ tests/ migrations/
    agents/ pipelines/ monitors/
    manifests/ container/
```

`belongings_path` in the catalog is relative to an explicitly bound scope root,
not automatically a globally shared directory. A folder request must resolve a
principal/group/project scope and a binding. Default private scope; sharing is an
explicit grant/binding. Retain existing Qt media bindings exactly. Adding these
categories requires an additive, reviewed migration, not editing media-v1 in place.
No live root or folder is created by loading the catalog.

Managed source checkouts can contain source code while remaining local to a
runtime instance; generated worktrees, database files, sessions and private artifacts
are excluded from distribution. The canonical source repository contains only
explicitly selected, distributable files.

## IDs and names

Every durable entity has an immutable ID. Names are editable labels and are never
credentials, filesystem authority or identity. The catalog enumerates entity kinds
and unique short prefixes. Target generated external IDs have the form
`<kind-prefix>_<32 lowercase hexadecimal UUID digits>`. A future trusted registry
allocates them using an OS randomness source and enforces SQL uniqueness. Hashes
identify bytes/revisions; they do not replace entity IDs or ownership checks.

Do not rewrite existing Qt numeric categories, `seg_…`/`part_…` IDs, Rust caller-supplied
IDs or grant IDs. Integration must use a unique mapping table
`(installation_id, legacy_system, legacy_kind, legacy_id) -> entity_id` and preserve
old references. This increment introduces no new runtime ID allocator.

Minimum future entity record:

```text
id, kind, scope_id, owner_principal_id, display_name
created_at, created_by, updated_at, row_revision, schema_version
lifecycle_status, status_reason_code, archived_at
```

Version-specific records additionally pin source/content digests, architecture,
policy, template and schema versions. Human descriptions may be revised with history;
immutable input/version/evidence records cannot be overwritten.

Readable agent labels can follow `role/project/short-id`, for example
`ingest/music/7c42`, while their IDs remain `agt_<uuid>`. A session uses `ses_…`, an
attempt uses `atm_…`; neither is an alternative spelling of the agent ID. Concurrent
instances of one agent version have distinct sessions, operations and leases.

## Connectivity graph and ownership tree

The UI may show a primary containment tree:

```text
installation -> workspace -> project
                            +-> tools / agents / harnesses / workflows
                            +-> sources / monitors / pipelines / datasets
                            +-> repositories / sessions
```

The full system is a typed directed graph. A tree cannot represent shared libraries,
many-input transformations or independent grants. Store edges with IDs, endpoint
IDs, relation kind, scope, creator, timestamps and provenance. Validate endpoint
existence, permitted kind pairs, scope and live access on creation and traversal.

| Graph | Typical edges | Required rules |
| --- | --- | --- |
| Containment | `contains` | One primary parent, no cycles, explicit cross-scope attachment |
| Execution | `depends_on`, `attempt_of` | DAG per pinned run; attempts refer to one operation |
| Provenance | `version_of`, `forked_from`, `derived_from`, `produced` | Immutable origin references; no ancestry cycles |
| Connectivity | `connects_to`, `observed` | Links may cycle; endpoints and egress policy remain explicit |
| Association | `runs_with`, `uses_binding`, `captured_in`, `governed_by` | References are scoped, never access grants |
| Permission | Parent grant lineage | Separate authorization registry; existing narrow/delegation/revocation checks |

The catalog declares coarse allowed source/target kinds and acyclicity requirements.
It does not validate live edges, enforce pairwise version matching or prevent actual
graph cycles. The entity/edge registry must add those checks transactionally. For
example `tool_version version_of tool` is valid; `tool_version version_of dataset`
is invalid even though both kinds appear in the coarse version relation domain.
Queries must filter every node and edge before rendering, pagination, count summaries
or retrieval; a visible parent does not disclose private descendants.

## Branch, fork, clone and instance mean different things

| Operation | Result | Preserved | Never implicitly copied |
| --- | --- | --- | --- |
| Branch | New source ref at a pinned commit | Repository identity and commit ancestry | Runtime state, permissions |
| Fork | New independently editable tool identity from a selected version | Provenance and selected source/architecture references | Credentials, grants, user data |
| Clone | New local repository checkout identity | Authorized Git objects and remote provenance | Runtime bindings, chat database, secrets |
| Instance | New runtime identity of one verified release | Release/tool version references | Another instance's private data or credentials |
| Retry | New attempt of the same operation | Immutable operation request, prior attempt evidence | Prior lease authority or an assumed successful effect |
| Resume | Continue from a durable checkpoint | Completed steps and pinned inputs | Permission that has since expired or been revoked |
| Reprocess/backfill | New run over declared source/version/window | Upstream provenance and checkpoints | Silent replacement of published dataset versions |

A clone/fork snapshot cannot be remotely erased after authorized export. Revocation
blocks later protected operations; retention/export policy must explain already
released copies. Source/data licensing and contribution eligibility are separate
from the right to read a local resource.

## Agents, harnesses, tasks and operations

An **agent definition** declares role, instructions, expected inputs/outputs and
requested capabilities. An immutable agent version pins those definitions and model
configuration references. Model/provider names are metadata; they are not authority.

A **harness version** pins the execution contract: allowed adapters/tools, argument
schemas, network/mount/environment policy, time/operation/token/cost limits, retry
policy, checkpoint protocol, artifact rules and independent verifier references.
A harness is reusable across agents; no agent may grant itself a different harness
or expand a capability request into permission.

A **task** describes the intended result and acceptance criteria. A **workflow run**
pins the plan, template, architecture, agent/harness versions and allowed resources.
An **operation** is one bounded request with a stable idempotency key. An **attempt**
is one leased execution of it. Session, process/container and provider-call IDs are
recorded separately. The trusted host derives principals and verifier identities.

Future request envelope:

```text
request_id, actor_principal_id [host-derived], scope_id
workflow_run_id, task_id, operation_id, parent_operation_id
agent_version_id, harness_version_id, plan_revision_id
input_artifact_refs, output_contract_ref, grant_ref
idempotency_key, deadline, resource_budget, trace_id
```

Before retrying an operation with external side effects, reconcile its receipt or
remote idempotency key. A timeout alone cannot establish whether a write occurred.
Use bounded attempts and explicit `unknown_effect` result details; do not claim
exactly-once delivery. Initial networking/ingestion adapters should be read-only.

## Status, event logs and decision records

Keep lifecycle, execution outcome, health and freshness as separate fields. An agent
can be active while its last task failed; a successful ingestion may still yield
stale data; a disconnected terminal does not mean its operation succeeded.

The catalog supplies target definition, execution, session, artifact, approval, grant and
attempt lifecycles, and validates their transition declarations. Future entity storage must enforce transitions with
compare-and-swap row revisions and transactionally appended events. Existing kernel
statuses remain unchanged until an explicit migration maps their semantics.

```text
Task/operation: pending -> ready -> running -> succeeded
                                      |
                                      +-> waiting -> ready (resume)
                                      +-> failed / cancelled / expired
Retry after terminal failure -> new attempt/run as policy permits
```

Common event envelope (target):

```text
event_id, schema_version, recorded_at, observed_at, producer_sequence
scope_id, actor_id, entity_id, entity_kind, event_type
workflow_run_id?, task_id?, operation_id?, attempt_id?
agent_id?, harness_version_id?, session_id?, terminal_session_id?
trace_id?, span_id?, parent_span_id?, correlation_id?, causation_event_id?
previous_status?, new_status?, reason_code?, artifact_refs[]
redaction_policy_version, retention_policy_id, payload_digest
```

Use a per-producer sequence and database event order; timestamps alone are not a
cross-device total order. Preserve original observed time and trusted ingest time.
Append status events atomically with state changes; an outbox publishes committed
events for external adapters. Metrics and caches are derived, never independent
sources of workflow truth.

| Stream | Record | Keep outside source Git |
| --- | --- | --- |
| Code log | Repo/branch/commit, selected file diffs, build/test artifact refs | Private diffs and local session context |
| Network log | Endpoint ID, method, result, latency, byte count, bounded retry info | Credentials, headers, query secrets and body content |
| Terminal session | Explicit invocation, cwd binding, executable/args, exit/signal, bounded stdout/stderr refs | All captured transcripts |
| Agent log | Agent/harness version, operation, tool-call refs, usage/budget counters | Private prompts and content |
| Decision record | Goal, brief rationale, assumptions, alternatives, evidence, confidence, reviewer/outcome | Private source material |

Reasoning means concise decision summaries and evidence references. Do not require
or store private model chain-of-thought. Link inspectable requests, tool results,
verifier evidence and human decisions instead.

No ambient terminal capture or desktop input injection. Commands are captured only
through an explicitly invoked adapter. Redact before durable storage, bound output,
mark truncation/dropped counts and link paged artifacts. Request/response bodies are
off by default. Secret references resolve in a protected credential service; neither
names, URLs, logs nor distributable manifests contain raw secrets. Retention, access,
quota and purge policies apply independently to every stream and artifact class.

## Web monitoring and ingestion

```text
Source + endpoint + explicit access policy
             |
Disabled monitor template -> enabled schedule -> unique occurrence
             |
Read-only connector -> rate/size/time bounds -> fetch receipt
             |
Raw immutable artifact + content digest + source metadata
             |
Type/schema validation ---failure---> quarantine + diagnostic artifact
             |
Normalization -> deduplication -> staging -> quality checks
                                              |
                                      reject / dead-letter
                                              |
Dataset version + partition manifest + provenance + freshness watermark
             |
Curated outputs -> scoped index / summaries -> diff event / notification
```

Pin connector/parser versions and source/endpoint IDs. Keep credentials behind
scoped references. Record conditional-fetch tokens, pagination cursors, source
watermarks and last-success timestamps as private state. Source changes, deletions,
duplicates and out-of-order records have explicit policies. A fetch with unchanged
content updates observation metadata without inventing a new data version.

Enabling a monitor requires a declared source, allowed network destinations,
polling floor, per-source concurrency, byte/time/storage limits, retry/backoff/jitter,
maximum failures, retention and notification policy. Validate resolved addresses
and redirect destinations at each hop in the future network adapter; stored URLs
alone do not authorize arbitrary host access. Fetch content is untrusted evidence,
never privileged instructions or executable scripts. No live monitoring is enabled
by this catalog.

Commit data-version manifests and cursor/checkpoint advancement together or through
a recoverable staged-commit protocol. A failed fetch or quality check must not
advance the source watermark or replace the last good published dataset. Dead-letter
records retain bounded diagnostics and can be replayed as new operations under a
valid grant.

## Data engineering and reproducibility

A dataset has a stable identity; each published version pins source artifacts,
connector and transformation versions, schema, partition/window selection, quality
results and output digests. Raw inputs remain separate from derived staging and
curated results. Partition manifests describe files; paths do not establish dataset
identity or access.

A pipeline run pins the DAG, source versions, transform code, harness/environment,
parameters and watermark. Support incremental runs and explicit bounded backfills.
Late data and schema changes create new revisions, with compatibility checks before
publication. Never silently coerce an incompatible schema, overwrite published
versions, or let a quality failure become a successful narrative.

Track both byte identity and semantic provenance: identical bytes from two private
sources do not automatically become a globally shared artifact. Deduplication,
indexes, caches and search results remain scope-aware. Revocation invalidates future
retrieval and derived caches according to recorded dependency bindings; licensed
exports and already read copies require separate retention semantics.

## Reliability and implementation order

1. Implement typed entity/edge SQL tables, random IDs, legacy mappings and scoped
   queries. Test endpoint kind matching, missing references, ancestry cycles,
   cross-scope denial and concurrency. Graph links cannot mint authority.
2. Finish immutable reviewed-plan/approval records, policy revisions and contribution
   eligibility. Migrate existing checkpoints without changing active run inputs.
3. Add agent/harness/session/operation records and structured event/decision schemas.
   Test idempotency conflicts, lease loss, cancellation and bounded log capture.
4. Build a local-file connector and deterministic temporary-data pipeline. Test
   malformed input, duplicates, quality failure, crash recovery, cursor advancement,
   schema changes, late records and bounded backfills.
5. Add authenticated gateway and network adapters, then disabled-by-default monitors.
   Test redirect/address policy, revocation, rate limits, retries, retention and quotas.
6. Add isolated Git/build runners, independent verification and atomic release
   activation/rollback. Then connect Qt Edit/Fork/navigation and model adapters.

Cross-cutting requirements: schema migrations with consistent backups, transactional
outbox, idempotent consumers, bounded queues, per-scope quotas, admission control,
backpressure, lease fencing, cancellation propagation and crash-recovery tests.
Restore tests must check blobs and bindings as well as SQL. Policy or ownership
changes must invalidate cached authority. Deletion uses tombstones and explicit
reference-aware garbage collection; archiving a tool never deletes user belongings.

This list is an implementation contract and acceptance sequence. It does not claim
that folders, schemas or fixture completion provide a production sandbox or a live
agent/data platform.
