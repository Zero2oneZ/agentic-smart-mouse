# JSON CLI and library reference

## Invocation and envelopes

Run commands from `core/` after `cargo build --locked --offline`:

```sh
target/debug/hotbox-core validate templates/segmented-build-v1.json
target/debug/hotbox-core /private/existing-directory/workflow.sqlite3 < request.json
```

`validate` reads one template file, checks it and returns its ID, version and digest.
It does not open a database. Other invocations accept exactly one database argument
and one complete JSON request on stdin; they do not stream multiple requests. Both
template-file input and stdin are limited to 256 KiB. Unknown fields and commands
are rejected. The parent directory for a new database must already exist.

Success exits with code 0 and prints:

```json
{"ok":true,"result":{}}
```

Errors exit nonzero and print:

```json
{"ok":false,"error":"access denied: run belongs to another actor"}
```

Error strings are diagnostic, not stable machine-readable error codes. The Rust
library distinguishes `Invalid`, `Denied`, `Conflict`, `NotFound`, `Sql`, `Json` and
`Io`. Malformed requests are parsed before opening the database. A syntactically
valid command can open/initialize a database before later semantic checks fail.

All example identities below are assertions by a trusted local caller. They do not
authenticate Alice, Bob or a worker. Do not expose this CLI directly to remote users.

## Command catalog

| Command | Required fields besides `command` | Result |
| --- | --- | --- |
| `resource` | `actor`, `resource` | Empty object after bootstrap or idempotent match |
| `grant` | `request` | Grant object, including `revoked:false` |
| `revoke` | `actor`, `grant_id` | Empty object |
| `start` | `request` | Run view |
| `status` | `actor`, `run_id` | Run view |
| `claim` | `actor`, `run_id`, `worker` | Claimed work or idle state |
| `finish` | `actor`, `run_id`, `segment_id`, `worker`, `token`, `report` | Updated run view |
| `cancel` | `actor`, `run_id` | Current run view |
| `events` | `actor`, `run_id`, `after` | Object with `events` array |
| `fixture_loop` | `actor`, `run_id`, `worker`, `max_steps` | Executed count and run view |

### Resource bootstrap

```json
{"command":"resource","actor":"alice","resource":"project"}
```

Creates one resource owned by Alice. An existing resource is not reassigned.
Resource creation authorization belongs to the future authenticated host.

### Grant and revoke

```json
{
  "command":"grant",
  "request":{
    "id":"alice-bob",
    "issuer":"alice",
    "grantee":"bob",
    "resource":"project",
    "parent":null,
    "actions":["source.read"],
    "may_delegate":true,
    "expires_at":2000000000
  }
}
```

The example expiry is illustrative: replace it with a future Unix timestamp no
more than 31,536,000 seconds from the actual call. `actions` must be nonempty and
use the [known action vocabulary](CONTRACTS.md). The grant only permits `source.read`;
it is insufficient to start the eight-stage example build.

A child grant sets `parent` to its parent grant ID and `issuer` to that parent's
grantee. Its resource must match; actions and expiry cannot exceed the parent's;
the parent must allow delegation. A result contains all request fields plus
`revoked:false`. There is no CLI command to enumerate grants.

```json
{"command":"revoke","actor":"alice","grant_id":"alice-bob"}
```

Only the grant issuer or resource owner may revoke it. The grantee cannot revoke a
grant merely because they receive it. Ancestor revocation is evaluated dynamically;
descendants are not deleted or rewritten. See [Security](SECURITY.md).

### Start

The request object has these fields:

| Field | Meaning |
| --- | --- |
| `id` | New run ID, or an exact replay of an existing request |
| `actor` | Principal that owns the run and is the grant's grantee |
| `resource` | Exact resource covered by the grant |
| `grant_id` | Existing live grant covering every action required by the template |
| `template` | Full versioned template object, not a filename |
| `inputs` | Bounded map of explicit string context |

The walkthrough below constructs this request without embedding a long template
inside the documentation. No `resume`, `update_run`, `change_grant` or
`approve_plan` command exists. Reusing an existing run ID with a different grant,
template, actor, resource or input map conflicts.

### Status and run view

```json
{"command":"status","actor":"alice","run_id":"fixture"}
```

The result contains `id`, `template_id`, `template_version`, `template_hash`,
`status`, `operations`, `reserved_cost_units`, `deadline_at` and a `segments` map.
Each segment view contains `status`, `attempts` and `outputs`. It excludes lease
tokens, worker identities, raw inputs, the grant ID and the full template.

Reading requires run ownership and a live matching grant chain. Persisted state
is returned without advancing lease or deadline timers.

### Claim

```json
{"command":"claim","actor":"alice","run_id":"fixture","worker":"worker-1"}
```

