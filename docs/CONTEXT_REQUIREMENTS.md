# Hotbox product requirements and implementation audit

Hotbox is an OS-connected environment for building and using reusable tools and
agents through a categorized mouse launcher. Simple project chats retain the
history, documents, artifacts and decisions behind that work. Headless services
perform execution and context assembly; the master interface governs scope and
permissions. A side task must not displace the original objective.

This review reconciles the original requests with the current working tree and
the existing architecture. Its implementation sequence is in
[Tool and context delivery plan](../core/docs/TOOL_CONTEXT_DELIVERY_PLAN.md).

## Source references

The references below identify actual user messages by conversation ID and UTC
timestamp. The latest requests occurred on November 15 in America/New_York and
November 16 UTC. Quotes are short excerpts with original spelling. They are
requirement references, not a copy of the private transcripts. Earlier assistant
completion reports are not treated as independent proof of functionality.

| Conversation | ID | Subject |
| --- | --- | --- |
| A | `01a1dbed-6d94-7101-8f11-44519da683ac` | Original persistent chatbot and mouse launcher; focused history and reusable evidence |
| B | `01a1ddaf-6b7a-76a1-a54e-a4e3409ed692` | Tool source tree and separate user belongings; reusable architecture |
| C | `01a1de10-a37e-78b2-8787-846d89306c61` | System catalog, agent builder, scopes, specialist orchestration and clean presentation |
| D | `01a1df14-fe13-7680-a67c-d22a7d74f3a7` | Mouse/numpad navigation, headless jobs, feed identity, rolling build ledger and optimization |
| E | `01a1e0ee-d843-7431-8b66-aa7ad63f2c76` | This chat: project-local Ollama, headless work, context continuity, chat panel and categorized tools |

### Original requirements

| Ref | Source and UTC timestamp | User excerpt | Requirement |
| --- | --- | --- | --- |
| H01 | A · 2026-11-14 21:25:21 | “the plus adds a catergory and a specific chat opens” | Creation carries its category, build location, specifications and development context. |
| H02 | A · 2026-11-14 21:25:21 | “right click gives the choices deeper options” | Menu targets have contextual actions and deeper navigation. |
| H03 | A · 2026-11-14 23:54:15 | “clone itself into the area of focus to keep context small” | The master sees ordered summaries and opens focused work with selected history. |
| H04 | A · 2026-11-15 00:28:38 | “keep the history the scripts the io for later analysis” | Preserve originals and I/O separately from derived output; describe and reference reusable segments. |
| H05 | B · 2026-11-15 05:52:17 | “2nd mirroered location which is own by the runtime and the user's belongings” | Separate reusable source/architecture from personal runtime data. Existing design resolves this as bindings, not a literal recursive mirror. |
| H06 | B · 2026-11-15 06:00:07 | “refernence the archtecture provided first” | Reuse declared storage capabilities; materialize scoped directories only when needed. |
| H07 | C · 2026-11-15 07:56:51 | “everything to have id a connectivity tree” | Durable identities and typed relationships connect code, tools, agents, runs and evidence. |
| H08 | C · 2026-11-15 08:58:55 | “add a agents catergory to the hotbox” | Agents, tasks and skills become categorized entries; contextual + opens an appropriate builder with required fields. |
| H09 | C · 2026-11-15 09:10:13 | “agent databse should again be global” | Reusable definitions belong to a governed catalog; generations and task outcomes remain personal/project scoped. |
| H10 | C · 2026-11-15 09:21:07 | “we need a orchastratio loop planner” | Compose specialist work under one bounded controller, rather than disconnected agent launches. |
| H11 | C · 2026-11-15 09:28:43 | “as much cold code setup and execution as we can” | Prefer deterministic setup/control, clean labels and supervised headless execution. |
| H12 | D · 2026-11-15 12:32:24 | “0 launch a headless instace” | A selected menu target supplies the subject; 0 requests a headless job and period opens its contextual options. |
| H13 | D · 2026-11-15 13:39:38 | “labled based on the hotbox cxombination of choices” | Preserve launch-route identity and provenance on the resulting feed/run. |
| H14 | D · 2026-11-15 13:51:50 | “chevron collapseable” | Keep operational detail inspectable under compact, identifiable feed segments. |
| H15 | D · 2026-11-15 14:58:29 | “get this building and uitlizingg what is built” | Prioritize a complete build-to-use loop over further disconnected UI/features. |
| H16 | D · 2026-11-15 17:45:06 | “orchastrator loop and its optimization a primary concern” | Measure complete loop cost and verified outcomes before promoting improvements. |
| H17 | D · 2026-11-15 18:08:13 | “build step sql that is bindeed to the master chat and teminal session” | Link milestones, steps, issues, tests, documents and checkpoints to the same work; retain document versions and branches. |
| H18 | D · 2026-11-15 18:32:52 | “a rolling ledger” | Plans, ideas, additions and outcomes must remain linked and guide later work. |

