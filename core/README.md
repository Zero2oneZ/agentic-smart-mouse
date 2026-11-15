# Deterministic Rust core

This standalone crate contains the first implementation increment and the
tool-lineage slice of the orchestration plan in [docs/PLAN.md](docs/PLAN.md). It can be copied out of this repository and
built without Qt, Omarchy, Hyprland, a model, or a running service.

Use the [documentation index](docs/README.md) for the complete architecture,
contract, CLI, security, operations, testing and decision references. The
[CLI walkthrough](docs/CLI.md#executable-walkthrough) is runnable with a fresh
temporary database; the [implementation plan](docs/PLAN.md) separates the next
increments from current fixture functionality.

It implements immutable tool versions and fork provenance alongside versioned segment contracts, mandatory stage barriers, a validated
DAG, private SQLite checkpoints, transactional worker leases, bounded retries,
operation and abstract cost budgets, cancellation, and a local delegation policy
engine. The fixtures exercise the whole protocol without executing commands.

**This is a local kernel, not a production security service or build sandbox.**
Actor strings are supplied by a trusted caller. SQLite file access is privileged
access to the whole kernel. Authentication, SSH/device keys, OS isolation, real
build execution, Git changes, release activation, PTC, RAG, Redis, GraphQL, and Qt
integration are not implemented here. Fixture completion proves protocol behavior;
it does not claim that an application was built, tested, published, or activated.

## Build and verify

Install Rust 1.89 or newer with Cargo, rustfmt and Clippy. The compiler is installed
through [Rust's official installation workflow](https://rust-lang.org/tools/install/).
Python 3 and a C toolchain are needed for the CLI fixture and bundled SQLite.

```sh
cd core
cargo fetch --locked
scripts/check
```

`scripts/check` runs formatting, linting, Rust tests, and an isolated multi-process
CLI fixture. After fetching the locked dependencies, verification runs offline.
The fixture uses a temporary database, never the application's chat database.
Build output is ignored by Git. The lockfile is part of the source contract.

## Contracts and workflow

`templates/segmented-build-v1.json` is an executable fixture template. Every template
must include discover, document, plan, prepare, build, verify, release and activate
stages. Each segment must transitively depend on every segment in the preceding
stage. Cycles, missing stages, unknown actions/executors and unsupported schema
versions are rejected. Within-stage ordering is expressed by dependencies.

Runs pin the full template and its SHA-256 digest, bounded input context, resource,
actor, and grant. Repeating the same start request returns its existing checkpoint;
reusing its ID for a different request fails. An updated template starts a new run.
Input values are explicit text, not paths that the engine reads automatically.

Version 1 schedules one segment per run at a time, selecting ready work by stage
and ID. A claim reserves one operation and its declared cost units. Failed and
expired attempts consume those reservations. A claim expires at the earlier of
the segment timeout and run deadline. Stale workers cannot finish reclaimed work.
The engine recomputes fixture evidence against the pinned contract before accepting
success. Arbitrary commands and arbitrary executor names are rejected.

Deadlines and lease expiry are enforced at claim/completion boundaries; there is
no background watchdog yet. Status returns the last persisted checkpoint. A future
runner must also enforce process termination and actual resource limits.

## JSON CLI

```sh
target/debug/hotbox-core validate templates/segmented-build-v1.json
printf '%s\n' '{"command":"resource","actor":"alice","resource":"demo"}' |
  target/debug/hotbox-core /private/existing-directory/workflow.sqlite3
```

The CLI accepts one JSON request on stdin (maximum 256 KiB) and emits one JSON
response. `ok:false` also returns a nonzero exit status. Commands are `resource`,
`grant`, `revoke`, `start`, `status`, `claim`, `finish`, `cancel`, `events`, and
`fixture_loop`. See `scripts/fixture_workflow.py` for a complete runnable example.
The library exposes the same operations to future service adapters.

`claim` returns an attempt-specific lease token for its worker. Tokens do not
appear in run status or audit events. Do not publish claim responses or databases.
The database's parent directory must already exist and should be private. The
engine rejects symlinked database paths and unrelated or unsupported databases.
It uses a distinct application/schema identifier and never migrates Qt chat data.

## Delegation boundary

A resource has one owner. Only its owner can issue root grants. A child grant must
name an active parent held by its issuer, retain the same resource, narrow or retain
its actions and expiry, and have explicit permission to delegate. Chains are limited
to 32 grants. No wildcard permissions or implicit cross-resource access exist.

Revocation of any ancestor invalidates dependent access at the next check.
Independent direct grants remain valid. Permissions are checked when starting,
reading, claiming and completing runs. A run owner may cancel their work after
revocation. Revocation does not interrupt a running external process, erase copies,
or undo an already completed operation; external execution is not present yet.

Accepted transitions have transactional audit events. Rejected calls return errors;
durable denial logging belongs to the future authenticated gateway. Local callers
can assert identities, so this policy engine must not be exposed directly to other
users or devices without authentication and resource-creation authorization.

## PTC integration contract

Anthropic's [programmatic tool calling documentation](https://platform.claude.com/docs/en/agents-and-tools/tool-use/programmatic-tool-calling)
describes calls made from its code-execution environment. The future adapter will
translate those calls into bounded kernel operations. Provider execution must never
receive database access, root grants, signing keys, or an unrestricted host shell.
Its execution environment does not itself provide isolation for local builds.

PTC is not connected in this increment. No model credentials or network calls are
used by the core. Real executors need explicit adapters and independent verification
before their contracts can replace the fixture executors.

## Tool lineage

See the [tool registry CLI](docs/CLI.md#tool-lineage-registry). Tool and source
identities are separate from permission grants and runtime storage. Forking records
provenance and a new development-chat reference; it neither reads a Git repository
nor copies data or credentials. Version references cannot be rewritten. These are
trusted-host APIs, not authenticated endpoints or a connected desktop menu.

## Default system catalog

Read [the system model](docs/SYSTEM_MODEL.md) for the expanded folder taxonomy,
entity identity/connectivity, agent and harness contracts, log envelopes and future
monitoring/data pipelines. The read-only commands below need no database:

```sh
cargo run --locked --offline -- catalog
cargo run --locked --offline -- validate-catalog templates/system-catalog-v1.json
```

Catalog validation is implemented; folder provisioning, entity graph persistence,
ID generation and new agent/data services are not. The bundled catalog does not
modify the existing Qt media-v1 template or any live storage binding.