The `result` is one of these shapes:

```json
{"status":"idle","run_status":"running"}
```

or an object with `status:"claimed"` and a `claim` containing:

| Claim field | Meaning |
| --- | --- |
| `run_id` | Selected run |
| `segment` | Full pinned segment specification |
| `template_hash` | Pinned template digest |
| `worker` | Worker bound to this attempt |
| `token` | Attempt-specific lease token; do not publish |
| `attempt` | One-based attempt count |
| `expires_at` | Absolute Unix expiry |
| `inputs` | Bounded run input map |
| `dependencies` | Direct dependency IDs mapped to their accepted output maps |

Idle with `running` means another live lease exists. Idle may also report a terminal
run status. It is not a signal to spin in an unbounded polling loop.

### Finish

Send the actual worker and token from the claim. A success report is:

```json
{"outcome":"success","outputs":{"evidence":"RECOMPUTED_FIXTURE_DIGEST"}}
```

The digest above is a placeholder and will fail validation. Produce valid fixture
reports using the Rust library's `fixture_report(&claim)` or use `fixture_loop`.
The complete request wraps the report with `command:"finish"`, `actor`, `run_id`,
`segment_id`, `worker` and `token`. The engine loads its own pinned specification;
it does not trust a replacement specification from the worker.

A failure request can be sent directly:

```json
{
  "command":"finish",
  "actor":"alice",
  "run_id":"fixture",
  "segment_id":"discover",
  "worker":"worker-1",
  "token":"ACTUAL_TOKEN_FROM_CLAIM",
  "report":{"outcome":"failure","error":"fixture worker stopped"}
}
```

Failure text is limited to 1,024 UTF-8 bytes and is not persisted in event detail.
Accepted success or failure clears the lease. Retrying an already accepted finish
is rejected; query state to recover from an ambiguous acknowledgement.

### Cancel

```json
{"command":"cancel","actor":"alice","run_id":"fixture"}
```

The run owner may cancel even after grant expiry or revocation. Cancellation of a
running run clears leases and persists `cancelled`. Terminal runs remain unchanged.
It does not terminate an external process; no such executor is implemented.

### Events

```json
{"command":"events","actor":"alice","run_id":"fixture","after":0}
```

Returns up to 100 events in increasing local ID order. `after` must be non-negative;
continue using the last returned ID until a page is empty. Each event contains
`id`, `kind`, parsed JSON `detail` and `created_at` in Unix seconds. Row IDs are
global within this database, so a run's IDs can have gaps.

| Kind | Detail |
| --- | --- |
| `run.started` | `template_hash` |
| `segment.claimed` | `segment`, `worker`, `attempt` |
| `segment.verified` | `segment` |
| `segment.failed` | `segment` |
| `segment.lease_expired` | `segment` |
| `run.budget_exhausted` | Empty object |
| `run.stopped` | Terminal `status` for completion, failure or observed deadline |
| `run.cancelled` | Empty object |

`resource.created`, `grant.issued` and `grant.revoked` are stored with no run ID,
so this run-scoped endpoint does not return them. A governance audit API is future
work. Tokens and raw worker errors are not included in event detail.

### Bounded fixture loop

```json
{"command":"fixture_loop","actor":"alice","run_id":"fixture","worker":"worker-1","max_steps":3}
```

`max_steps` must be 1–512. Each iteration claims, computes a fixture report and
finishes the segment. Idle ends the loop immediately. The result contains `executed`
and `run`. A second invocation resumes from SQL, subject to the original run limits
and deadline. This loop cannot execute host scripts or call a model provider.

## Executable walkthrough

Run this block from `core/` after building. It creates a private temporary directory,
uses fresh processes for each request, resumes a partial run and cleans up afterward.
The checked-in [fixture workflow](../scripts/fixture_workflow.py) additionally tests
delegated revocation and independent grants.

```python
import json
from pathlib import Path
import subprocess
import tempfile
import time

binary = str(Path("target/debug/hotbox-core").resolve())
template = json.loads(Path("templates/segmented-build-v1.json").read_text())
actions = sorted({a for step in template["segments"] for a in step["required_actions"]})

with tempfile.TemporaryDirectory(prefix="hotbox-docs-") as directory:
    database = str(Path(directory) / "workflow.sqlite3")

    def call(command, **fields):
        completed = subprocess.run(
            [binary, database],
            input=json.dumps({"command": command, **fields}),
            text=True, capture_output=True, check=True, timeout=15,
        )
        response = json.loads(completed.stdout)
        assert response["ok"], response
        return response["result"]

    call("resource", actor="alice", resource="project")
    call("grant", request={
        "id": "local-build", "issuer": "alice", "grantee": "alice",
        "resource": "project", "parent": None, "actions": actions,
        "may_delegate": False, "expires_at": int(time.time()) + 600,
    })
    call("start", request={
        "id": "fixture", "actor": "alice", "resource": "project",
        "grant_id": "local-build", "template": template,
        "inputs": {"goal": "Exercise the deterministic protocol"},
    })
    first = call("fixture_loop", actor="alice", run_id="fixture",
                 worker="docs-worker", max_steps=3)
    assert first["executed"] == 3 and first["run"]["status"] == "running"
    second = call("fixture_loop", actor="alice", run_id="fixture",
                  worker="docs-worker", max_steps=8)
    assert second["executed"] == 5 and second["run"]["status"] == "complete"
    print("Documentation walkthrough passed:", second["run"]["template_hash"])
```