### Corrections and requests in this chat

| Ref | Source and UTC timestamp | User excerpt | Requirement |
| --- | --- | --- | --- |
| R01 | E · 2026-11-16 03:58:06 | “most of this struf should be hjappening headless” | Background services own orchestration and retrieval; basic chats expose the work. |
| R02 | E · 2026-11-16 03:58:06 | “master gui area is like permission and scope governace” | The master is the control surface for access, scope and context sharing. |
| R03 | E · 2026-11-16 03:58:06 | “shared contextual underrstanding of whats history has bee created” | Assemble relevant permitted history without filling every conversation with all history. |
| R04 | E · 2026-11-16 04:02:26 | “mouse hotbox launcher is about creating tools and agents” | The launcher is the main creation/use/context entry point, not merely a shortcut list. |
| R05 | E · 2026-11-16 04:02:26 | “tweaking and edfint of the gui shouldint be pusing out context” | Branch side work with its own focus while preserving a resumable parent objective. |
| R06 | E · 2026-11-16 04:02:26 | “chats like projects that collect artifacts and docs” | Conversations belong to durable work contexts, with evidence and documents linked to them. |
| R07 | E · 2026-11-16 04:02:26 | “create changes to the gui as its connected directly toi the os” | GUI/host changes are legitimate scoped tool work; OS integration is part of the product. |
| R08 | E · 2026-11-16 04:06:18 | “a slide side panle that always has a color and the project label h1” | A sliding chat panel clearly identifies the project; large semantic heading, persistent identity color. |
| R09 | E · 2026-11-16 04:06:18 | “post describe the task in a post the agent repies” | Show task discussions with identifiable participants, outcomes and expandable evidence. |
| R10 | E · 2026-11-16 04:12:33 | “tool need get options catergorized in ther hhotbox menu” | Built reusable tools appear in categorized menus with eligible tool-specific actions. |
| R11 | E · 2026-11-16 04:15:29 | “take notes and make refernces to this chat” | Keep these decisions and source references discoverable for subsequent development. |

Later explicit interaction requests supersede earlier shortcut experiments: current
activation remains Ctrl + Super held for 300 ms; 4/6 select spreads. This review
does not authorize a new desktop binding. “Like Reddit” establishes a task/reply
presentation; votes, public sharing and social ranking were not requested.

## Findings from the implementation

