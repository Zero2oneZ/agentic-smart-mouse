# Design decisions and unresolved choices

This record distinguishes settled requirements from current implementation choices.
It assigns no brand or product name. See [Architecture](ARCHITECTURE.md) for current
boundaries and [Plan](PLAN.md) for delivery order.

| ID | Decision | Status and consequence |
| --- | --- | --- |
| D01 | Rust implements the portable deterministic core | Accepted; initial crate exists, while the desktop remains Qt/C++ |
| D02 | Build reliable contracts and scripts before agent orchestration | Accepted; fixtures prove protocol behavior before PTC or real executors |
| D03 | Every build uses versioned templates and mandatory segmented stages | Implemented for the fixture workflow; architecture/policy pinning and approvals remain to add |
| D04 | SQL owns durable state | Implemented with private SQLite; no production server database is selected |
| D05 | Tool/source trees and user belongings have separate ownership and lifecycle | Existing app implements local registry/provisioning; OS sandbox enforcement remains to add |
| D06 | Git/version ancestry and data permissions are separate | Accepted; local permission lineage exists, tool/version registry remains to add |
| D07 | Delegation can only retain or narrow upstream authority | Implemented locally for the fixture action vocabulary |
| D08 | Independent grants survive revocation of a different lineage | Implemented; there is no implicit switch of a run's pinned grant |
| D09 | Sharing carries architecture/provenance without silently exporting private state | Accepted; export/share packaging remains to add |
| D10 | Local governors and upstream contribution rights are explicit scoped roles/agreements | Accepted; agreement records and group governance remain to add |
| D11 | Private authentication keys stay with their principals/devices | Accepted; no key infrastructure is implemented |
| D12 | Hotbox interactions select the subject and workflow context | Accepted target: launch, edit and fork route differently |
| D13 | PTC uses the same controlled operations as deterministic callers | Accepted target; Anthropic adapter is not connected |
| D14 | Embeddings/RAG must preserve provenance and current data permissions | Accepted target; retrieval contracts precede provider integration |
| D15 | Server-owned state drives future web views | Accepted target; SQL stores state and an application layer renders views |
| D16 | GraphQL is an API option and Redis supports cache/coordination | Accepted target; neither replaces workflow or permission authority |

## Current implementation choices

- One resource and grant per run; one active segment lease per run.
- Eight stages and eight explicit actions; only two fixture executors.
- SQLite stores pinned JSON documents and accepted-operation events.
- Template hashes use current Serde serialization, not a cross-language signature format.
- Attempts consume budgets even when they fail or expire.
- Wall-clock Unix seconds determine grant, run and lease expiry.
- CLI actor strings are trusted inputs; this is not remote authentication.
- Tool/runtime/Qt integration is deferred rather than approximated through shell commands.

These choices describe v1. Changing them requires contract, test and documentation
updates; some also require a stored-data migration.

## Choices still open

| Area | Decision needed before implementation |
| --- | --- |
| Production state store | SQLite service deployment versus another SQL backend; transaction and migration contract |
| Identity | Login, recovery, organization membership and service-account authority |
| Device trust and SSH | Credential format/storage, registration, rotation, loss and revocation delivery |
| Offline access | Expiry, clock trust, reconnection and conflict/revocation behavior |
| Runtime isolation | Runner technology, image trust, mount/network limits and termination guarantees |
| Approvals | Who approves which operations, signed/durable evidence and separation of duties |
| Shared branches | Confirmed contribution agreement format, merge authority and withdrawal consequences |
| Artifact trust | Canonical hashing, signatures, independent verifiers and retention |
| Data lifecycle | Encryption, backups, recovery, deletion and authority reconciliation |
| PTC | Provider-session binding, tool definitions, capability discovery and token/cost accounting |
| Embeddings/RAG | Local/external provider, vector storage, permission filtering and index invalidation |
| Frontends | Authenticated API boundary, server rendering transport and Qt migration strategy |

Resolve these through focused decisions and acceptance tests. Do not encode an
unsettled production choice as an implicit dependency in fixture code.


## Default categories and connectivity

Use a versioned, read-only catalog for high-level category and entity vocabulary.
Provision lazily through explicit scoped bindings; catalog loading has no filesystem
or runtime effects. Keep entity IDs distinct from display names, paths, content
hashes, provider sessions and permission grants. A primary ownership tree is one
view over typed connectivity, execution and provenance graphs; it cannot replace
SQL authority or live grant checks. Record short decisions and evidence references
rather than private model chain-of-thought. See [System model](SYSTEM_MODEL.md).


## Portable backpacks and the ingestion boundary

Treat a backpack as a hashed requirement definition, separate from the permissions,
secrets and data bindings of each instance. Local and cloud workers share operation
contracts, not a shared database file or ambient credentials. Preserve explicit
backend differences; simulations are not isolation and weaker backend fallback is
forbidden. The external observer, quarantine processor and trusted admission
controller have separate identities and roles. Only a scoped receipt-based promotion
operation crosses into trusted datasets. See [Environments](ENVIRONMENTS.md).
