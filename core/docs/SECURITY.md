# Security, sharing and governance

## Current trust boundary

The Rust core is a local policy and workflow kernel. Its caller supplies actor
identities. There is no login, device registration, SSH service, TLS endpoint,
credential issuance or network authorization layer. Access to the database or its
OS account permits changing the underlying state. This is not a safe remote
multi-user service merely because grant evaluation is implemented.

The filesystem checks reject visible symlinks and parent traversal when opening a
database. They are defensive checks, not a race-proof filesystem sandbox. Use a
trusted private parent directory. The kernel sets database permissions to `0600`
on Unix; it does not encrypt the file, implement OS ACLs or defend against a local
privileged administrator.

## Implemented authority model

| Object | Current rule |
| --- | --- |
| Resource | One immutable owner; no transfer command |
| Root grant | Issued by that resource's owner |
| Child grant | Issued by the active parent's grantee, with permission to delegate |
| Resource scope | Child and parent must name exactly the same resource |
| Actions | Nonempty subset of eight known build actions; no wildcard |
| Expiry | Future Unix time within one year; child cannot outlive its parent |
| Lineage | Maximum 32 grants including the root; invalid chains are denied |
| Revocation | Grant issuer or resource owner can revoke; no un-revoke operation |
| Run | One owner, one resource and one pinned grant |

Effective permission requires every ancestor to be active and internally consistent.
The leaf must name the requesting actor and resource, and cover the requested
actions. Permission is checked before run creation and at protected reads, claims
and completion. Run creation requires all actions used by the full template.
Changing to an independent grant requires a new run; there is no implicit rebind.

## Three-party sharing example

```text
Alice owns resource R
  ├─ Alice → Bob: source.read, may delegate, expires T
  │    └─ Bob → Carol: source.read, no onward delegation, expires before T
  └─ Alice → Carol: independent source.read grant
```

Bob cannot add `source.write`, cross to resource S, extend the expiry beyond T or
delegate without permission. Revoking Alice → Bob invalidates the dependent Carol
grant at the next check. It does not invalidate Alice's independent grant to Carol.
The records remain for provenance; cascading revocation is an access decision, not
recursive deletion of grants or user content.

The current action vocabulary is for fixture build stages. Photo viewing, album
editing, group administration, repository contribution and application-use rights
require future domain-specific actions and enforcement adapters. No current kernel
grant maps directly to a folder ACL, signed URL or container mount.

## Revocation during work

- New protected reads, claims and completions using a revoked lineage are denied.
- Existing leases do not exempt a completion from a fresh permission check.
- A run owner can cancel their own run after revocation.
- Revocation leaves checkpoints and already accepted artifacts in place.
- Revocation does not erase downloaded copies, roll back accepted effects, terminate
  external processes or guarantee immediate offline-device invalidation.

External execution does not exist in this increment. Before adding it, define how
revocation reaches workers, terminates processes, invalidates mounts/tokens and
handles partial external effects. Offline access needs an explicit expiry policy.

## Local governance and upstream contributions — target

Sharing an app should carry a versioned architecture and tool provenance. Each
receiving group binds it to its own runtime, personal belongings and explicitly
shared resources. Software ancestry does not imply data access or group control.

A confirmed branch/share agreement should record:

- Originating tool version and architecture/policy versions.
- Local group and authenticated governor, with the scope and expiry of authority.
- Allowed functions, resources, actions and onward-delegation limits.
- Which technology contributions may be received and merged upstream.
- Required approvals, review rules, attribution and applicable licensing terms.
- Revocation consequences and any independently granted access.

The original sharer's right to receive technology changes is distinct from rights
to local files or governance. A middle party may govern its local group only within
delegated authority. Confirmation must not transfer their private keys to the mother
repository owner. These agreements and group roles are not implemented. Tool lineage metadata is
implemented, with live source-read checks and owner-only mutations; it does not
assert licensing or contribution eligibility.

## Credentials and execution — target

Give each person, service and device separate credentials. Use SSH for appropriate
Git/admin transport and authenticated encrypted service connections for the API.
Issue short-lived execution authority tied to actor, resource, run, segment and
allowed actions. Establish recovery, rotation, loss and revocation procedures before
cross-device operation. Key storage and certificate format remain undecided.

A provider/agent must not choose its principal or issue its own grant. Resolve
identity and authority in the host gateway. Model-generated code and retrieved
documents are untrusted inputs. Local workers need pinned images, bounded mounts,
explicit network rules, resource limits and verifiers separate from builders.
The provider's execution environment is not a substitute for the local sandbox.

## Data and audit handling

Run inputs and lease tokens are stored in the private SQLite document. Inputs are
not automatically redacted or encrypted. Grant events contain identities, actions
and expiry. Keep databases, backups, claim responses and actual credentials out of
source control and external prompts. Public run views omit raw inputs and leases.

Accepted transitions have local transactional events. Denied operations currently
return errors without durable denial events. Events are neither cryptographically
signed nor tamper-evident. A future gateway needs denial auditing, controlled audit
read access, retention and deletion policies, and sensitive-field handling.

## Before enabling shared remote use

Complete authenticated identity binding, authorized resource creation, domain
permissions, device trust, encrypted transport, durable denial auditing, isolated
execution, independent verification, revocation delivery and a documented threat
model. Test cross-user disclosure and cross-resource escalation alongside the
existing lineage tests. This list is a delivery boundary for this feature, not a
claim that the current local kernel supplies those controls.
