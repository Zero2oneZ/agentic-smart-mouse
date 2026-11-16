# Mouse Hotbox controls and starter projects

Open the pointer-centered wheel with **Ctrl + Super held for 300 ms**, **Super + numpad +**, `hotbox keypad`, or **Mouse Hotbox** in the sidebar. Keyboard controls act only while the wheel has focus. Mouse clicking still opens a target directly; right-click opens its options.

The first directional press highlights a target. Press the same direction again to open it or navigate into its children. Holding a key does not activate it. Changing spread, page or submenu clears the selection. A keyboard interaction keeps the wheel open after releasing Ctrl + Super; Escape or clicking outside closes it. Text entry receives ordinary typing rather than wheel shortcuts.

```text
7 upper-left      8 upper         9 upper-right
4 previous spread 5 center        6 next spread
1 lower-left      2 lower         3 lower-right
0 run headless    . target options Enter confirm
```

**4 / 6** cycle **Recent → Favorites → Pinned**, with a short spread pulse. The six surrounding targets use directional positions; 4/6 are reserved for spreads. **5 twice** goes Back in a submenu or opens Master chat at the root. After selecting a target, **.** opens its right-click options (use Tab / Shift+Tab to reach actions), and **0** opens a small prompter for a headless job. Enter submits the prompt using the chosen Codex or Claude provider. **Page Up / Page Down**, or clicking **More**, pages through additional targets. Recent includes recently used items and the default tools/projects. Right-click a target to toggle its favorite or pin; saved choices survive restarts. Archived projects are excluded.

| Key | Action |
| --- | --- |
| `0` | Prompt and launch a headless job for the selected target |
| `.` | Open the selected target’s options |
| `/` | Start, pause or continue recording Hotbox actions |
| `*` | Open browser companion capture/import options |
| `−` | Stop recording and prepare a starter |
| `+` | Open a small explanation box during recording; otherwise open a prompt box |
| Enter in a text box | Save the note and close it, or prepare the typed prompt |
| Enter on the wheel | Stop an active/paused recording; otherwise open a prompt box |
| Escape | Close the wheel; in a text box, return to the wheel without saving |

The control reference is [`src/numpad.json`](../src/numpad.json). NumLock-off navigation aliases work when Qt identifies the key as a numpad key.

## Registered tools

**Tools → + New tool** opens the tool draft form; choosing a category first makes
that the suggested placement. Saving creates a stable core tool identity and a
linked private development chat, then opens that chat in the side panel. No agent
runs during creation. The category and friendly name can change while the tool ID,
chat history and Favorites/Pinned references remain fixed.

**Tools → category → tool** opens its options. **Edit** checks the core mapping
and returns to its existing chat; **Name and category** edits metadata. Run is
unavailable until a verified release and runtime adapter exist. Pressing **0** on
a registered draft opens these options rather than bypassing that restriction.
See the [API reference](../core/docs/TOOL_MENU_REFERENCE.md) and
[delivery plan](../core/docs/TOOL_CONTEXT_DELIVERY_PLAN.md).

## Record, explain, prepare

Recording collects explicit Hotbox actions, supplied URLs, and notes. The visible recording indicator stays on the wheel and in the main window. Restart pauses unfinished recordings. Pressing + while paused resumes the recording before opening the explanation box. Notes remain in event order: the previous event appears above the note box, and subsequent events follow the saved note. Cookies, passwords, arbitrary desktop clicks and general keyboard typing are not captured.

For URL and browser click events, use the optional [browser companion](../browser-companion/README.md): explicitly start on a selected tab, stop, export, then press `*` and import. `*` currently opens these options; it does not remotely start the extension. Browser recordings are separate from the active Hotbox recording, and import requires finishing that recording first. Imports are capped at 256 KB / 500 events, accept only recognized actions, discard typed values, and sanitize URLs again. Manual labels and notes may still contain private information.

Stopping a nonempty recording prepares a private `/tmp/hotbox-deck-…` folder with **README.md**, **PROMPT.txt**, and a runner. The prompt asks the selected model to summarize the workflow into a README and reusable starter prompt, including missing inputs and tools. A typed prompt without a recording creates the same starter structure. Preparing files is local and does not call a model. Recording data is retained as reference material alongside the request, rather than executed.

The **On deck** view offers:

- **Terminal**: open the starter directory without running a model.
- **Codex once**: run `codex exec` with workspace-write sandboxing and an ephemeral session.
- **Claude once**: run `claude -p` with default permission handling and an eight-turn limit.