## Rust host API

The library exports `Engine`, `Clock`, `SystemClock`, typed templates, requests,
claims, reports, views and `Error`. Construct with `Engine::open(path)` or inject a
trusted clock with `Engine::open_with_clock(path, clock)`. Methods correspond to
the command catalog. `check_access` additionally evaluates an explicit action set
for a grant; it has no CLI equivalent. `fixture_loop` is CLI composition, not an
`Engine` method. `Template::validate`, `Template::hash` and `fixture_report` can run
without a database. Use `cargo doc --locked --offline --no-deps` for generated API
navigation; this reference supplies the behavioral contract.


## Tool lineage registry

These commands use the same trusted-host actor model as the workflow API. They
store metadata only; they do not run Git, verify commit existence, open chats,
copy source files, provision data, or grant permissions. Chat and architecture
references are bounded identifiers. Source commits must be full lowercase 40- or
64-character hexadecimal Git object IDs. Version labels are immutable bounded
identifiers; they are not interpreted as ordered semantic versions.

Register on an existing resource owned by the actor:

```json
{"command":"register_tool","request":{"actor":"alice","tool_id":"player","resource":"project","initial":{"version":"1.0.0","source_commit":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","architecture_version":"architecture-v1","originating_chat":"chat-original"}}}
```

Add a version with `{"command":"tool_version","actor":"alice","tool_id":"player","version":{...}}`,
where `version` has the same four fields as `initial`. The owner may replay identical
version metadata, but cannot change an existing version; choose a new label instead.
Registering or forking with an existing tool ID returns a conflict.

Resolve the originating context for a selected version:

```json
{"command":"tool_context","actor":"alice","grant_id":"alice-root","tool_id":"player","version":"1.0.0"}
```

The result contains `tool_id`, `resource`, `selected` version metadata and nullable
`forked_from` ancestry. Reads require a current `source.read` grant for that resource,
including for the owner. Mutations require ownership and do not use delegated
`source.write` authority. No wildcard or new permission action is introduced.

Fork into a distinct existing caller-owned resource:

```json
{"command":"fork_tool","request":{"actor":"alice","grant_id":"alice-root","source":{"tool_id":"player","version":"1.0.0"},"tool_id":"player-fork","resource":"fork-project","originating_chat":"chat-fork"}}
```

The initial fork version retains the selected version label, source commit and
architecture reference, while using the supplied fork chat reference. Its ancestry
retains the original tool and version. Permissions and runtime bindings are never
copied. The destination needs its own grant for protected reads. Revocation prevents
later reads/forks from the source; it does not erase an already created independent
fork. Reviewed plans and licensing eligibility are not implemented in this slice.

Registry mutations emit accepted-operation events with no `run_id`; current event
pagination is run-scoped, so registry events do not yet have a public query command.


## System catalog (no database)

```sh
hotbox-core catalog
hotbox-core validate-catalog core/templates/system-catalog-v1.json
```

`catalog` returns `catalog` (the bundled definition) and `catalog_hash`.
`validate-catalog FILE` returns `catalog_id` and `catalog_hash`. Both use the normal
`ok/result` or `ok/error` JSON envelope. File reads have the same 256 KiB limit as
other CLI inputs. The hash pins the parsed typed representation: whitespace is
ignored, while array ordering and definition changes affect the digest.

Validation rejects unknown fields/schema/provisioning modes, duplicate IDs/prefixes,
unsafe or overlapping category roots, invalid lifecycle transitions, unreachable
or nonterminating lifecycle states, unknown relation endpoint kinds, and ancestry
relations that permit cycles. It validates **declarations**, not runtime graph
instances or external filesystem contents. See [System model](SYSTEM_MODEL.md) for
scope-root resolution, migration boundaries and the proposed entity contracts.

These commands never open SQLite, create category folders, resolve symlinks on disk,
start agents/monitors, capture terminals or register the catalog in the running app.
