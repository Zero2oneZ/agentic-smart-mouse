# Development, verification and extension

## Source map

| Path | Responsibility |
| --- | --- |
| [Cargo.toml](../Cargo.toml) and [Cargo.lock](../Cargo.lock) | Package, dependency and compiler requirements |
| [lib.rs](../src/lib.rs) | Public exports and typed errors |
| [tasks.rs](../src/tasks.rs) | Fixed local notes recipe, durable task controls and create-only results |
| [environment.rs](../src/environment.rs) | Typed backpack requirements and read-only portable environment plans |
| [catalog.rs](../src/catalog.rs) | Typed system catalog, structural validation and hashing |
| [lineage.rs](../src/lineage.rs) | Immutable tool versions, selected development contexts and fork metadata |
| [model.rs](../src/model.rs) | Templates, graph validation, input/output types and fixture evidence |
| [permissions.rs](../src/permissions.rs) | Resources, grant lineage and revocation |
| [engine.rs](../src/engine.rs) | Database format and transactional workflow operations |
| [main.rs](../src/main.rs) | JSON request parsing, CLI commands and bounded fixture loop |
| [Template](../templates/segmented-build-v1.json) | Eight-stage successful protocol fixture |
| [kernel.rs](../tests/kernel.rs) | Deterministic library integration tests |
| [check](../scripts/check) | Formatting, lint, tests, build and CLI fixture |
| [fixture_workflow.py](../scripts/fixture_workflow.py) | Fresh-process end-to-end CLI exercise |

Unsafe Rust is forbidden by the package lint configuration. No build step modifies
the desktop, user libraries, system config or Qt runtime. Build output is ignored
under `core/target/`. The Rust crate is independent of the qmake build.

## Verification

Run `core/scripts/check` from the repository root after fetching locked dependencies.
The script uses offline Cargo operations and treats Clippy warnings as errors.
Tests use temporary directories/databases and local manual clocks, with no model
endpoint or external repository. The CLI fixture uses real system time with one
shared future grant expiry and cleans up its temporary database.

To work on one component from `core/`:

```sh
cargo fmt
cargo clippy --locked --offline --all-targets -- -D warnings
cargo test --locked --offline --test kernel ancestor_revocation
cargo doc --locked --offline --no-deps
```

The last command generates navigable Rust API documentation. The behavior references
in this directory remain necessary: signatures alone do not capture revocation,
replay, deadline and fixture boundaries.

## Current test coverage

The existing suite has 46 library integration tests (24 kernel, five catalog, seven environment and ten local-task tests). This table names the tests so
behavior can be traced directly to executable checks.

| Test | Behavior covered |
| --- | --- |
| `mandatory_stages_cycles_unknown_dependencies_and_version_are_rejected` | Stage coverage, dependency direction, cycles, unknown references and invalid contracts |
| `every_previous_stage_segment_is_a_required_barrier` | Multiple preceding-stage segments must all be dependencies |
| `unknown_executors_and_unknown_template_fields_fail_closed` | Unsupported executors and extra template fields are rejected |
| `full_fixture_pipeline_completes_with_pinned_context_and_no_lease_in_views` | All stages, digests, bounded context, counters and token omission |
| `duplicate_start_is_idempotent_and_cannot_rewrite_the_template` | Exact start replay and immutable pinned requests |
| `checkpoints_survive_reopening_without_repeating_completed_steps` | Fresh connection resumes after completed work |
| `concurrent_connections_do_not_double_claim_and_stale_workers_cannot_finish` | Overlapping live connections, lease takeover and stale completion rejection |
| `exhausted_expired_leases_fail_instead_of_looping_forever` | Lease-expiry attempt limit |
| `fixture_failure_retries_only_up_to_the_segment_limit` | Reported failure retries and terminal failure |
| `operation_and_cost_budgets_stop_work_before_another_claim` | Both global budget checks |
| `global_deadline_and_cancellation_invalidate_outstanding_work` | Deadline rejection, persisted expiry and cancellation |
| `fabricated_output_evidence_is_rejected_without_completing_the_step` | Altered output map does not complete work |
| `delegation_can_only_narrow_actions_expiry_and_resource` | Delegated scope and lifetime cannot expand |
| `delegation_requires_explicit_authority_and_cannot_mint_root_grants` | Explicit delegation, root ownership and revocation authority |
| `ancestor_revocation_is_transitive_and_independent_grants_survive` | Derived access ends while independent grants remain usable |
| `revocation_blocks_active_completion_and_future_claims_but_allows_cancellation` | Revocation during a live attempt |
| `grants_expire_and_other_actors_cannot_read_or_change_runs` | Expiry and run-owner boundaries |
| `delegation_depth_is_bounded` | Maximum permission lineage depth |
| `unrelated_databases_are_not_migrated` | Existing non-kernel SQLite data is not adopted |
| `symlinked_database_locations_are_rejected` | Unix symlink-path rejection without target creation |

