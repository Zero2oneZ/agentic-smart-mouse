![Hotbox](docs/hotbox.png)

# Hotbox

A native Omarchy hotbox with persistent category chats, project briefs, folder links, and live shortcut search.

The intended global gesture is to hold **Tab first, then Q** for 0.5 seconds. The menu expands at the pointer; release either key to retract. Click a choice, right-click for deeper actions, or double-click the center for master chat.

The global shortcut is experimental and currently disabled on the development machine after unexpected input during desktop keyboard simulation. Physical-keyboard activation remains unverified. Use `hotbox open` for chat and its **Preview hotbox** button for the menu. Tab reaches the focused app and may change its focus; the proposed integration consumes Q while Tab is held.

## Build and run

`bin/build`, `bin/test`, then `build/hotbox open`. Install system-wide with `bin/install`, or use `bin/install-local` for an atomic user-local update with a database backup and resident-app restart. `hotbox daemon` runs in the background; `hotbox open` opens the chat; `hotbox menu` previews the hotbox; `hotbox release` dismisses it; `hotbox keybindings` opens shortcut search; `hotbox quit` stops the resident app.

`hotbox check --json` reports database and model configuration status without a display. `hotbox --smoke OUTPUT_DIRECTORY` renders sample chat and menu screenshots offscreen, using an isolated database.

The development machine uses a user-local install: executable in `~/.local/bin/hotbox`, desktop entry in `~/.local/share/applications/`, and experimental integration in `~/.local/share/hotbox/hotbox.lua`. Startup is configured in the user's Hyprland autostart; the integration source line in their bindings is commented out. System package installation was not completed because sudo authentication was required.

`integration/hotbox.lua` contains the proposed global bindings for later repair and manual verification. The Arch package installs that file at `/usr/share/hotbox/hotbox.lua`. Configuration changes require `hyprctl reload` and `hyprctl configerrors`. Do not enable the global integration automatically.

## Verification

Fourteen Qt Test checks pass. `bin/test-cli` exercises the complete headless session workflow after a build. The Qt Test suite covers persistence and archive restoration, hold timing, settings validation, streamed replies returning to their original workspace, failed connections retaining messages, scoped event retrieval, redaction, focused handoffs, bounded context, isolated Git worktrees, local bare-remote pushes, background scheduling, and context-tool round trips. Offscreen screenshots use sample content. The live overlay rendered successfully; global shortcut verification did not pass. Keep desktop keyboard injection out of further automated checks.

## Model connection

Open Model settings. Enter an OpenAI-compatible **chat completions** endpoint, model name, and API-key environment variable (default `HOTBOX_API_KEY`). The default endpoint is local Ollama at `http://127.0.0.1:11434/v1/chat/completions`; choose an installed model. Remote endpoints require HTTPS. Secrets are read from the process environment, never saved to SQLite. Restart the daemon after changing its environment. Sending transmits a bounded project brief, recent event summaries, and up to 12 recent messages (12,000 characters total). Master chat also receives a compact directory of up to 40 visible workspaces. Each request is capped at 64,000 serialized bytes, with at most four rounds of two context lookups. Older messages remain in the local database and can be read through their event references.

Without a model, messages still save locally and the app explains how to connect replies. Model responses stream and survive restart. Stop preserves partial responses. The model can look up events and create focused child chats through function tools. It cannot edit files, execute commands, or manage Git itself; Generate opens the category chat for discussing a request. Use a model that supports Chat Completions function tools.

## Memory

SQLite in `~/.local/share/hotbox/hotbox.sqlite3` stores category hierarchy, messages, project folders, user-maintained briefs, and non-secret model settings. The entire chat history is retained. The editable Project brief supplies durable context to each new model request; history is retrieved on demand through scoped context tools, rather than a semantic embedding index. Archives retain chats and folder links. Restore from the sidebar. Creating a category links a folder without creating or modifying it.

## Shortcuts

| Keys | Action |
| --- | --- |
| Hold Tab then Q | Hotbox after 500 ms |
| Release either key | Retract without launching |
| Ctrl+K | Search Omarchy bindings |
| Ctrl+N | Add category |
| Ctrl+, | Model settings |
| Enter / Shift+Enter | Send / newline |
| Ctrl+Q | Quit resident app |
| ? outside composer | Help |

