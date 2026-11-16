# Portable environments, capability backpacks and the ingestion boundary

## Scope and implemented behavior

The local CLI is the first interface. A cloud worker should consume the same
versioned environment contract; host-specific filesystem paths, credentials,
runner endpoints and live authority are resolved separately at the destination.
This increment implements structural validation and **read-only planning**, not an
execution adapter, live monitor, cloud deployment or authorization service.

Implemented:

- Typed, hashed backpack and environment request contracts.
- Simulation, bubblewrap, OCI and microVM backend selection in plans.
- Four example backpacks: simulation, ingress observer, quarantine processor and
  trusted pipeline.
- Trust-zone storage/network restrictions and bounded declared resource budgets.
- Local/cloud targets and explicit Linux amd64/arm64 platform requirements.
- A narrow source-only Containerfile and build helper requiring digest-pinned bases.

All environment plans return `launchable: false` and list unresolved requirements,
starting with `runner_not_implemented`. The installed Qt application is unchanged.
Planning does not write SQLite, allocate a durable instance, mount folders, fetch
images, verify signatures/digests, resolve bindings, issue grants or start workers.
Example artifact digests are all-zero placeholders, not actual images or fixtures.
They pass syntax validation only and must be replaced and verified before execution.

## Reusable definition, local instance

```text
Git-owned reusable definitions
  architecture version
  agent version + harness version
  backpack: abilities + artifacts + limits + storage/network requirements
                              |
                       immutable request hash
                              |
          +-------------------+-------------------+
          |                                       |
    Local instantiation                     Cloud instantiation
    CLI + local worker                      Job/API + remote worker
          |                                       |
    Local identities/grants                  Destination identities/grants
    Local storage bindings                   Cloud volumes/object references
    Local secret references                  Cloud secret references
          |                                       |
          +-------- same operation protocol ------+
                              |
                    attempt + lease + event log
                              |
                  verified output/admission receipt
```

A backpack is a versioned capability **request and dependency description**. Its
name does not imply Linux capabilities or granted permissions. Effective access is
the intersection of approved plan, harness policy, current actor/resource grants,
backend controls and resolved bindings. No backpack may authorize itself.
The eight current kernel build actions are unchanged; network/ingestion/publication
actions require a future explicit permission vocabulary and gateway migration.

A future durable instance record links its own ID to backpack/request digests,
agent/harness/architecture/policy versions, run/task/operation/attempt IDs, scope,
backend, host/platform, resolved nonsecret binding IDs, start/stop events, status,
lease and output receipts. A new instance never silently adopts another instance's
scratch, credentials, provider session or in-flight lease.

Suggested ownership layout (target, created lazily):

```text
SOURCE ROOT /                              PRIVATE BELONGINGS ROOT /
  templates/backpacks/                       scopes/<scope-id>/
  templates/environments/                      environments/<instance-id>/
  harnesses/                                      state/ scratch/ receipts/
  agents/                                         logs/ checkpoints/
  schemas/admission/                            boundary/<connection-id>/
  runners/                                        inbox/ quarantine/ staging/
  simulation-fixtures/                          datasets/<dataset-id>/versions/
```

Do not repoint existing Qt media or library bindings. Future registry integration
adds explicit environment/backpack/boundary/admission entity types in a new catalog
revision, with legacy mappings; this increment does not rewrite system-catalog-v1.

## Different backends, one contract

| Backend | Intended role | Required artifact keys | Isolation requirement |
| --- | --- | --- | --- |
| `simulation` | Deterministic recorded inputs and modeled effects | `fixture` | None; not an OS sandbox |
| `bubblewrap` | Local Linux namespace sandbox | `rootfs`, `seccomp` | Shares host kernel |
| `oci` | Portable Linux container worker | `image`, `seccomp` | Shares host kernel |
| `microvm` | VM-backed worker with a guest kernel | `kernel`, `rootfs`, `machine-config` | Separate guest kernel |

