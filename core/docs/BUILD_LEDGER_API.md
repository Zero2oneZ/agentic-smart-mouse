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

## Rolling work and orchestration guidance

Ideas, additions, decisions and contracts are node kinds alongside execution work.
`proposed` is also a planning state. Preserve original proposals as history; archive
abandoned work, fork alternatives, and append new versions rather than rewriting
past evidence. `recorded` means an implementation/outcome was recorded; completion
still requires independent acceptance from the existing execution controller.

Every actionable step can have an immutable, versioned contract:

```text
Idea / addition / decision
          |
Acceptance criteria + implementation references + verification instructions
          |
Pinned roadmap revision + contract version/digest
          |
Worker attempt / command -> observed evidence -> review
          |
Independent verifier acceptance (general runner integration remains planned)
```

`contract.save` takes common mutation fields plus `node`, `acceptance`,
`implementation` and `verification` (required nonempty text, up to 4,000 characters
each). Updating requires the latest `parent_contract`; first versions omit it or
use an empty string. The response includes `contract` and `digest`. Use
`contract.read` with `roadmap` and `contract` to retrieve a full immutable version.
`read` includes contract versions, newest first, capped at 1,000 per roadmap.
Contract changes have the same atomic revision/event/idempotency semantics as
other mutations. They are source references and instructions to review, never
shell commands executed by saving them. Schema migration 2 adds the private
`build_contracts` table and immutability triggers without changing API version 1.
Older application versions reject this newer storage schema.

Forks retain the latest contract at the selected snapshot and give it a new local
ID with a parent pointing to the original version. Subtree forks carry only
contracts attached to the selected nodes. A new branch's inherited command
results cannot establish that its functionality works.

`guide` reads `roadmap` and optional `node`. It returns a deterministic digest,
roadmap revision, branch/source lineage and up to 12 reference items. A focused
read includes the selected item first, then its ancestry. An overview excludes
archived items. Item payloads are capped at 10,000 serialized characters in total;
body/contract excerpts are clipped and full versions remain retrievable by ID.
`omitted` reports items beyond the count/size bounds. This is a character/context
bound, not a token-usage measurement. Selection follows stored order and is not a
dependency solver, readiness decision or approved dispatch plan.

Guidance labels are `review_proposal`, `define_contract`, `review_blocker`,
`implement`, `review_evidence` and `archived`. They are reference hints, not
scheduler commands. Each item includes at most three command result references:
`command_passed`, `command_failed` or `unknown`, inherited status, observed Git
HEAD when available, and `contract_match` (`same_version`, `changed`, `unknown`).
Every result remains `unverified`: a matching contract and zero exit do not prove
artifact identity, independent checking or acceptance. Older/malformed receipts
remain unknown. No output/log bodies are exported in the guide.

Bound `hotbox run` commands pin the starting roadmap revision and selected
contract version/digest into their result event. A stale initial binding prevents
launch; a later result links back to the pinned context even if the plan changed.

Bound supervised agents read a fresh scoped guide before each launch/follow-up.
It is persisted as `target.buildContext` in the private run record and included in
`PROMPT.txt`, explicitly marked as untrusted reference data. The binding mutation
checks the pinned roadmap revision before provider launch. A stale guide prevents
launch; active runs keep their original context. Existing policy revisions,
family budgets, concurrency and follow-up limits continue to apply.

Master-loop inspection now includes a bounded `rollingLedger` roadmap index and
points to `read_build_guidance`. Chat models also have read-only
`read_build_contract` for full contract versions. This connects the implemented
supervised loop's context path. The general Rust controller, dependency planning,
lease dispatch and independent verifier bridge still need the build-roadmap
stages; these read operations do not enable an autonomous swarm.
