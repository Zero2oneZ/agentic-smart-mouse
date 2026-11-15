# Local operation, recovery and troubleshooting

## Supported deployment today

The kernel is a local library and short-lived CLI. There is no service to start,
network port to open, container runtime to configure or Redis/GraphQL endpoint.
It does not consume the desktop app's model settings or respond to Hotbox clicks.

Prerequisites are Rust 1.89+, Cargo, rustfmt, Clippy, Python 3 and a C toolchain.
SQLite is compiled through the locked bundled dependency. Fetch dependencies once;
subsequent repository verification runs offline.

From the repository root:

```sh
cd core
cargo fetch --locked
scripts/check
```

The checked-in lockfile fixes resolved dependency versions. `scripts/check` runs
format checking, Clippy with warnings as errors, tests, a debug build and the
temporary CLI fixture. It uses Cargo metadata to find the actual output directory,
including when `CARGO_TARGET_DIR` is set. `CARGO` can specify a Cargo executable.

The development session initially installed a temporary toolchain without changing
the system PATH. While it still exists, verify from the repository root with:

```sh
RUSTUP_HOME=/tmp/hotbox-rustup \
CARGO_HOME=/tmp/hotbox-cargo \
CARGO=/tmp/hotbox-cargo/bin/cargo \
core/scripts/check
```

Those `/tmp` paths are disposable, not an installation dependency. A new machine
should use its own toolchain and dependency cache. No temporary path is embedded
in the source contracts or database format.

## Database placement and lifecycle

Choose a new orchestration database in an existing private directory. Do not pass
the Qt app's `hotbox.sqlite3`. The fixture walkthrough uses a temporary directory
and deletes it on exit; keep a separate location if checkpoints must persist.

The kernel:

- Rejects `..` traversal and pre-existing symlink components in the database path.
- Creates a new database file with mode `0600` on Unix and applies that mode to an
  accepted existing kernel database.
- Uses SQLite WAL, full synchronous mode, foreign-key enforcement and a three-second
  busy timeout.
- Commits mutations and corresponding accepted-operation events together using
  immediate transactions.
- Uses application ID `1212307523` (`0x48425843`) and schema version `3`.
- Rejects unrelated databases and unsupported schema versions instead of adopting
  them. Only known kernel schemas 1/2 are upgraded.

Schema version 3 stores:

| Table | Columns | Meaning |
| --- | --- | --- |
| `local_tasks` | `id`, `document` | Immutable notes request, checkpoint, controls and private result references |
| `tools` | `id`, `resource`, `origin` | Tool identity and optional selected fork ancestor |
| `tool_versions` | `tool_id`, `version`, `document` | Immutable pinned version metadata |
| `resources` | `id`, `owner` | Local resource identity and ownership |
| `grants` | `id`, `document` | JSON grant and revocation state |
| `runs` | `id`, `document` | Full pinned run, template, inputs, counters and checkpoints |
| `events` | `id`, `run_id`, `actor`, `kind`, `detail`, `created_at` | Accepted-operation ledger |

The `document` fields and event `detail` must contain valid JSON. Nodes and edges
are represented within the pinned template document, not separate normalized DAG
tables. There is no public SQL-write interface; editing stored JSON bypasses kernel
invariants. The database is not encrypted or safe for untrusted writers.

## Restart and process boundaries

Every CLI request opens the database, performs one operation and exits. Run state
survives process restarts. There is no resident kernel scheduler. A host must call
the bounded loop or claim/finish operations to make progress.

After a worker disappears, a subsequent authorized claim checks lease expiry.
Unexpired work remains leased. Expired work is retried only if attempts, permission,
run deadline and global budgets permit. There is no manual lease-clear, renew or
terminal-run resume command. Use a new run ID for a new approved attempt at a
terminal workflow.

After an ambiguous acknowledgement:

1. Read run status and, if needed, the paginated events.
2. If a segment is complete, keep that accepted result and continue.
3. If it still has a live lease whose response was lost, allow it to expire; do not
   modify SQL to force another worker into the attempt.