Bubblewrap constructs namespaces and filesystem views, but its policy depends on
the arguments supplied by the caller. It is not a VM or a complete sandbox policy.
See the [upstream security model](https://github.com/containers/bubblewrap/blob/main/README.md).
OCI runtime configurations describe namespace, resource and security controls;
actual enforcement belongs to the selected runtime and host, not the JSON manifest.
See the [OCI Linux runtime specification](https://github.com/opencontainers/runtime-spec/blob/main/config-linux.md).
A microVM adapter can use a separate guest kernel, but must also configure host
containment and device/network exposure. Firecracker's design documents its
KVM-backed microVM and production jailer requirements; no Firecracker integration
is implemented here. See [Firecracker design](https://github.com/firecracker-microvm/firecracker/blob/main/docs/design.md).

A simulation is a mode with fixtures and a recorded clock/event stream, not a claim
of host isolation. Simulation plans require a simulation backpack and cannot
request real source fetching, trusted data mounts or build execution. Switching
between simulation and a real backend requires a changed request and review.

No backend automatically falls back to a host shell or a weaker environment.
Before a future runner accepts work, it must attest the platform, available user
namespaces/KVM/runtime features, seccomp, resource enforcement, network controls,
mount bindings and cancellation behavior. Unsupported requirements refuse launch.
An image digest pins bytes, not vulnerability status or publisher trust; artifact
verification and admission policy are separate checks.

## Backpack contract

The examples under [templates/backpacks](../templates/backpacks/) declare:

- Immutable-definition identifiers: `id`, `version`, `architecture_ref`, `harness_ref`
  and optional `agent_ref`.
- `zone`: simulation, ingress, quarantine or trusted.
- A bounded set of requested abilities. These are not executable commands or grants.
- Artifact names with full lowercase `sha256:` digests, never mutable image tags.
- Storage references and roles, never host paths, shell expressions or mount options.
- Denied networking, or ingress-only brokered networking with endpoint registry refs.
- CPU quota in milli-CPUs (1000 = one CPU), memory MiB, process count, wall seconds,
  scratch MiB and output bytes. All budgets are positive and bounded.

Examples use placeholder artifact hashes and unresolved storage/harness references.
The parser validates their shape. Destination resolution must verify actual bytes,
trusted provenance and compatibility, then pin the resolved immutable revisions.
Secrets cannot be embedded as extra JSON fields. Reference strings are opaque
identifiers; arbitrary URLs, file paths and credentials are rejected in those fields.
Identifiers themselves are not secret stores and do not authenticate their caller.

Storage roles and aliases need runtime validation: two differently named binding
refs must not resolve to the same underlying folder/device/object namespace if that
would cross a boundary. Scratch must be per-instance, quota-bound and disposable.
Code/input mounts are read-only. No home-directory, desktop socket, container-engine
socket, host process namespace or arbitrary host device is inherited.

## Paired boundary connections: the “no-man's-land”

Here that term means an explicitly scoped inbox/quarantine area between external
observations and admitted internal data. It is not an unmonitored shared filesystem
or unrestricted bridge. If the intended future concept is a digital twin, its
simulated environment can reproduce this same boundary using recorded fixtures.

```text
OUTSIDE / UNTRUSTED                   BOUNDARY                      INSIDE

Web/API/source
      |
Endpoint policy
      |
Observer instance --write-only--> Inbox + raw artifact
  ingress backpack                       |
  brokered egress only             Receipt + digest
  no trusted libraries                   |
                                  Processor instance
                                  quarantine backpack
                                  no network access
                                         |
                                  Validation + staged output
                                         |
                                  Independent admission check
                                         |
                                  Promotion transaction ------> Trusted dataset version
                                                                  |
                                                            Pipeline / index / agent
```

The outside connection and inside connection have separate IDs, endpoints,
principals, operations and leases, correlated by a boundary connection ID and
artifact/receipt IDs. An outside observer can write to its inbox but cannot mount
trusted data, run builds or perform transformations. The quarantine processor can
read the inbox and write quarantine/staging, but has no network or trusted-store
mount. The trusted worker reads admitted inputs; it cannot mount the raw inbox.

A trusted admission controller performs the crossing. It rechecks scope and live
grants, exact artifact digest, source provenance, scanner/parser/schema versions,
quality results, retention and freshness policy. It records an admission receipt
referencing the input and resulting dataset version. A parsing result alone is
not authority to promote data. Promotion and source-watermark advancement need a
transactional/recoverable publication protocol; failed checks keep the last good
version and do not advance the cursor. Queues are bounded; full queues apply
backpressure rather than dropping evidence silently.

Promotion cannot be implemented by giving the quarantine parser a writable trusted
mount. Use a narrow service operation/object-store copy capability checked by the
controller. Labels, writable marker files and caller-supplied `approved: true`
fields are not receipts. Preserve raw originals separately from derived content;
retention, redaction and purge policies still apply to both.

Monitoring initially means declared sources and scoped connection/receipt events,
not arbitrary interception of the user's network, desktop or terminal. A future
monitor needs a schedule, polling floor, failure budget, endpoint/redirect/address
policy, storage quota, notification policy and current grants before activation.
Fetched pages, metadata and scripts remain untrusted data. Publication does not
convert embedded instructions into privileged control input.

## Local CLI walkthrough

From `core/`, after building:

```sh
./target/debug/hotbox-core backpack-example quarantine
./target/debug/hotbox-core validate-backpack templates/backpacks/quarantine-v1.json
./target/debug/hotbox-core plan-environment templates/environments/simulation-local-v1.json
```

The last command returns the instance/scope/target/platform/backend, complete request
hash, backpack hash, isolation requirement, `launchable: false` and required checks.
This is portable planning output, not a signed attestation or approval token.
The CLI reads at most 256 KiB per request file, rejects unknown fields/backends and
never opens SQLite for these commands. Change `target` from `local` to `cloud` in a
copy of the request to plan a cloud placement; storage/credentials are not exported.
The request hash changes while the backpack hash stays the same.

## Packaging the CLI for a cloud worker

[Containerfile](../Containerfile) builds the standalone CLI from Cargo.lock and
explicit source/template files. It has no default mutable base images. The helper
requires reviewed repository references pinned by full SHA-256 digest. Choose a
Rust >=1.89 Bookworm builder compatible with the lockfile and a Debian Bookworm-slim
runtime of the same target platform. Provide actual verified image references,
not the zero-filled example artifact hashes. Native builds only; cross-compilation
and multi-platform publishing are not automated.

```sh
python3 core/scripts/package_image.py \
  --builder-image "$HOTBOX_BUILDER_IMAGE" \
  --runtime-image "$HOTBOX_RUNTIME_IMAGE" \
  --dry-run
```

`HOTBOX_BUILDER_IMAGE` and `HOTBOX_RUNTIME_IMAGE` must each contain a real reviewed
`repository@sha256:<64 lowercase hex>` reference (an optional tag before `@` is
allowed). Remove `--dry-run` to build the local `hotbox-core:local` image. It never
pushes or starts a container. Dependency/base fetches during an actual build need
network access; offline Cargo verification remains separate. Base digests pin
images but do not make all build outputs byte-for-byte reproducible.

The image defaults to printing the catalog under UID/GID 65532. Stateful execution
requires a private writable database volume with suitable ownership. Do not put a
SQLite file on a shared multi-host filesystem to scale workers. The future cloud
control plane needs an authenticated API, a transactional shared SQL implementation,
object/blob storage and leased workers using the same validated operation contracts.
SQLite remains the local authority; there is no automatic SQLite-to-Postgres migration
or remote service in this increment. A CLI image alone is not the sandbox runner.

No port is exposed. No credentials, chat databases, user belongings, generated
builds or runtime logs are included in the narrow build context. Publishing a
source/CLI image and migrating user data are separate operations; neither happens
here automatically. Cloud execution must resolve destination-local identities,
secrets, storage, encryption, egress and compute limits under the destination policy.

## Verification and remaining work

The check suite covers local/cloud plan hashes, zone-crossing denial, missing pinned
artifacts, invalid backend/schema/limits, simulation separation and reference-only
mount/endpoint inputs. CLI tests check plan/validation round trips, rejected trust
crossings and absence of database side effects. Packaging admission tests run only
in dry-run mode and validate pinned references/narrow context/build-only behavior.

The container image has not been built in this session: this account cannot access
the Docker daemon socket. Tests do not prove kernel isolation, container/VM launch,
actual limit enforcement, network brokerage, malware scanning, promotion safety or
cloud deployment. Do not advertise those as implemented.

Next: persist environment/backpack/connection/receipt entities, finish reviewed
plans and identity policy, implement an offline local-file boundary fixture with
crash-safe promotion, then add one fully attested runner. Network collection and
cloud workers follow only after their controls and revocation/cancellation tests.