`Q` alone is not bound to quit because chats must allow normal typing. Undo and redo use the text controls' standard shortcuts.

## Session timeline and focused chats

![Session timeline](docs/timeline.png)

Open **Session timeline** from the sidebar or the chat header. Master chat sees events across the workspace tree; a focused workspace sees its descendants. Rows contain a stable sequence ID, UTC timestamp, workspace, event kind, and short description. Click a row to read details in bounded pages. Load older events to continue through history. Existing saved chats are indexed during the database migration.

Chats, brief edits, focused handoffs, session attachments, recorded commands, Git snapshots, checkpoints, and failures enter the same private SQLite ledger. The event session ID groups records from a single app/CLI process. Imported log files are snapshots attached at import time; their original internal timestamps remain in the attachment. Attach text or JSONL files up to 256 KiB. Common credentials are redacted, but pattern matching is not a complete secret detector. Do not attach material you do not want the configured model to access.

**Create focused chat** creates a child conversation with a short goal, bounded parent brief, and links to recent parent events. It shares the model connection and initially links the same folder. It does not copy the entire history or launch another autonomous model process. The master can also invoke `focus_workspace`; the new child appears in the sidebar. `workspace_overview` and `read_event` retrieve detail within the originating conversation's scope, even if you switch windows while a reply arrives.

This is a master index of linked projects, not an automatic conversion of unrelated Git repositories into one physical repository. Each linked repository retains its own history.

## Background Git

From a project's timeline, open **Git settings**, then **Create isolated branch and worktree**. Hotbox creates a `hotbox/<uuid>` branch from the linked repository's committed `HEAD`, in the app data directory's `worktrees/` folder. The focused workspace then points to that worktree. Uncommitted changes in the original folder are not copied or altered. This is a local branch/worktree fork, not a hosting-service repository fork.

Enter exact relative file paths, one per line. Checkpointing commits only those files, including explicitly selected new files or deletions. Directories, symlinks, path traversal, common credential paths, raw `.log`/`.jsonl` files, databases, and detected credentials are rejected. Pre-existing staged changes pause the operation. Repository hooks are disabled for managed operations; ordinary commits made outside Hotbox retain their usual behavior. Operations run off the UI thread and are serialized per worktree.

Enable **Checkpoint selected files every minute** to persist that policy. Enable push only after selecting an existing named remote, such as `origin`. Push targets only that managed branch, uses existing Git authentication without prompting, and never force-pushes. Failures are recorded and pause automatic Git until you explicitly resume it. Hotbox does not merge, rebase, clean, reset, delete branches, or prune worktrees in the background. Checkpoints also record Git status, tracked file tree, recent commit timestamps, and the resulting commit ID. Raw sessions and conversation databases stay local.

No remote is configured for the Hotbox development repository yet; external pushing remains disabled. A remote URL and credentials are needed before that can be enabled.

## Session commands

All commands below work without a display and emit JSON. Add `--data-dir PATH` before `--` to use a separate private database. Category IDs are shown in timeline rows and returned by `focus`.

```sh
hotbox events --category 1 --json
hotbox read-event --category 1 --id 42 --offset 0 --json
hotbox focus --category 1 --name "Menu layout" --brief "Fix edge placement" --folder ~/Work/project --json
hotbox attach --category 2 --path ~/session.jsonl --json
hotbox record --category 2 --kind session.note --summary "Decision" --content "Use native Qt" --json
hotbox run --category 2 -- make -j4
hotbox snapshot --category 2 --json
hotbox branch --category 2 --json
hotbox git-config --category 2 --paths src/main.cpp --remote origin --automatic --push --json
hotbox sync --category 2 --json
```

`run` executes only the program and arguments you explicitly supply, in that category's folder. It captures bounded output, working directory, exit status, start/end events, and elapsed time, with a two-minute timeout. It is not a global shell recorder. Existing external sessions can be attached explicitly; Hotbox does not crawl other applications' histories. Background Git runs while the resident app is running. External CLI changes are reflected in the app within about two seconds.