Follow-up implementation on 2026-11-16: the first owner-scoped draft registry,
categorized menu and development-chat mapping are now implemented. See the
[current acceptance checkpoint](../core/docs/TOOL_CONTEXT_DELIVERY_PLAN.md#first-implementation-ticket)
and [executable API](../core/docs/TOOL_MENU_REFERENCE.md). The findings table below
records the earlier audit snapshot; the broader context/runner gaps still apply.

Inspection checkpoint: base commit `b64f80bc8057028a9998d478b5e05939584fe886`
plus the existing uncommitted working tree. This is not a clean release identity.
The audit reads source and existing checks; it does not certify a live provider,
fresh Rust build or full governed execution.

| Area | Implemented evidence | Missing product connection |
| --- | --- | --- |
| Mouse navigation | [Hotbox.qml](../src/Hotbox.qml), `rebuild`, `choose`, `dispatch`; [wheel.cpp](../src/wheel.cpp), `wheelItems`, `validAction` | Hard-coded root/Tools actions and generic category children. No tool registry query, tool-version selection or per-tool action manifest. |
| Tool identity | [lineage.rs](../core/src/lineage.rs), `register_tool`, `add_tool_version`, `tool_context`, `fork_tool`; [engine.rs](../core/src/engine.rs), tools/tool_versions tables | Immutable metadata exists, but lacks a live menu listing/category/action/release projection. `originating_chat` is a validated string, not a verified Qt chat mapping. |
| Catalog | [catalog.rs](../core/src/catalog.rs), [system catalog](../core/templates/system-catalog-v1.json) | Read-only design vocabulary and validation; no live entities, bindings or runtime eligibility are created by loading it. |
| Focused conversations | [backend.cpp](../src/backend.cpp), `focusWorkspace`, `contextTool` | A child receives a bounded brief, eight recent event references and up to five segment references. No durable parent-task return/result-acceptance protocol. |
| Historical evidence | [sessionledger.cpp](../src/sessionledger.cpp), `record`, `timeline`, `read`, `grant` | Ordered events and explicit links exist. Category ancestry is not task/reply identity or complete governance. |
| Reusable segments | [segmentstore.cpp](../src/segmentstore.cpp), `visible`, `create`, `add`, `read` and queue operations | Preserved artifacts, summaries and links exist. Automatic task-based attachment and return-to-parent summaries are not integrated. |
| Context assembly | [backend.cpp](../src/backend.cpp), `requestContext`, `startReply`, `toolDefinitions` | Brief + recent summaries + bounded recent messages. No first-class active objective/versioned context manifest that survives unrelated work. |
| Sliding panel/posts | [Main.qml](../src/Main.qml), `projectChatPanel`; [ActivityFeed.qml](../src/ActivityFeed.qml), `refresh`; [ActivityRow.qml](../src/ActivityRow.qml) | Presentation preview only. Posts inferred from event order; no stored reply/task-parent links. Drafts are in QML memory, not durable across restart. |
| Feed identity | [feedidentity.cpp](../src/feedidentity.cpp), `identifyFeed`; message-origin records in `Backend::append` | Route mark/color derives from traversed labels/spread/action. Stable project identity and mutable route identity need separate fields. |
| Reply authorship | `ActivityRow.kindLabel`, `Backend::append`, `AgentService::recordReceipt` | Chat labels use current `backend.model`, so changing settings can relabel old replies. Agent run/provider IDs exist, but reusable agent identity is separate and not integrated. |
| Headless supervision | [agentservice.cpp](../src/agentservice.cpp), `start`, `finish`, `recordReceipt` | Private starter directory, bounded process, output and receipts. Not general managed-source dispatch; a selected target is reference data, not execution authority. |
| Rolling build ledger | [buildledger.cpp](../src/buildledger.cpp), `call`, `guide`; [build contracts](../src/build-ledger-v2.sql) | Versioned contracts, document/evidence links and guidance exist. They do not drive the general Rust dispatch or independently accept results. |
| Policy and governance | [permissions.rs](../core/src/permissions.rs), [security boundary](../core/docs/SECURITY.md), `SessionLedger::contains` | Local fixture grants and Qt scope checks exist. Master scope currently sees the local tree; there is no authenticated remote governance service or arbitrary OS permission adapter. |
| Runtime publication | [BUILD_ROADMAP.md](../core/docs/BUILD_ROADMAP.md), B3/B4; current `Backend::launch` | No general verified-release registry/activation-to-wheel path. Provider exit success cannot publish a tool as accepted. |

### Why the recent preview does not solve continuity

`ActivityFeed.refresh` groups chronological chat events under the nearest loaded
user message. Interleaved work cannot be assigned reliably by chronology alone.
Loading an older page can change the inferred parent when the original prompt
was outside the first page. Worker events often group by source/run, but there is
no durable edge connecting that run to the chat task post.

`requestContext` supplies at most twelve recent messages within a 12,000-character
history budget, twenty recent event summaries and six segment summaries. It caps
the system text, and `startReply` bounds serialized request size. These are useful
limits, but recent UI discussions can still crowd out the build objective because
the objective and selected task references are not separate pinned inputs.

`focusWorkspace` is the strongest existing starting point. Extend it with explicit
work/task references and return receipts rather than replace it with full transcript
copying or create another chat store. Existing event/segment grants must remain
effective; context linking must not widen them implicitly.

Qt's `event_links` and `segment_grants` are category-to-evidence associations with
no expiry/revocation fields. They are not the Rust grant-lineage service. Scoped
retrieval works locally, but live governance over shared context needs an explicit
policy/adapter contract rather than a GUI toggle that claims stronger enforcement.

`SessionLedger::record` also clips event content to 262,144 characters; its summaries
are bounded excerpts. Segment artifacts instead reject oversized text and ask for
ordered parts. The new context/evidence path must track coverage and original
artifact references so a clipped log is never presented as a complete source.

## Product model to preserve

| Object | Meaning | Keep distinct from |
| --- | --- | --- |
| Category placement | An editable navigation reference to a tool, agent or workspace | Ownership, permission grants and physical file moves |
| Tool or agent definition | Reusable identity and immutable versions with declared inputs/actions | One provider process or one conversation title |
| Development context | Objective, source/version, decisions, permitted evidence and linked documents | Runtime instance and another user's private history |
| Task post | A bounded request within a work context, with replies and result links | Execution attempt or reusable definition |
| Execution attempt | One fenced worker invocation with observations and usage | Verified acceptance of its result |
| Artifact and evidence | Preserved originals, outputs, checks and derived summaries | A generated claim that work succeeded |
| Release and instance | A verified tool version and its separately bound personal runtime | Definition publication or provider exit status |

The user's “projects that collect artifacts” does not mean every task must create
a repository, chat database, VM or directory copy. A GUI adjustment can be a focused
child of the tool build and return a linked result. Code owns stable relationships;
the model may propose names, summaries and relevant context within those limits.

## Architecture reconciliation

The [context-routing table](../core/docs/ARCHITECTURE.md#hotbox-context-routing--target)
already specifies launch, edit and fork. The [agent builder](../core/docs/AGENT_BUILDER_PLAN.md)
already specifies contextual +, typed routes, categorized definitions, eligibility
and replayable Qt/core mappings. The [system model](../core/docs/SYSTEM_MODEL.md)
keeps IDs, ownership, source and belongings separate. The [product review](PRODUCT_REVIEW.md#smart-segmentation-and-context-assembly)
distinguishes work steps, evidence and execution sessions.

These documents agree with the current request. The gap is integration, not a need
for another independent scheduler, project database or permission authority.
The older [build roadmap](../core/docs/BUILD_ROADMAP.md) still controls the execution
gates. Authoring/menu/context work can be delivered while real-run capabilities
remain unavailable. A published definition is not a verified activated release.

## Decisions and proposals

Confirmed requirements: categorized tool options; focused development context;
headless execution; simple chats; master scope/governance; task/reply presentation;
linked artifacts; preserved parent flow; separate source and personal data.

Proposed implementation choices, not new user requirements: separate stable project
color from route mark; store explicit task/reply/context edges; use versioned context
manifests; show uncategorized drafts in an Unsorted view; project registry changes
through an idempotent outbox; use a small deterministic formatter as the acceptance
fixture. The delivery plan makes these choices concrete and reviewable.

## Validation and open boundaries

The earlier chat-panel pass reported 42 passing offline checks and then five
targeted Qt results including setup/cleanup. This audit did not rerun those suites.
The source shows tests for scoped history, provenance, focused handoffs, immutable
tool versions, fork grants, receipts and recovery; these are regression foundations,
not tests for the missing end-to-end product path.

Current inspection found `qmake6`, no `cargo`/`rustc` on PATH, and `uv` resolving to
a mise shim. The Ollama package installation from this chat does not establish
authenticated provider behavior, usable uv preparation or fresh core verification.
No provider workloads, production database migrations, desktop configuration
changes or new tool executions are part of this planning pass.

Required next proof: a user creates a tool in context, finds it under its chosen
Hotbox category, opens its eligible options, edits it in the same development
history, branches a side task, and resumes the original objective with cited
results after restart. Execution/activation is proved separately under B1–B4.
