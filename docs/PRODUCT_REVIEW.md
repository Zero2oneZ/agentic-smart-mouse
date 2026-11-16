# Hotbox whole-product review and release priorities

## Verdict and scope

Hotbox has a working local desktop prototype and a separately tested Rust kernel.
The architecture plans cover much more than the integrated product. The next release
should prove one complete everyday task, with deterministic setup, supervised
background work and an inspectable result, before expanding the infrastructure or
agent catalog further.

The product promise: **Ask for a job, see what it will use, let it work quietly,
and get a result you can inspect, keep or revise.** Users should not need a terminal,
a JSON editor, an agent taxonomy or knowledge of containers to complete the default
flow. The CLI remains available for developers and automation.

This review consolidates the existing plans rather than introducing another runtime
subsystem. Its release priorities govern the first user-facing slice; the linked
architecture specifications retain their detailed technical requirements. This
increment is an audit/design checkpoint, not a completed UI redesign or runner.

## First implementation follow-up

Hotbox 0.5.0 now connects the [local notes recipe](../core/docs/LOCAL_TASKS.md) to
Activity/New task, with setup checks, real private results, pause/continue/stop and
restart recovery. IDs/paths stay in Details and older controls move under More
options. The findings below record the audit baseline; the fixed recipe addresses
the first end-to-end bridge without implementing generic agents, smart retrieval
or a persistent process/PTY worker supervisor.

## Evidence reviewed and fresh verification

Reviewed the current QML views, backend/category/model paths, explicit command runner,
Git handling, session ledger/segments, storage registry, Rust kernel/contracts,
environment plans, planner prompts, test suites and existing sample screenshots.
Screenshots under `docs/` include older labels; source code governs current behavior.

Fresh checks for this review:

- `bin/build` passed.
- `bin/test` passed: 21 checks, offscreen.
- `bin/test-cli` passed: sessions, Git, segments and shared storage.
- `core/scripts/check` passed: 36 Rust integration tests, three packaging admission
  tests and the multi-process CLI fixture.

The first restricted test run could not bind local mock-server sockets and could
not write Git locks to the host runtime location. Rerunning with local socket access
and a private mode-0700 temporary `XDG_RUNTIME_DIR` passed. These were environment
limitations, not evidence of a product regression. No live desktop input, user data,
model provider, remote repository or container workload was used in this audit.
Passing these tests does not verify production isolation, novice usability, actual
parallel swarm execution or cloud deployment.

## Findings, ranked

| Priority | Finding and evidence | Required outcome |
| --- | --- | --- |
| P0 | Qt and Rust are disconnected; Agents, swarm composition and loop planning exist as plans/prompts. [README](../README.md), [core](../core/src/main.rs) | One authoritative job lifecycle exposed through a bounded Qt/core bridge; honest availability states |
| P0 | Terminal launch is a detached visible desktop terminal. [Backend launch](../src/backend.cpp), around line 534 | Background job service owns unattended tasks; visible terminal opens only on request |
| P0 | Explicit CLI command runner has a two-minute timeout, merged output and direct-process kill; no persistent queue, PTY attachment, process-tree ownership or enforced sandbox. [Command runner](../src/main.cpp), lines 212–282 | A separate supervised runner contract with real lifecycle/cancellation/resource enforcement before agent execution |
| P0 | No novice end-to-end setup and first-result experience. Model setup asks for endpoint/model/environment-variable fields. [Settings](../src/Main.qml), lines 188–198 | Guided capability check, safe default folders, connection test, useful no-model demo and clear repair actions |
| P1 | UI displays `seg_…`, `web_search`, raw processing states and copied IDs for linking. [Segments](../src/Segments.qml) | Readable work cards, selectable references and translated labels backed by stable internal codes |
| P1 | Activity detail displays the raw event payload, including command/Git JSON. [Timeline](../src/Timeline.qml) | Typed event cards and summaries; raw payload only in diagnostic details |
| P1 | Storage view exposes monorepo roots, capability IDs, template version and comma-separated capability entry. [Storage UI](../src/Architecture.qml) | Pick named libraries and destinations; technical configuration lives under Advanced |
| P1 | Saving a chat segment takes the latest 100 timeline entries, then filters chat; processing is extractive excerpts. [Backend](../src/backend.cpp), lines 1092–1112; [processor](../src/segmentstore.cpp) | Automatic task/evidence boundaries, explicit coverage, bounded retrieval and durable continuation links |
| P1 | Global generated entity IDs, friendly display names and Qt/core mappings are planned, not integrated. [System model](../core/docs/SYSTEM_MODEL.md) | One typed ID allocator/mapping service, collision-safe storage, readable contextual titles |
| P1 | Categories, workflows, memory segments, terminals and agent roles could become competing user concepts. [UI](../src/Main.qml), [plans](../core/docs/README.md) | Stable product nouns and one task-centered navigation model |
| P1 | Current category archive only changes visibility; it has no agent/schedule lifecycle. [Backend archive](../src/backend.cpp) | Agent archive fences new work/disables schedules; active attempts show real cancellation/reconciliation status |
| P1 | Background Git reports every lock acquisition failure as another active checkpoint. [Git lock](../src/gitworkspace.cpp), lines 86–93 | Distinguish busy, permission, unavailable storage and unknown errors; provide actionable recovery |
| P1 | Existing tests prove protocol/storage behavior, not a persistent headless worker surviving UI closure or cancellation of descendants | Add real temporary-worker lifecycle, concurrency and crash-recovery integration tests |
| P1 | Current saved evidence is bounded text; there is no integrated media/blob result store or safe image/video result workflow. [Segments](../src/segmentstore.cpp), [environment contracts](../core/docs/ENVIRONMENTS.md) | Scoped streamed blob/artifact storage, digests, quotas, previews and retention before real media generation |
| P2 | Catalog sharing, RAG, schedules, VM placement, image/video adapters and cloud are specified but unavailable | Deliver behind capability gates after the first reliable local task flow |

