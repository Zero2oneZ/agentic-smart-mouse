# Orchestration loop planner

## Delivery boundary

**The full model-assisted planner remains planned**, not an executable planner or swarm
runtime. The [planner system prompt](../templates/prompts/orchestration-loop-planner-v1.md)
adds a reusable definition to the design. It is not installed in the Qt model
context. The current Rust engine still runs deterministic fixtures only; it has
no general agent/scheduling/executor adapter. The example JSON is a proposed
transport shape, not a core-validated or authorized command.

## Responsibility

Every Hotbox process workflow needs a loop planner that translates the user's
objective into a bounded specialist graph and revisits that plan when durable
state changes. It should answer: what is ready, what needs verification, what is
blocked, whether to revise the plan, and whether the objective is complete.

Keep six responsibilities distinct:

| Component | Responsibility | Cannot do |
| --- | --- | --- |
| Placement/composer | Propose specialist/category/coordination-role fit | Grant permissions or fabricate readiness |
| Loop planner | Propose initial graph and one next control decision | Execute tools, own status or approve itself |
| Deterministic controller | Validate transitions, policies, plan revisions and budgets | Replace verifier evidence with narrative |
| Scheduler | Dispatch ready work with leases and concurrency limits | Invent work outside a reviewed plan |
| Worker/executor | Perform one declared operation under a scoped harness | Change its identity, grant or output acceptance rules |
| Independent verifier | Check artifact/result acceptance | Silently become the producer it is verifying |

The planner can be model-assisted, scripted or a deterministic fixture. All use the
same proposal contract and checks. Small native actions may use a fixed one-step
plan with no model. Larger swarms share one parent run budget and cancellation
scope; child loops do not receive fresh unlimited budgets.

## Loop and control decisions

```text
Objective + acceptance criteria + personal environment
                         |
             Placement / specialist selection
                         |
                  PROPOSE INITIAL PLAN
                         |
       Validate graph / authority / dependencies / budgets
                         |
              Review exact revision as policy requires
                         |
                Durable approved-plan checkpoint
                         |
          +--------------+--------------------------+
          |                                         |
    Ready operations?                         Nothing ready?
          |                                         |
    Scheduler claims                          Wait on event /
    leased operations                         escalate blocker
          |
    Scoped specialists -> artifacts -> independent checks
          |
    Atomic outcome + usage + event + checkpoint
          |
    LOOP PLANNER assesses durable state
          |
          +-> dispatch_ready -> scheduler
          +-> wait ----------> bounded event/deadline
          +-> replan --------> new revision -> validate/review
          +-> escalate ------> operator/policy decision
          +-> finish --------> final verification + local report
          +-> stop ----------> fence new work + reconcile active attempts
```

The model does not drive a busy `while true` loop. Accepted state-change events wake
the controller; the planner runs only when a planning decision is needed. The
scheduler can dispatch unchanged approved ready work deterministically without
paying for another model call. Waits have explicit wake conditions and deadlines.
Deduplicate wake events and suppress no-op replan churn.

## Required input and output contracts

Planner input contains only:

- Run/task/owner scope and personal runtime-environment refs.
- Objective, immutable acceptance contract and authorized input artifact refs.
- Current plan revision and optimistic checkpoint revision.
- Permission-filtered eligible specialist/dependency versions and placement evidence.
- Available harness/backpack/runner capabilities and outstanding preflight checks.
- Durable node states, active leases and accepted verifier/artifact/receipt refs.
- Remaining aggregate budgets, reservations, concurrency, retry/replan/iteration
  allowances, deadline, cancellation/revocation observations and policy refs.

No raw credential store, global personal run history, unrestricted filesystem or
unfiltered log database belongs in planner context. Fetch particular permitted
evidence by reference; use summaries first. Provider export needs the same declared
data policy as other agents.

Return **one** action proposal: propose_plan, dispatch_ready, wait, replan, escalate,
finish or stop. Echo the run/scope/environment/checkpoint revision and budget
snapshot ref. A plan proposal declares node/version refs, typed handoffs, acceptance
checks, output destinations, allocations and termination rules. Other actions refer
to existing nodes/evidence. Concise decision summaries are stored privately with
provenance. Never ask for private model chain-of-thought.

The [example decision](../templates/examples/orchestration-loop-decision-v1.json)
is a blocked escalation for an unavailable runner, not a successful run. The future
core schema must reject unknown fields, oversized graphs, unknown references,
stale checkpoint revisions, illegal action/field combinations and fabricated
budget/authority fields. A response parser must not turn arbitrary text into a
shell command, agent creation operation or approved transition.

## State and plan revisions

Proposed future loop states: planning, awaiting_review, ready, executing, assessing,
waiting, replanning, blocked, finalizing, completed, failed, cancelled, expired.
These do not rename current `RunStatus`; introduce a versioned schema/adapter with
migration tests. Proposal action is separate from durable run state.