4. If the run is terminal, inspect the reason and prepare a new run rather than
   changing its pinned template or budget in place.

Status does not advance timers. A running status can therefore describe a run whose
deadline has passed until an authorized claim persists the expiry. A revoked grant
blocks protected inspection; the owner can still cancel their work. This exception
does not grant a different actor administrative access.

## Backups

Databases, WAL files, backups and claim responses are private runtime state and
must stay out of Git. Plainly copying only the main database while writers are
active may miss committed WAL state. Use SQLite's backup interface, preferably
after stopping callers for a predictable application checkpoint.

This example reads the source in read-only mode and creates a new backup. Supply
absolute paths whose parent directories are private. It refuses to replace a backup.

```python
from pathlib import Path
import sqlite3
import sys

source = Path(sys.argv[1]).resolve(strict=True)
backup = Path(sys.argv[2]).absolute()
with backup.open("xb"):
    pass
backup.chmod(0o600)
with sqlite3.connect(source.as_uri() + "?mode=ro", uri=True) as original:
    with sqlite3.connect(backup) as destination:
        original.backup(destination)
        assert destination.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
        assert destination.execute("PRAGMA application_id").fetchone()[0] == 1212307523
```

For example, save the block as a local maintenance script and invoke it with
`python3 backup.py /absolute/workflow.sqlite3 /absolute/new-backup.sqlite3`.
Do not commit actual database paths, backups or maintenance output containing user
data. Backup files contain grant and lease state as well as workflow context.

## Recovery and cloning

For local recovery, stop all callers/workers, preserve the current database and its
sidecar files, validate the backup, and restore into a fresh private directory.
Open the restored database with the same compatible kernel version before making
it active. Do not overlay a restored main file with stale WAL/SHM sidecars.

A backup can contain formerly active grants, stale leases and an older revocation
state. Restoring it can revive authority that was revoked afterward. The current
kernel has no external revocation ledger or recovery reconciliation service. Do
not treat backup restoration as a safe production multi-user failover procedure.
Cross-device recovery needs the policy and credential controls described in the
[implementation plan](PLAN.md).

Cloning software is different from restoring an installation. A fresh clone receives
code, lockfile, docs and templates. Initialize a new database and local resources;
do not include the original users' identities, grants, leases or belongings in a
distributable clone. Source history does not grant access to runtime data.

## Troubleshooting

| Symptom | Check or response |
| --- | --- |
| Cargo/rustc not found | Use an installed toolchain or the temporary-session command above |
| Offline dependency failure | Run `cargo fetch --locked` with intended network access first |
| rustfmt or Clippy missing | Install those components into the selected toolchain |
| Unsupported database/schema | Confirm a separate kernel database and compatible release; do not edit identifiers to force adoption |
| Database locked | Stop overlapping maintenance, allow the busy timeout, and retry after identifying the writer |
| Access denied after sharing | Check the leaf actor/resource/actions and every parent's expiry, delegation and revocation |
| `idle` with `running` | Another live attempt is held; avoid an unbounded polling loop |
| Stale worker lease | A token expired, was reclaimed, finished or cancelled; read current state |
| Budget exhausted | Start a new reviewed request with sufficient declared budget; existing limits are pinned |
| Template conflict | A run ID already names a different pinned request; assign a new ID |
| Output evidence mismatch | Use the fixture executor's real report; invented hashes are not accepted |

Do not troubleshoot by injecting desktop input, disabling authorization checks or
editing checkpoint JSON. The [CLI reference](CLI.md) documents the supported controls.

## Kernel schemas 1/2 → 3

Opening a known schema-1 or schema-2 database adds the required tool/version tables
and the fixed local-task table/index, then sets user_version to 3 in one immediate
transaction. Existing resources, grants, events and active workflow checkpoints are
preserved. The Qt chat database is separate and never migrated by the core.
Older core binaries reject schema 3. Use a consistent database backup and matching
binary for downgrade; do not manually lower user_version. See [Local tasks](LOCAL_TASKS.md)
for private result storage and interrupted-publication recovery.