There is also a definite copy defect: the radial overlay footer still says Tab + Q
while the actual configured gesture is Ctrl + Super. Its source label is corrected
with this review; installed binaries/screenshots are not changed automatically.

## Product decisions to lock for the first release

1. **Task first.** Users see a job, its steps, progress and results. A swarm is an
   implementation detail; small jobs can be one deterministic operation.
2. **Deterministic core.** Normal code handles setup recipes, identities, validation,
   selection gates, scheduling, process launch, storage and recovery. AI proposes
   interpretation, composition and creative work where it adds value.
3. **Quiet by default.** Jobs run headlessly. No terminal-window herd, ambient
   terminal capture, hidden prompt wait or unsolicited focus stealing.
4. **Simple creation.** Default builder asks what to do, what it may use, where to
   save results and when to run. Templates resolve the detailed requirements.
5. **Progressive detail.** Main UI shows names, outcomes, status and the next useful
   action. IDs, JSON, raw logs and backend terms are available in inspectors/CLI.
6. **Personal outcomes.** Global catalog holds reusable definitions. User requests,
   generated content, task results, memory, RAG, logs and checkpoints remain personal.
7. **Honest states.** Available, preparing, working, waiting for you, checking,
   finished, stopped and could not finish reflect actual controller/runner evidence.
8. **Reversible local changes.** Show previews/diffs and version outputs. Explain
   external effects that cannot be undone; cancelling is not a promise to reverse them.
9. **Small teams.** Reuse a pinned recipe where it fits. Parallelize only useful
   independent steps, within a parent budget. No model call for every UI action.
10. **Local first, portable contracts.** Cloud workers use the same operation protocol
    with destination-local authority and bindings; no shared multi-host SQLite file.

These decisions are acceptance requirements, not claims about features already
implemented. They refine presentation and delivery order without weakening the
permission, provenance, sandbox or verification contracts.

## Clean information architecture and language

The primary menu should stay small:

```text
                 New task
     Projects                 Agents
           Library       Activity
                 Home
```

Home remains the global conversation/index view. Agents is the agreed catalog and
builder entry; its description is "Reusable helpers for your work." Library contains
saved work, sources and results. Activity contains current/background jobs and
history. Settings lives in the main window, not as another mandatory radial layer.
Project-specific actions such as Open folder/Open terminal are contextual. Recent
items/pins need a hard menu-size limit and an accessible overflow/search view.

| Internal concept | Default user-facing wording |
| --- | --- |
| Category/workspace | Project or folder, according to its actual function |
| Reusable session segment | Saved work; sections inside a task |
| Artifact | File, source, result or attachment, according to content |
| Session timeline/event ledger | Activity |
| Shared architecture / registry | Files and storage |
| Harness/backpack | Working rules / Tools and access, usually summarized |
| DAG / orchestration graph | Steps / Plan |
| Spawn/claim/lease/attempt | Start / Working / Retry, with mechanics in details |
| Swarm/member/worker pool | Team / Helper / Background work |
| Scope/grant | What this task can access |
| RAG/embedding collection | Reference library / Search your sources |
| Runtime/container/microVM | Workspace / Computer; backend choice under Advanced |
| System prompt | Instructions |
| Cron/misfire | Schedule / Missed runs |
| Rank/fitness score | Best match / Needs setup, with a reason |

