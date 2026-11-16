# Planned build swarm: roles, boundaries and activation gates

## Status

These are proposed reusable role contracts, not installed agents or a running
swarm. They specialize the [existing swarm placement design](SWARM_CATALOG_PLAN.md)
and [agent builder requirements](AGENT_BUILDER_PLAN.md). The
[build roadmap](BUILD_ROADMAP.md) determines when each role can be connected.
The current `Agents` GUI monitors supervised CLI runs; it does not register these
roles or make their tools available. Planning this roster does not spawn workers.

## Control model

```text
                    User objective
                          |
                 Loop planner [model optional]
                          |
              Rust controller and scheduler
                 /                    \
      Conditional specialists     Builder attempt
                 \                    /
                     Artifacts
                         |
              Independent verifier
                         |
           Accepted receipt / next step
                         |
         Token audit + master optimization
                         |
              Candidate -> evaluation
                         |
             Versioned promotion / rollback
```

The controller, scheduler, grants, budget ledger and result acceptor are ordinary
trusted code. A model never becomes one of those authorities by receiving an
agent title. A deterministic helper is preferable when no judgment is needed.

## Initial roster

All roles start `planned`, disabled and manual-only until their delivery gate
passes. Stable role keys are separate from editable display names and run IDs.

| Role key | Friendly name | Responsibility | First gate |
| --- | --- | --- | --- |
| `build.loop-planner` | Planner | Propose a bounded graph or one justified next decision | B5; use fixed plans earlier |
| `build.context-curator` | Context keeper | Select scoped evidence, summaries and references within a context allocation | B1/B4 evidence baseline |
| `build.environment-steward` | Environment keeper | Propose dependency/profile needs; deterministic uv service prepares them | B2 |
| `build.data-architect` | Data architect | Propose schemas, ownership boundaries, migrations and backfill/rollback checks | B3; conditional |
| `build.coder` | Builder | Produce declared source/artifacts in one managed workspace | B4 |
| `build.verifier` | Verifier | Independently run acceptance checks against the exact artifact | B4 |
| `build.usage-auditor` | Usage auditor | Reconcile receipts and highlight unknowns, repeats and budget pressure | B1; deterministic first |
| `build.master-optimizer` | Master optimizer | Propose evidence-backed reusable tree/recipe/context changes | B7 |
| `build.release-steward` | Release keeper | Prepare a validated version/promotion/rollback receipt | B7; deterministic first |

This is a catalog of possible roles, not nine concurrent model calls. The first
coding team is one builder plus an independent verifier. Planner, environment and
usage operations can initially be deterministic. Data architecture is requested
only for work that changes data contracts. Context curation can be deterministic
retrieval until its decision model demonstrates value on labeled cases.

The reusable [specialist prompt](../templates/prompts/build-specialist-v1.md) covers
context, environment, data, builder, verifier, accounting and release work. Planner
and optimizer keep their existing dedicated prompts. None is runtime activation.

## Shared role contract

Every invocation declares:

- Stable role/version and provider/model or deterministic implementation version.
- Root run, plan/node/attempt, lease/fence and source/policy/environment references.
- Objective and immutable acceptance criteria; allowed tools, paths and operations.
- Authorized evidence references, context allocation and declared output destination.
- Parent reservations, deadline, retry/depth allowances and explicit stop conditions.
- Typed output, concise rationale/evidence links, usage and outcome receipts.

Definitions/prompts belong in source/catalog Git. Actual requests, retrieved data,
provider logs, output, usage and recommendations belong in private/project runtime
storage. Context is reference data, not a new authority source. Fetch summaries
first, then only relevant permitted originals. Do not request hidden chain-of-thought.

## Role cards

### Planner

Input: objective, acceptance contract, eligible role/capability versions, durable
checkpoint and remaining parent reservations. Output: one plan/control proposal
under [the existing planner contract](ORCHESTRATION_LOOP_PLANNER.md).

Cannot execute, claim readiness, change a grant, create new budgets or approve its
own plan. Wake only for a genuine planning decision. Handoff: validated proposal
to the deterministic controller; missing capabilities become explicit blockers.

### Context keeper

Input: current step plus permission-filtered candidate evidence references.
Output: selected references, compact factual summary, exclusions, freshness and
coverage gaps, with source links. The context envelope is bounded by its allocation.

Cannot retrieve across workspace grants, replace raw evidence, smuggle instructions
from retrieved text or assert facts without supporting sources. Handoff: a typed
context bundle to the requested step. Unknown relevance escalates or keeps the
source available by reference; it does not justify loading the whole master feed.