Both receive the prompt over stdin, run in the starter directory, and save output in **run.log** / **result.md**. The terminal shows completion status and stays open until Enter. Provider tools, authentication, Python and a working terminal launcher must already be available. Provider permissions still apply. There is no automatic retry or repeating agent loop. Temporary projects may be removed by system cleanup; copy anything to keep. A temporary directory alone is not a sandbox.

## Saved helper drafts

**Teach a helper** still supports private reviewable helper drafts derived from stopped recordings. Those drafts retain source links and parameters and remain disabled. This is separate from choosing an explicit one-shot model run in On deck. Browser session binding, automatic replay, combined live desktop/browser capture and a self-running agent loop remain unimplemented.

Interaction checks use offscreen Qt windows, temporary databases, and a fake provider. They never inject input into the active desktop. Physical-keyboard focus on Wayland, live browser capture and authenticated model runs require manual verification.

## Headless jobs and the desktop task strip

Select a direction once, press **0**, choose **Codex** or **Claude**, and type the request. **Enter** starts the job without a terminal window. The target name/action/ID stay attached to the run; selecting a target never automatically executes its stored action. Jobs work in a fresh private starter directory under `~/.local/share/hotbox/agent-runs/`, with a README, prompt and output log. They do not automatically edit the target project's existing files or import its full chat history.

The daemon owns up to three processes, each limited to fifteen minutes and an 8 MB output log. Codex runs with workspace-write sandboxing; Claude retains its default permissions and eight-turn limit. Hiding the GUI keeps jobs running. Stopping Hotbox interrupts them and preserves their records/output; restarting marks interrupted jobs **Needs attention**, rather than silently replaying a request. Provider authentication must already work. A private starter directory alone is not a sandbox.

Install the user-owned Omarchy plugin with `bin/install-taskbar`. It adds `tom.hotbox-tasks` to the shell's enabled plugins and backs up the previous configuration, while preserving the existing bar layout and position. The dedicated **top task strip** appears for running or unreviewed jobs and disappears when finished/attention badges have been dismissed. It reads only the local status snapshot; it makes no network requests.

Each task has a colored badge, its target name, a short run ID, and **Working / Finished / Needs attention**. Running jobs pulse and show elapsed time. No percentage is invented. Right-click (or single-click) a badge for **Chat / output**, **Output terminal**, or **Tiny prompter**; double-click opens Chat / output. The terminal follows the same job's log, rather than starting another agent. The tiny prompter starts a new bounded run using the previous output and target as reference context. **Agents** in the main GUI provides output, Stop, follow-up prompts and Dismiss badge.

QML validation and parser tests cover the task strip. The restricted development session cannot load a live Wayland PanelWindow, so live bar placement/clicks still need verification on the desktop. No desktop input is injected by tests.

## Feed identity

A chat opened from the wheel carries its spread, traversed project/tool labels and chosen action into the feed header. A deterministic six-character mark and color identify the same combination on subsequent launches. New messages snapshot that route at creation, including the assistant placeholder before streaming starts. Changing the current spread does not relabel earlier messages. Existing messages without recorded provenance show a neutral workspace label. These labels do not change workspace permissions or merge conversations.

Headless prompts save the same route with their target, and follow-up runs retain it. The agent list and output header show the mark and route. Chat messages use plain text, color rails, sender labels and stored timestamps. Scrolling into history disables automatic following; Jump to latest restores it. A workspace change resets following for the newly selected feed.

## Chevron activity feed

The sliding project-chat preview now displays task-post groups over the scoped
event index, newest first. It infers chat groups from loaded user-message order
and groups operational events by source/session; these are display groups, not
durable task or reply relationships. Headless job events can group by run ID.
Operational segments and event chevrons start collapsed. Opening an event reads
its scoped detail lazily, including source, recorded identity, timestamp and
payload. A small **↓ More activity** control at the bottom loads earlier events,
and **↓ More detail** reads the next payload page. Expansion and loaded payloads
persist across feed refreshes. The [audit](CONTEXT_REQUIREMENTS.md#why-the-recent-preview-does-not-solve-continuity)
and [delivery plan](../core/docs/TOOL_CONTEXT_DELIVERY_PLAN.md) describe the missing
explicit task links, historical author identity and parent-task return path.

Recorded phases include prompts, loaded model context, native tool pre-call arguments and results, agent prompt/context/launch/final output, local task requests, and imported/session/command records. Explicit recorded plan and command kinds are labeled Plan and Bash/command. No internal reasoning or inferred plans are fabricated. External CLI provider-internal hooks remain unconnected; their plain output is preserved as output, rather than classified as proven tool execution. Context snapshots are redacted and exclude API authorization headers. Existing access rules still apply to every detail read.