The controller writes state changes and their accepted events atomically using a
checkpoint revision check. Stale or duplicate planner responses cannot double
schedule work. It records a separate plan_revision ID and digest; an approved plan
is immutable. Replanning preserves original requests, finished outputs, failed
attempts and provenance. Mark superseded pending nodes and invalidate affected
acceptance/review evidence without erasing history.

Existing in-flight attempts must be quiesced or explicitly continued against their
pinned old revision before a new graph can claim the same logical work. Never
silently rebind their inputs, grants or harnesses. Resume after restart uses SQL
checkpoints and current permission checks, not the provider's conversation memory.
Late or stale attempts cannot publish results after their fence/lease is invalid.

Only policy-approved plan changes may continue without another human review.
Any expansion of requested scope, privileges, external effects or resource limits
needs an explicitly authorized revision; the planner cannot authorize it. A simple
retry within the same immutable operation creates a new attempt and consumes the
existing reservation/allowance. A changed operation requires a new revision.

## Progress, verification and termination

Completion is a predicate over required accepted artifacts and independent checks,
not the planner saying "done". The controller must also reconcile outstanding
workers/effects before terminal completion. Verification failure cannot be turned
into success by changing the report wording or removing a required check.

Bound planner invocations, specialist members, parallelism, nesting, operations,
retries, replans, total iterations, wall time, tokens/cost, scratch and output size.
Planning, placement, verification and reporting consume that same total budget.
Keep abstract cost reservations distinct from measured provider usage. Scheduling
never reserves more than the remaining amount; workers cannot reset the parent
counter by spawning a new sub-swarm.

No-progress policy compares meaningful changes in accepted evidence, completed
contracts and resolved blockers. Repeating the same failure or plan with no new
information consumes a bounded allowance, then blocks/escalates. A wait does not
spend model calls indefinitely. Failed external writes with unknown outcomes go
through receipt reconciliation before retries; do not claim exactly-once execution.

Cancellation, expired grants/deadline or exhausted hard budgets prevent new claims.
The runner requests process-tree termination, invalidates execution credentials as
supported, preserves evidence and reconciles partial effects. Never describe an
unreachable worker as stopped merely because the controller set a flag. The local
report records unresolved effects and actual termination status.

## Global definitions, private planning and UI

The reusable planner prompt, harness and generic recipe versions live in the
permission-filtered global catalog. The user's objective, composition decisions,
plan revisions, node assignments, generated SQL/code/images/video, outcomes,
reports, RAG, logs and checkpoints live in their personal/project environment.
Improving a generic recipe from selected work is a separate publication action;
no automatic training or catalog upload of personal outcomes.

Add a **Workflow plan** view reachable from a running Hotbox action and the Agents
builder. Show the objective, specialist graph, current node, verified outputs,
remaining budget, blocked requirements and next proposed action. Distinguish
proposed, reviewed, dispatched, running and verified status. Offer Pause/Resume,
Cancel, inspect evidence and review a plan diff when policy needs it. Opening the
view or selecting a menu category never invokes a model or starts a workflow.

## Implementation sequence and acceptance

1. Define typed Plan/PlanRevision/Node/Handoff/LoopDecision records, scoped SQL
   storage, checkpoint revision checks and graph/field validation. Add a read-only
   CLI inspector and deterministic decision fixture before any model integration.
2. Implement controller/scheduler transitions using the existing lease/budget
   semantics through an explicit schema/adapter extension. Preserve mandatory
   development-stage contracts and independent verification boundaries.
3. Run a synthetic local fixture: planner -> summarizer specialist -> independent
   check -> organizer/local report. Use temporary state and no provider/network/VM.
4. Add a permission-filtered model-planner adapter and the Workflow plan UI, keeping
   model output advisory. Add real workers only after runner controls are verified.

Required tests: graph cycles/missing references; stale/duplicate decisions;
unavailable tools/runner; self-review or permission expansion; budget exhaustion
including planner calls; no-progress exhaustion; parallel branch readiness;
replan during active work; verifier rejection; cancellation/revocation; unknown
external effects; restart/resume without repeating accepted work; wait wakeup and
missed-event recovery; private-output exclusion from catalog exports. UI tests stay
offscreen. No active desktop input injection or real credentials in fixtures.


## Current master optimization checkpoint

The Qt supervised-run adapter now pins `src/master-loop-policy.json` by digest
and preserves parent/follow-up lineage in private `loop_metrics` rows. Global
`hotbox optimize` is a read-only advisor over the latest 1,000 agent runs, plus
aggregate recorded context size. It identifies unknown usage, runs needing
attention and repeated prompts as investigation candidates, not proven savings.
Token fields accept recognized provider terminal-event receipts; text-only logs
remain unknown. Current process/time/output/follow-up limits are enforced, while
token checks can stop future follow-ups only after reported usage reaches the
parent limit. This is not an in-flight token cap or full Rust planner adapter.

The proposed master optimizer role is defined in
`templates/prompts/master-tree-optimizer-v1.md`. Model-assisted evaluation and
automatic promotion are not enabled. The existing native kernel remains the
intended controller/scheduler; this adapter does not create a competing scheduler.