Avoid showing raw IDs, enum strings, absolute paths, empty technical fields or JSON
as the primary label. A selected folder path or error diagnostic is appropriate
when it helps the user make a decision. Do not remove auditability: provide Details,
Copy reference and View logs. Stable internal state codes must be separate from
translated labels; the current segment filter passes `currentText` to the API, so
changing its text without adding a code role would break filtering.

A normal task view:

```text
Weekly research brief                    Working
Finding sources is finished. Writing your brief now.

Plan       Find sources  ✓   Write brief  •   Check results

Results    Brief.docx    Sources

[Pause]  [Stop]                      [Details]
```

Show verified step counts or specific phases, not fabricated percentage estimates
for a model call. Results appear as meaningful previews/downloadable files. Large media belongs in
scoped blob/file storage with streamed transfer and checksums, not base64 inside
chat messages, JSON events or unbounded SQL text. Show
"Needs your attention" with one concrete repair action when blocked. User-facing
errors should explain the problem before offering optional diagnostics.

## First-run and repeatable setup

A first-run path must work without editing configuration or opening a terminal:

1. Explain where personal files will live; offer one folder picker and a safe default.
2. Check installed capabilities with deterministic, versioned probes. Distinguish
   installed, ready, needs connection, unsupported and unavailable. Probe output is
   bounded and does not imply permission to install software.
3. Offer a synthetic local sample task that needs no model, internet or VM. This
   proves setup, background work, progress, saved output and restart recovery.
4. Connect AI through a named provider/local-service choice, credential handling and
   a bounded connection test. Keep custom endpoints/environment variables advanced.
   Credentials use an appropriate protected store; never ordinary manifests or logs.
5. Prepare only dependencies needed for the selected task using reviewed, pinned,
   idempotent setup recipes. Show size/resource/network requirements before an
   operation that needs approval. No global system mutation from generated scripts.
6. Persist a readiness result and repair action. Reruns reuse verified assets,
   detect stale versions and resume incomplete setup rather than starting over.

Ship installable binaries/packages so normal users do not need Rust, qmake or a
compiler. Source builds remain a developer path. Remote publishing/installation,
cloud account setup and privileged OS changes stay separate explicit actions.
Auto-setup does not mean guessing passwords, accepting licenses, widening grants
or silently installing a model/provider. Failed setup retains a useful local report
and does not mark a missing adapter ready.

## Where cold code owns behavior

| Deterministic code | Model-assisted only where useful |
| --- | --- |
| ID allocation, names/paths, scope and permission checks | Interpret an ambiguous objective |
| Template matching gates and version/dependency resolution | Suggest a specialist plan when no recipe fits |
| Setup probes, installers under policy, cache verification | Propose new instructions/skills for review |
| DAG readiness, queues, leases, retries, budgets, schedules | Creative writing, design, code/image/video generation |
| Process launch, stdin/output handling and cancellation | Suggest concise evidence-linked titles/summaries |
| Boundary detection, chunk offsets, fingerprints and dedup | Optional topic-boundary suggestions for messy content |
| Artifact validation, checkpoints, report assembly and repair rules | Explain a complex failure using verified diagnostics |

A deterministic selector may choose among known compatible recipes using explicit
contract/capability data. The model cannot substitute for missing eligibility checks.
The loop planner wakes only for decisions; ready approved steps dispatch without
re-planning. Cache reusable plans/evidence by pinned inputs and scope. Measure saved
model calls, latency and actual usage; do not invent savings claims before profiling.

## Headless worker and terminal supervision

Implement one persistent worker supervisor independent of the Qt window. It owns
queue/lease state, workload groups, output streams and termination. The app attaches
to it; closing a window does not silently kill a job. Reboot/restart reconciles
attempts and actual worker state before offering Resume. A PID alone is not durable
identity and must never authorize attaching to or killing a reused process.

Two execution modes are needed:

- **Noninteractive process:** default. Exact executable and argument vector,
  explicit working-directory binding, clean environment and declared stdin payload
  or closed stdin. Keep stdout/stderr separate, bounded and streamed to artifacts.
- **Headless terminal session (PTY):** only for declared tools needing terminal
  semantics. Private session, controlled dimensions, bounded transcript and explicit
  input ownership. Unknown prompts time out or request attention; no guessed input.

User actions: View output, Open terminal, Take over, Return to background, Pause,
Stop and Retry. Viewing output is read-only. Taking over a terminal requires a
separate input capability and clear ownership; an agent and user cannot race writes.
No terminal appears on the user's desktop unless requested. Never attach to an
unrelated terminal or inject keyboard/mouse input into the active desktop.