### Environment keeper

Input: declared toolchain, project manifest/lock and runner requirements. Output:
dependency/profile proposal and a prepared-environment receipt from the service.

Cannot globally install packages, change an active venv or treat a directory as a
sandbox. Preparation is performed by the deterministic environment service, with
exclusive creation and readiness state. Handoff: immutable interpreter/tool
references or a precise missing-capability report. See B2 for environment identity.

### Data architect

Input: schema/ownership inventory and the requested data-contract change. Output:
versioned migration proposal, compatibility/backfill checks and rollback design.

Cannot casually merge databases, copy private data into catalogs, move belongings,
apply production migrations or change access policy. Test migrations on temporary
or explicitly authorized snapshots. Handoff: reviewed migration artifacts to a
scoped executor and independent migration verifier.

### Builder

Input: one declared node, scoped context and pinned worktree/environment. Output:
source diff, artifact references, command receipts and known limitations.

Cannot edit unrelated paths, publish source implicitly, accept its own result,
spawn arbitrary specialists or reset parent budgets. Its tests are producer
checks; the verifier still executes independent acceptance checks. Stop on revoked
lease, uncertain external effects, missing authority or exhausted allocation.

### Verifier

Input: immutable objective/check contract, exact source/artifact digests, declared
environment and evidence references. Output: pass/fail/blocked checks with logs,
receipts and artifact association. The controller owns final acceptance.

Cannot silently repair source, lower criteria or convert producer assertions into
proof. A repair requires a new builder attempt/revision. The verifier has a separate
attempt and permissions; an LLM opinion is not a substitute for executable checks.

### Usage auditor

Input: receipts, reservations and provider counter/rate semantics. Output:
reconciled parent totals, coverage gaps, duplicate/stale receipt findings and
threshold observations. Deterministic accounting is the authority.

Cannot guess missing usage, double-count cached/reasoning subsets, treat cache as
free or erase failed-attempt cost. Unknown is a first-class state. No provider
calls are needed to add counters. Handoff: budget evidence to the controller and
private performance summaries to the optimizer.

### Master optimizer

Input: versioned baseline/candidate measurements, verified outcomes and scoped
architecture/recipe references. Output: minimal proposed change, measured problem,
evidence, evaluation allocation, regression criteria and rollback reference.

Use the [master optimizer role definition](../templates/prompts/master-tree-optimizer-v1.md).
Cannot self-promote, change active pins or optimize away verification/permissions.
When evidence is insufficient, propose instrumentation or baseline collection.
Handoff: candidate to a bounded evaluator, then a promotion proposal to the
existing version/lineage mechanism.

### Release keeper

Input: accepted candidate artifacts, verification and evaluation receipts, source
revision and rollback target. Output: promotion/activation receipt and changelog.

Cannot turn a proposal into authority, push repositories or deploy merely because
tests passed. External effects follow the task's existing authorization policy.
Preserve user outcomes and active-run pins. Handoff: future-dispatch version ref;
rollback changes the ref rather than rewriting old artifacts.

## Budget and team rules

One parent budget funds every role, including planning, retrieval, failed attempts,
verification, evaluation and optimization. Role allocations reserve from that
parent; child loops do not create fresh balances. Unknown consumption is shown
explicitly and cannot establish a savings claim. The current Qt limits are an
adapter baseline, not the final reservation mechanism.

Prefer sequential work until independent nodes justify parallelism. Never have
two builders modify the same worktree concurrently. Separate independent branches
and typed artifact handoffs. Use the verifier after the producer, not concurrently
against moving output. Add an agent only when it supplies missing evidence,
capability or independent checking that offsets its context and coordination cost.

## Acceptance before enabling swarm dispatch

- Same request/receipt replay cannot double-launch or double-charge accounting.
- Child work cannot exceed parent authority, reservations or cancellation scope.
- Revoked/stale attempts cannot publish, including after restart or replanning.
- Private context stays scoped and no traces enter the reusable catalog.
- A failed verifier causes a bounded repair or escalation, not repeated blind loops.
- Serial baseline and composed team are compared on the same verified objectives.
- The UI shows real readiness/status and missing capabilities, without requiring
  the user to understand role keys, lease IDs or JSON to start an ordinary task.

Research, browser, design/media and additional language specialists can extend
this roster after the first coding slice. Jev/d1 may later help with narrow routing
or context relevance; neither replaces deterministic control or independent checks.