The CLI fixture adds template validation, partial bounded-loop execution across
fresh processes, exact start replay, cursor paging, token omission, delegated
revocation, owner cancellation and independent access preservation.

These tests do not prove authenticated multi-user isolation, actual simultaneous
thread/process races, OS sandboxing, container termination, real Git fork safety,
network security, model behavior or build correctness. The overlapping-connection
test issues calls sequentially against live connections; it is not a stress test.
Add targeted concurrent/crash tests before introducing production workers.

## Adding or changing a template

1. Read the [contract reference](CONTRACTS.md).
2. Assign a version and retain all mandatory stages, actions and dependency barriers.
3. Validate the file with the CLI and run fixture tests.
4. Start a new run for a changed template; do not rewrite an active run.

Changes to whitespace do not change the template digest, but segment array order
does. A version string alone is not an integrity proof; runs also pin the full
template and returned digest. No signature/trusted-template registry exists yet.

## Adding a real executor

Do not introduce a generic `shell` executor as a shortcut. Before accepting a new
executor kind, specify its trusted adapter, required actions, exact inputs, runtime
mounts, environment, network policy, output/artifact format, independent verifier,
limits, cancellation and idempotency behavior. Test denied access, stale leases,
partial effects, retry and revocation during execution.

The current engine deliberately recomputes only fixture evidence. Production
verification must not let a worker or model supply its own approval/check result.
Changing the executor vocabulary and report contracts requires a versioned contract
update, not a magic string in a stored template.

## Schema and policy changes

Kernel schema 3 includes transactional additive migrations from schemas 1 and 2; there is no general migration framework.
Do not change stored run/grant JSON compatibility while silently keeping the same
schema identifier. Define migration direction, backup requirements and failure
behavior; test both an empty installation and existing accepted checkpoints.
The Qt database remains separate and must not be migrated implicitly.

Actions are an explicit vocabulary. New domains such as photos, app use or group
administration need defined resource/action semantics and tests. Do not implement
wildcard authority merely to avoid enumerating domain permissions.

## Adapter rules

- Keep provider, UI, network, Git and runtime-specific dependencies out of the kernel.
- Derive principals from an authenticated host, not JSON tool arguments.
- Keep SQL authoritative; caches and model conversations cannot rewrite truth.
- Propagate resource, run, segment, template version and artifact references explicitly.
- Recheck live permission lineage at protected boundaries.
- Never treat stored scripts or retrieved documents as privileged instructions.
- Preserve personal runtime data outside source exports, forks and shared manifests.

The next work should follow [PLAN.md](PLAN.md), with the acceptance criteria made
executable before enabling agent-driven orchestration.

Tool registry tests additionally cover immutable versions and reopened contexts,
independent fork identity and grants, revoked source access, ownership and metadata
validation, and preservation of an in-flight workflow through schema-1 migration.
The CLI fixture exercises all four registry commands across fresh processes.

The catalog suite exercises path traversal and overlapping roots, identity/prefix
collisions, unknown lifecycle/relation references, terminal-state reopening,
unreachable states, states with no terminal path, unsupported contracts and digest
pinning. CLI checks validate a catalog round trip, reject a malicious path and
confirm no database was created. Runtime graphs and filesystem provisioning are
outside these tests because the catalog has no such adapters.

Environment tests cover local/cloud hashes, always-nonlaunchable plans, trust-zone
mount and network restrictions, reference-only inputs, pinned backend requirements,
simulation separation and bounded contracts. Three Python packaging tests use dry
runs to reject mutable/injected base references and invalid engine/tag inputs and
check the narrow build context. CLI fixtures validate backpacks, plan environments,
reject trust crossings and confirm no database side effects. Run them all with
`core/scripts/check`; no container daemon is needed for this suite.

The local-task suite covers restart/resume, pause/cancel, owner boundaries, immutable
start replay, conflicting results, interrupted publication recovery, independent
project queues, bounded inputs, migration and symlinks. The CLI fixture exercises
these operations across fresh processes. Qt's real-helper tests live in
`tests/hotbox_tests.cpp` and use only synthetic inputs and temporary state.