Launch preflight must resolve pinned tools and sandbox policy, declared mounts,
network/credential needs, CPU/memory/process/time/output quotas and current grants.
Generated scripts are inert until admitted through a declared executor and review
policy. Remove unrelated inherited environment variables/file descriptors, browser
profiles, agent sockets, display sockets and container-engine sockets.

Use a workload ownership boundary that covers descendants. On Linux, delegated
cgroup/container supervision can support resource control and whole-workload cleanup;
verify host support and permissions, and refuse unsupported required controls.
Qt's `kill()` sends SIGKILL to the process it manages; it is not a process-tree
supervisor. See [Qt QProcess](https://doc.qt.io/qt-6/qprocess.html#kill) and
[Linux cgroup-v2](https://docs.kernel.org/admin-guide/cgroup-v2.html). A cgroup alone
is not a filesystem/network sandbox or tenant-security boundary.

Cancellation records requested -> stopping -> confirmed stopped, or unresolved.
Try supported graceful shutdown, then enforce a bounded termination deadline and
reconcile remaining processes/effects. A CLI process timing out must not leave an
untracked descendant. Test descendants, detached children, no-output hangs, stdin
prompts, crashes, log floods, stale leases and UI restarts in temporary sandboxes.
Do not equate "Cancel clicked" with proof that external work stopped.

Bound herd size and per-project concurrency, queue excess tasks, and apply fairness
and backpressure. Reuse verified read-only base assets, never another instance's
private scratch or credentials. Pooling and idle cleanup must not remove belongings
or accepted outputs. Manual and scheduled tasks share one admission/budget policy.

## Identity and readable naming

Use the planned typed random UUID-based IDs from the authoritative registry; retain
legacy Qt/category/segment mappings. Enforce uniqueness in SQL. A job uses the
workflow-run identity, an operation has its own ID, a retry gets a new attempt,
and terminal/process/instance identities stay distinct. Do not create a competing
identity scheme for each adapter.

```text
Project:  Website redesign
Job:      Build the landing page
Step:     Check the layout
Worker:   Website redesign · Layout check · 2
Terminal: Layout check                         [Copy reference in Details]
```

Names are editable labels. Generate deterministic readable titles from project,
step role and a scoped ordinal; a model-written title is optional. Use the immutable
ID for private storage paths and authority, not an editable slug, date, PID, machine
name or full user request. The UI disambiguates same-name items in context; CLI short
references must fail on ambiguity instead of selecting arbitrarily. Preserve
Unicode display names, support rename history and prevent labels becoming paths or
shell arguments accidentally. Retry/clone/fork/instance lineage stays explicit.

## Smart segmentation and context assembly

There are three different boundaries:

1. **Work steps:** executable contracts with dependencies and acceptance tests.
2. **Saved evidence:** immutable sources, outputs, decisions and reusable summaries.
3. **Execution sessions:** attempts, processes/PTYs and checkpoints.

Do not split all three every fixed number of messages or use one ID/state machine
for them. Automatic segmentation should attach evidence when a job/step starts or
finishes, a distinct tool result arrives, an accepted decision changes the plan, an
output contract changes or a recoverable checkpoint is made. Optional model
suggestions refine topic grouping; code owns provenance and storage boundaries.

For large content, chunk deterministically using appropriate structure: paragraphs
and sections for text, symbols/functions for code where a parser is available,
rows/partitions/schema for data, timestamp ranges for media. Preserve parent
artifact/version refs, byte/text offsets or time spans, encoding, content digest,
redaction policy and coverage. Oversized content needs an explicit continuation or
error; it must not quietly become a supposedly complete summary of the last 100
events. Keep original redacted evidence separately from derived summaries, as the
current segment contract requires.

Use a layered retrieval flow: task brief -> relevant summaries -> selected original
chunks. Filter scope/current grants at every layer, including counts and caches.
Embeddings are optional after exact/scoped search works. Deduplicate content without
merging private authority. Revoke derived cache access when its source authority
ends. Reprocessing creates a new derived revision, not overwritten originals.

Summaries include goal, current outcome, decisions, unresolved work and citations.
Show stale/partial coverage explicitly. A title or summary is not new system policy.
Reusable evidence can be linked into several permitted tasks without copying whole
transcripts. Users see saved work cards and click to link them; copying segment IDs
belongs only in Advanced/CLI. Allow correcting a title or splitting/merging a display
group without destroying original evidence or altering completed execution records.

## Missing features worth adding

| Feature | Why it earns a place | Delivery priority |
| --- | --- | --- |
| Setup check and Repair | Resolve dependencies/connections without shell knowledge | First slice |
| One unified Activity view | Show jobs, progress, waiting reasons and actual stop state | First slice |
| Result tray with previews | Make the result, its sources and destination obvious | First slice |
| Changes preview and output versions | Inspect generated files/diffs before applying to personal originals | First slice |
| Resume after interruption | Continue checkpointed work without duplicate effects | First slice |
| Plain-language access card | "Read these files; save results here; contact these services" | First slice |
| Search and choose references | Link prior work/sources without entering IDs | First slice |
| Time/resource/usage limits | Keep background work bounded and expectations visible | First slice |
| Human-readable schedules | "Every weekday at 9" with timezone and missed-run policy | After durable scheduler |
| Personal and shared reference libraries | Reuse knowledge with explicit sharing and retrieval scope | After scoped retrieval |
| Safe support bundle | Preview/redact diagnostics before an explicit export | After core diagnostics |
| Named task recipes | Reuse tested setup/execution without rebuilding a swarm each time | First slice, expand gradually |
| Accessible keyboard/search flow | Operate without precise mouse gestures or technical knowledge | First slice |

An offline/local sample, cached definitions, clear provider-disconnected state and
no-model deterministic tasks should remain useful. Media previews must use safe
viewers; generated code or HTML is not executed just because it is a result.

Defer custom graph editing, universal cloud administration, every VM backend,
user-visible swarm hierarchy/ranks, global reputation scores and large plugin
marketplaces until repeat use demonstrates a need. Keep internal contracts extensible
without displaying all of them on the first screen.

## Consolidated implementation order

**A. Product skeleton:** friendly view models and labels, Home/Projects/Agents/
Library/Activity navigation, first-run readiness, guided access/destination selection,
empty/error states and availability flags. Keep detailed builder requirements behind
sensible template defaults; expose four everyday questions, not 12 technical forms.

**B. One complete deterministic job:** connect a typed Rust job/draft/event/artifact
service to Qt with version/availability checks and crash-safe legacy mappings. Run
one synthetic local recipe through setup, queued work, verification, saved result,
local report and restart. No model, network or VM dependency for this acceptance
fixture. Label it sample/simulation until real execution controls exist.

**C. One real supervised runner:** implement private instance/process lifecycle,
noninteractive mode, scoped bindings, effective sandbox/resource policy, cancellation
and logs. Add PTY only for a declared tested need. Keep one backend working well
before adding the others. Hook Activity/results to actual runner events.

**D. Smart evidence and model composition:** deterministic segmentation and scoped
retrieval, then optional planner/composer assistance over the same typed job contract.
Enforce aggregate budgets, independent checks and local/private outcomes. Expand
from a simple role to small useful teams with measurable evidence.

**E. Repeatability and sharing:** installable tested recipes, persistent schedules,
shared libraries/catalog publications, backup/restore and optional remote workers.
Cloud requires authenticated identity and transactional state/blob adapters; a
container image alone does not deliver cloud orchestration.

These are end-to-end slices, not a demand to finish every general registry and
future infrastructure component before anyone can use the product. Each slice
must have real UI behavior, CLI parity, migration/rollback evidence and failure
handling; unavailable capabilities stay clearly unavailable.

## Release gates

Before describing the product as ready for everyday users:

- A new user completes the sample task without editing JSON, setting an environment
  variable, copying an internal ID or opening a terminal.
- A supported real task prepares only required dependencies, obtains any necessary
  concrete authorization, then runs without unattended interactive prompts.
- Closing/reopening the UI retains job status, results and current controls.
- A reboot/crash resumes or offers a truthful recovery path without duplicate writes.
- Stop reaches all owned workload descendants, or reports unresolved termination.
- Concurrent same-name jobs receive unique IDs/private workspaces and do not cross
  outputs, credentials, memory, logs or terminal input ownership.
- No accepted output or personal original is overwritten silently. A documented
  restore/version route exists for supported reversible changes.
- Missing tools, wrong credentials, denied scopes, full disks and expired leases
  yield a readable reason and useful next action; no false Ready/Done status.
- Automatic segmentation preserves full declared coverage and citations, and cannot
  leak private evidence through retrieval, titles, cache hits or catalog exports.
- Main controls work via keyboard and accessible names at supported window sizes,
  text scaling and languages. No raw enum is used as a translated API parameter.
- First-run/task usability is observed with nontechnical users; unit test counts
  alone are not evidence of understandable setup or successful completion.

Every implementation milestone should update a small implemented/available/planned
capability matrix. Existing architecture documents are reference contracts; this
review is the first-release product sequence and acceptance checklist.
