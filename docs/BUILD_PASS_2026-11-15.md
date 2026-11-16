# Build pass: contracts, gates and provider receipts

This pass followed the existing roadmap. It did not start a swarm or replace the
Rust controller. Every one of the 20 pre-existing live work items now has a
versioned acceptance contract; new implementation work receives its own contract
and evidence. Plans and recorded outcomes remain distinct from independent
functionality acceptance.

| Stage | Pass result | Next evidence needed |
| --- | --- | --- |
| B0 baseline | Blocked: Cargo/rustc absent; local TCP bind returns EPERM | Fresh Rust+Qt build and TCP mock profile on a capable host |
| B1 receipts | Offline adapter implemented and tested; full gate remains partial | Authenticated provider/version fixtures and general runner receipt integration |
| B2 environments | Blocked: uv resolves to a mise shim without an installed usable binary; runtime download cannot resolve package hosts | Usable uv, pinned locks/interpreter and immutable preparation tests |
| B3 bridge | Waiting on B1/B2 and fresh Rust verification | General leased executor, idempotent receipts and cross-database replay/outbox |
| B4 coding acceptance | Waiting on B3 | Exact source/artifact identity and independent checks |
| B5 loop | Waiting on B4 | Event-driven dispatch, bounded replans/retries, aggregate reservations and stopping |
| B6 swarm | Waiting on B5 | Builder/verifier team compared against serial baseline under one parent budget |
| B7 optimization | Waiting on verified outcomes and complete accounting | Held-out baseline/candidate comparison, promotion receipt and rollback |

## Implemented slice

The [provider receipt contract](../core/docs/PROVIDER_RECEIPTS.md) now owns structured
CLI flags, conservative terminal usage normalization, provider-specific cache
accounting and parser bounds. Receipts persist immutably with run, policy, context
and source references. Cancellation, failure, interruption and partial/unknown
usage remain explicit. Parent families cannot automatically follow up with unknown
or legacy unnormalized usage. Scoped UI/CLI/model reads share the same receipts.
The canonical contract has a generated reference and compiled-catalog drift gate.

SQL receipt/counter writes are atomic within the Qt database. Feed notification
is post-commit, not a durable cross-database outbox. Lease, environment, model,
provider-version and billing identities stay unknown until their adapters exist.
Process success remains unverified functionality.

## Checks

- `bin/test --desktop-only --offline`: 37 checks passed; three TCP mocks excluded.
- `bin/test-cli`: both compiled catalog comparisons, negative reference-drift
  checks and the isolated command/session workflow passed.
- `python tests/install_local_smoke.py`: live WAL backup, rollback copies and
  paired binary/launcher installation passed in a temporary home.
- Existing Rust helper reused. No fresh Rust build or authenticated provider run.

Capability probes used local tool lookup, cached CLI help, a TCP bind probe and
uv version resolution. No desktop input or provider workload was launched by
those probes. Temporary fake providers exercised receipt behavior in tests.

The next executable delivery work is to unblock B0/B2 while completing B1's live
provenance contract, then extend the existing Rust runner under B3. Do not claim
B4-B7 completed because the ledger contains their plans or the receipt adapter
passes its offline tests.
