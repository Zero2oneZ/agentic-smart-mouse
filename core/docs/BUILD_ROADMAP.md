# Build roadmap: one working loop, then measured swarm execution

## Status and authority

This is the near-term delivery backlog beneath [PLAN.md](PLAN.md), not a second
orchestrator or a claim that the proposed stages are already available. Role
contracts live in [PLANNED_SWARM_AGENTS.md](PLANNED_SWARM_AGENTS.md). Existing Rust
plan, lease, grant, checkpoint and verifier contracts remain authoritative.

The [implemented build ledger](BUILD_LEDGER_API.md) now tracks this delivery work
in private SQL: hierarchical steps, immutable documents, checkpoints, provenance
forks, and links to Master chat, agent runs and recorded terminal commands. It
indexes plans/evidence and does not replace the execution controller. Immutable
acceptance contracts and bounded rolling guidance now feed supervised worker
prompts and Master-loop inspection; general leased dispatch remains a later gate.

As of this build checkpoint:

- Implemented: local notes execution/recovery; private data/catalog separation;
  supervised CLI processes; saved run output; scoped chevron feed; pinned Qt loop
  policy, parent follow-up lineage and a read-only optimizer report.
- Last verified: 34 offline Qt checks and the CLI workflow pass. Three TCP mock
  server checks are excluded. Provider behavior is tested with a fake provider.
- Missing: Cargo for a fresh Rust build in this session, authenticated coding-run
  evidence, reliable live provider usage, independent general artifact acceptance,
  native kernel dispatch of CLI agents, uv environment preparation, interactive
  PTY attachment and a model-assisted master optimizer.
- The current Agents screen is a run monitor, not the proposed reusable agent
  builder. Current headless jobs use private starter directories; they do not
  automatically edit the selected project's source.

## Product milestone

A user selects a project, states one bounded change and receives a tested artifact
with a receipt. They can inspect context, planning, execution, verification,
usage and failures from collapsed feed rows. Hiding the window preserves work;
restart reconstructs durable state without guessing whether effects occurred.

```text
Selected objective + acceptance contract
                    |
          Scoped evidence and plan
                    |
       Pinned source / policy / environment
                    |
        Rust controller + leased dispatch
                    |
             One builder attempt
                    |
       Independent checks + artifact digest
                    |
           Durable accepted receipt
                    |
         Visible result / bounded next step
                    |
       Baseline -> candidate -> measured promotion
```

No swarm, scheduler UI or decision-model dependency is needed to prove this path.
Model-assisted planning may initially be replaced by a fixed one-task template.

## Delivery stages

| Stage | Deliverable | Depends on | Completion evidence |
| --- | --- | --- | --- |
| B0 Baseline | Reproducible full build, explicit capability checks and data-location inventory | Existing app | Fresh Rust + Qt build; existing offline checks and network tests on a capable host; rollback/install check |
| B1 Receipts | Provider adapter emits normalized, versioned attempt and usage receipts | B0 | Recorded/replayed success, failure, cancellation, truncated output, missing usage and duplicated receipt cases |
| B2 Environments | uv-backed preparation and environment registry | B0; coordinate receipt format with B1 | Recreate a locked environment, reuse it, change lockfile without modifying an active environment, reject unresolved/missing dependencies |
| B3 Native bridge | General runner dispatch through existing Rust plans/leases/checkpoints | B1, B2 | One declared operation claimed once; duplicate requests do not double launch; stale/revoked attempts cannot publish |
| B4 Verified coding slice | One real coding task in an explicitly managed source worktree | B3 | Independent checks pass for the exact artifact/source digest; failure and restart recovery demonstrated |
| B5 Loop control | Event-driven dispatch, waits, bounded retries/replans and parent reservations | B4 | Duplicate wakes are harmless; no-op events cause no planner call; aggregate budget/no-progress limits stop work |
| B6 Small swarm | Conditional specialist composition under the same controller | B5 | Serial and parallel baselines compared; context/grant boundaries hold; child work consumes the parent budget |
| B7 Master improvement | Model-assisted optimizer proposes, evaluates and promotes candidate revisions | B1 baseline capture; promotion requires B4-B6 | Quality/resource comparison on held-out tasks, immutable promotion receipt and tested rollback |

B1 and B2 may be developed independently after their shared receipt/interface
contract is agreed. That does not authorize parallel agent execution today. Do
not start B6 simply because several specialist prompts exist.

## B0: establish the buildable baseline

Use `bin/build`, `bin/test`, `bin/test-cli`, `python tests/install_local_smoke.py`
and the shell-plugin checks. `--desktop-only` reuses the existing Rust helper;
`--offline` excludes three network tests explicitly. Neither substitutes for the
full-build/provider gate. Record compiler/helper/source versions and test scope.

Keep production state out of tests. Check `hotbox doctor`, backups, source-tree
placement and restore behavior. Do not migrate user data or change desktop
bindings merely to make a test pass.

## B1: trustworthy provider and usage receipts

Start with one authenticated provider adapter, then add the second against the
same contract. Reuse the existing supervisor; move authority into the core bridge
rather than building a parallel scheduler.

A receipt must identify root run, plan/step/attempt, lease/fence, source revision,
policy revision, environment, provider/model version, event sequence, output
references, timestamps and outcome. Credentials are references, never values.
Capture input, output and cache categories using explicit provider semantics:
cached input may be a subset for one provider and a separate bucket for another.
Do not add reasoning-token subsets to output twice. Distinguish incremental from
cumulative receipts, deduplicate them, and mark incomplete usage as partial/unknown.
Money requires a pinned rate version; unavailable rates mean unknown cost.

