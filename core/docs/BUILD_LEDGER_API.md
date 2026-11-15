# Build ledger: plans, evidence and versioned documents

Implemented in the desktop backend. Open **Build roadmap** from the sidebar.
This is a durable planning/evidence ledger. Rust remains responsible for execution
leases, acceptance and verified results; a planning state cannot certify a build.

```text
Master chat (global event feed)
  > Workspace roadmap [revision, declared Git branch/commit]
      > Milestone
          > Step
              > Substep / test / issue / debug / documentation
                  + terminal command start/result + observed Git state
                  + supervised agent run + private run directory
                  + chat event
                  + document -> immutable version -> next version
      + checkpoint -> snapshot at a specific roadmap revision
          + fork entire roadmap or selected subtree -> new IDs + provenance
```

## Ownership and storage

| Data | Authority / location |
| --- | --- |
| Source definitions, SQL schema, API and build plans | Version-controlled repository |
| Roadmaps, nodes, links, changes, checkpoints, document text | Private `hotbox.sqlite3`, `build_*` tables |
| Redacted document object copies | Private `build-ledger/objects/<sha256>.txt` under the application data directory |
| Master chat and scoped evidence | Existing session event ledger; each accepted build mutation emits a `build.*` event |
| Agent process output and terminal command evidence | Existing private run/event storage, linked by stable references |
| Execution leases, plans and verified acceptance | Existing Rust core and its separate `orchestration.sqlite3` |
| Personal belongings and workspace libraries | Existing storage registry; no implicit relocation or copying |

SQL holds canonical document content; object files are immutable copies. Both
receive redacted text. Versions preserve the parent version, content digest,
UTC creation time and declared branch/commit. Existing object bytes are verified
before reuse; SQL rejects updates/deletes to versions, changes, checkpoints and
idempotency receipts. This guards application mistakes, not a user with arbitrary
database access. Back up the private database and object directory together.
An interrupted transaction can leave an unreferenced object; it is not deleted
automatically. Restart retains committed SQL state.

Workspace visibility follows existing scoped event rules. Master sees the global
feed; child scopes cannot read another workspace's roadmaps or documents. Agent
launches and bound CLI commands require the roadmap's exact workspace. Model
context tools can read bounded roadmap/document context and cannot mutate it.

## Standard local API, version 1

Backend/QML: `backend.buildAction(operation, requestObject)`.
CLI: `hotbox build OP --category ID --request-file /path/to/request.json`.
This is a local structured API, with no HTTP listener or arbitrary SQL endpoint.
Every request includes numeric `"schema_version": 1`. Responses include `ok` and
`schema_version`; errors include `error`. Unknown fields and wrong types fail.
Use `schema` for a machine-readable operation/field/enum catalog and `list` for
visible roadmaps.

Mutations require a unique `request_id`. Operations on an existing roadmap also
require numeric `expected_revision` from the latest read. Identical request replay
returns the original receipt with `replayed: true`; reusing a key for different
arguments fails. A stale revision fails without changing data. Refresh and review
before issuing a new request. Data changes, history, the Master event and the
idempotency receipt commit in one SQL transaction.

| Operation | Fields in addition to schema version |
| --- | --- |
| `schema`, `list` | None |
| `read` | `roadmap` |
| `history` | `roadmap`, optional `before` revision; 40 changes per page |
| `document.read` | `roadmap`, `version`, optional character `offset`; chunks of 6,000 with `nextOffset` |
| `checkpoint.read` | `roadmap`, `checkpoint` |
| `create` | `request_id`, `title`, optional `branch_ref`, `commit_ref` |
| `node.add` | `request_id`, `roadmap`, `expected_revision`, `kind`, `title`, optional `parent`, `body` |
| `node.update` | Common mutation fields, `node`, `state`, optional `body` |
| `document.save` | Common mutation fields, `content`; new document: `title`, optional `node`; next version: `document`, latest `parent_version`; optional declared Git refs |
| `link` | Common mutation fields, `kind`, `ref`, optional `node` |
| `checkpoint` | Common mutation fields |
| `fork` | Common mutation fields, `title`, optional `checkpoint`, subtree root `node`, declared Git refs |

Common mutation fields mean `request_id`, `roadmap`, `expected_revision`.
Node kinds: `milestone`, `checkpoint`, `step`, `substep`, `issue`, `test`, `debug`,
`documentation`. Substeps require a parent. Planning states: `planned`,
`in_progress`, `blocked`, `recorded`, `archived`. `verified` is deliberately
rejected. Link kinds: `agent_run`, `chat_event`, `terminal_command`; referenced
records must exist and be visible. Command links require command events.

Read collections are bounded and report limits: at most 1,000 nodes, document
versions, links and checkpoints; read includes the latest 100 change summaries.
New items beyond these bounds fail. List returns at most 100 visible roadmaps.
Document content is limited to 65,536 characters and request files to 128,000
bytes. History and document text have explicit pagination.

Example request file for `create`:

```json
{"schema_version":1,"request_id":"roadmap-create-001","title":"First verified coding loop","branch_ref":"feature/build-loop"}
```

Use its returned `roadmap` and `revision` to add a step:

```json
{"schema_version":1,"request_id":"step-create-001","roadmap":"RETURNED_ID","expected_revision":1,"kind":"step","title":"Prove one command receipt"}
```

A document save creates a stable document ID plus an immutable version ID. Editing
requires that document ID and its latest version as `parent_version`; it does not
overwrite old text. Checkpoints capture the state **before** the checkpoint
mutation's revision increment. Forking from one copies the latest document version
visible in that snapshot. New node IDs and a `node_map` prevent identity collisions;
source document-version lineage, source roadmap/revision and selected subtree root
remain inspectable. A subtree fork includes descendants and their attached
versions/links, with its root detached from the old parent.

Branch/commit fields are declared provenance. Forking a roadmap does not create a
Git branch or worktree. CLI command receipts separately record observed Git state
before and after execution; execution still uses existing workspace safeguards.

## Bind real activity

```sh
hotbox run --category ID --build ROADMAP_ID --step STEP_ID -- python -m pytest
```

This explicit command records start/result events, a `terminal:<start-event>`
session reference, exit status, output and observed Git state, then links both
events to the step. Nonzero exit records an issue beneath that step. It is a
recorded command invocation, not an interactive PTY. A binding failure before
launch prevents execution. If result binding fails after execution, reconcile the
saved result instead of automatically rerunning a command with possible effects.

The Build roadmap page can start Codex/Claude with a prompt attached to a selected
step. The agent run is linked before provider launch; invalid bindings prevent
launch. Follow-ups retain the build/step association. These supervised runs still
use private starter directories; they do not implicitly edit project source.
Authentication and general Rust runner dispatch remain separate delivery gates.

## Verification and next integration

Temporary-database tests cover revision conflicts, idempotency, scope boundaries,
redaction, restart, immutable records, corrupted object rejection, historical
checkpoint and subtree forks, model read access, agent pre-launch binding and QML
loading. CLI smoke tests execute a successful and failed bound command, verify
issue creation, and exercise document versions/checkpoint forks.

Connect normalized provider receipts, then the existing Rust leased runner and
independent verifier as described in [BUILD_ROADMAP.md](BUILD_ROADMAP.md). Preserve
this ledger as the user-facing plan/evidence index rather than adding a second
scheduler or treating a saved checkpoint as execution acceptance.