The current log parser accepts recognized structured terminal usage records.
Ordinary CLI text is unknown. It is a transitional adapter, not authoritative
provider metering. Do not call the model again to estimate its own consumption.

## B2: Python environment ownership

Project Git owns `pyproject.toml`, `uv.lock`, supported Python constraints and the
selected dependency profile. The private environment registry owns installation
state, interpreter path and environment identity. A useful identity includes
project, lock digest, Python implementation/version/ABI, OS/architecture, dependency
groups, relevant accelerator configuration and editable source revision.

Prepare an environment at its final path under an exclusive preparation lease,
then mark it ready. Do not rename a populated venv as a publication mechanism:
entry-point scripts can contain absolute interpreter paths. Never sync packages
into an environment used by active runs. Share uv's package cache, not one mutable
site-packages directory across unrelated projects. Record native/system library
requirements separately; a venv is not an OS sandbox.

Python packages, environments, model weights, data and logs are different stores.
The database stores references and receipts, not package/model blobs. A missing
package is a preparation failure, not permission for a worker to install globally.

## B3: one durable control owner

```text
Git: reusable definitions / templates / lockfiles / policies
                              |
Qt UI -- requests --> Rust orchestration DB -- leases --> Runner adapter
   ^                         |                              |
   |                         +<-- accepted receipts --------+
   |
Private chat/event DB <--- replayable event projection / outbox

Environment registry ---> prepared runtime reference
Artifact store ---------> immutable output reference + digest
```

Rust owns accepted workflow transitions, leases, reservations and completion.
Qt owns presentation, chats, private evidence and references. The environment
service owns preparation state. Workers produce observations/artifacts; verifiers
produce checks. Neither owns its own acceptance decision.

Two SQLite files are not one atomic transaction. Use versioned request IDs,
idempotent receipt acceptance and a durable outbox/replay contract. A missed UI
event must be recoverable from the core. Do not rewrite either database in place
into a new unified schema. Map existing run IDs explicitly and fence old attempts
at any migration boundary; interrupted work must not silently execute again.

## B4: first real coding task

Use a small fixture project with known tests. Select an exact source revision and
create an authorized managed worktree. Do not silently copy uncommitted source or
write into the user's original folder. Put private prompts, provider traces and
results outside pushed source. Declare allowed files, checks and output artifacts.

Run one builder, then a separate verification attempt against the produced digest.
A zero provider exit code is not task completion. Verify build/tests, requested
behavior, change scope and required outputs. A producer's summary is evidence to
inspect, not a verification receipt. Demonstrate cancellation, failed checks,
restart and stale-attempt rejection. The output terminal follows the same attempt;
interactive PTY attachment is a later capability and is not required for this gate.

## B5-B6: bounded loop and small teams

Wake on accepted state changes or a declared deadline. Dispatch unchanged ready
nodes deterministically. Invoke the planner only for an initial graph or a real
planning change. Every parent budget includes planning, context selection,
workers, verification, failed attempts, retries and optimization. Reservations
must handle concurrent claims; checking reported totals after completion is not
an in-flight cap. Reserve worst-case provider output where supported and reconcile
actual usage. If a provider cannot honor a limit, expose that capability gap.

Bound fan-out, depth, retries, replans, wall time and no-progress events. Unknown
external effects require reconciliation before retry. A worker or verifier cannot
grant itself more scope or budget. Result acceptance rechecks live lease/fence,
policy and artifact identity. Start with one builder and one independent verifier;
add roles only when their evidence or expertise is needed.

## B7: improve the master without destabilizing active work

The [master optimizer](../templates/prompts/master-tree-optimizer-v1.md) reviews
private, permission-filtered receipts and proposes a minimal reusable change. It
can target context selection, recipe structure, model routing, redundant calls,
verification order, tool reuse or waiting behavior. It does not inspect arbitrary
personal histories or weaken success criteria to lower token counts.

Compare baseline and candidate on the same versioned tasks and acceptance checks.
Track verified success rate, total reported tokens per verified success, failure
cost, elapsed time, unknown-usage coverage and regressions. Unmeasured work must
not disappear from the denominator. Count the optimizer's own cost. Use held-out
cases to check that improvements generalize beyond the tuning examples.

Promotion writes a new immutable definition/policy version and rollback reference.
Existing jobs keep their pinned version. Normal low-risk promotions may be
policy-authorized; scope, authority or budget expansion requires the existing
explicit authorization contract. Rollback changes future dispatch, not historical
receipts or the identity of active attempts.

## Deferred until the loop is proven

Interactive PTYs, automatic browser replay, schedules, cloud/container runners,
remote sharing, media specialists, Jev/d1 routing and a general visual agent builder.
These plug into the same boundaries later; none should become another controller.

## Next implementation ticket

B1: create a provider-receipt adapter for one authenticated coding run, with pinned
model identity and explicit unknown/partial usage, then test replay/deduplication
against temporary private state. Define its Rust acceptance mapping before adding
another UI or automatic optimizer call.


## Build ticket checklist

Each implementation ticket names its stage and role(s), the existing contract it
extends, expected files/modules, state ownership, schema/migration impact, pinned
inputs, parent allocation, output/receipt schema, failure recovery and tests.
Close the ticket with actual commands/results and remaining limitations. A prose
plan, successful provider exit, or passing fixture is not proof of a real build.

The planned specialist prompt is
[build-specialist-v1.md](../templates/prompts/build-specialist-v1.md). It supplies
role boundaries, not an executor schema or an installed capability.
