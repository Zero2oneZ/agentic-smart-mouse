# Local notes tasks: first connected desktop workflow

## What ships

Hotbox 0.5.0 connects its Qt task screen to the Rust core through a bounded,
fixed-operation JSON CLI bridge. **Organize notes** accepts pasted text, trims
blank/bullet prefixes, removes exact repeated notes while preserving order, checks
coverage and its output digest, then creates a Markdown brief in private task storage.
No model, network, external program execution, VM or provider key is needed.

Open Hotbox, choose **Try sample notes** or **New task**, give the notes a title and
choose **Start task**. Activity shows Prepare, Organize, Check and Save. Pause,
Continue and Stop act between short atomic steps. Completed work offers a plain-text
preview, Open brief and Show in folder. IDs, project references, paths and detailed
failure messages live under Details. Ctrl+N starts a new task; projects/chats remain
available. Older storage/history tools are under More options.

The resident desktop app schedules one bounded step at a time, using short-lived
headless CLI processes. Hiding/closing its window leaves the resident app running.
Quitting the app stops advancement; reopening reloads queued/working tasks and
continues at the saved checkpoint. Paused tasks remain paused. This is not yet a
worker daemon independent of the Qt process, a general shell/PTY runner or a swarm
runtime. Existing model tools remain read-only.

## State, identity and files

- Qt chat/category storage stays in `hotbox.sqlite3`.
- Rust tasks use `orchestration.sqlite3` beside it, kernel schema 3.
- Each desktop task gets a random `wrun_<UUID hex>` ID and a friendly editable-input
  title. Its immutable request includes the source `workspace-<Qt ID>` reference.
- Result path: `task-results/<run-id>/notes.md`, relative to that private data folder.
  Generated directories use mode 0700 and newly published files mode 0600 on Unix.
- Input is private local text, not automatically redacted, encrypted or sent to a
  provider. Task history does not enter the global catalog or source Git.
- No original user file is read/moved/overwritten. Output publication is create-only,
  rejects symlinks and conflicting contents and refuses actual source-repository
  ancestors. Checks require a trusted private parent; they are not race-proof OS
  isolation against another writer using the same account.

Requests are limited to a 100-character title and 1–500 unique nonempty notes, at
most 64 KiB of UTF-8 input. The bridge caps output at 512,000 bytes and each helper
invocation at eight seconds. Recent Activity lists at most 50 jobs, prioritizing
active work; bodies are fetched for a selected job. Queue advancement independently
selects the oldest active task across the local actor's projects.

Each step/checkpoint and accepted event is committed together under an immediate
SQLite transaction. Retrying an identical start ID returns its existing task;
changing its request conflicts. Result staging is synced and atomically published
without replacement. A crash after file publication but before SQL commit resumes
by accepting an identical existing result; a changed result fails and remains
untouched. Failed/cancelled/completed jobs do not restart implicitly. Start a new
task to retry a terminal result. Pause/resume retains the same immutable input.

## CLI operations

After building, `hotbox-core DATABASE` reads one bounded JSON request on stdin:

```json
{"command":"notes_start","request":{"id":"wrun-example","actor":"local-user","resource":"workspace-1","title":"Weekend project","notes":"Measure the wall.\nReuse the lamp.\nMeasure the wall."}}
```

```json
{"command":"notes_list","actor":"local-user","resource":""}
```

An empty resource returns this local actor's recent jobs; a named resource filters
it. `notes_read` and `notes_advance` accept `actor` and `id`. `notes_tick` accepts
`actor` and advances one step of the oldest queued/working task, or returns null.
`notes_control` accepts `actor`, `id`, and `action` (pause/resume/cancel).
Responses use the existing `ok/result` or `ok/error` envelope. Task views contain
id/title/resource/status, step/total_steps, timestamps, note count, result path,
preview and failure detail. Lists omit preview bodies.

The fixed recipe is authorized by immutable local resource ownership; it does not
introduce generic execution rights into the eight fixture-build grant actions.
The host supplies the local actor. Caller strings remain trusted-host arguments,
not authentication; this is not a shared remote-user API. Other actors cannot read
or control the owner's tasks through these operations. No model tool can invoke them.

## Packaging and recovery

`bin/build` builds Rust release output plus the Qt app and places both executables
in `build/`. Cargo with the locked dependencies and Qt build tools are needed for
source builds; fetch dependencies beforehand if building offline. Installed packages
ship both executables, so normal users need no compiler. The task helper is resolved
beside the app binary. `HOTBOX_CORE_BINARY` is an optional trusted developer/test
absolute executable override; symlinked helpers are rejected.

`bin/install-local` backs up both SQLite stores and existing executables, installs
the app/helper, and restarts the resident app. It does not change keyboard bindings.
Schema 1/2 upgrade additively to 3. Older core binaries reject schema 3: restore a
consistent backup with the matching binary to downgrade; never lower user_version
by hand. Back up task-results and private registries as well as chat history.

## Verification and limits

Ten Rust task tests cover deterministic output and deduplication, restart, controls,
owner boundaries, idempotent starts, existing-file preservation, publication recovery,
project-independent queues, bounded input, schema-2 upgrade, symlink rejection
and refusal to publish when a recipe revision or checked digest changes.
Two Qt integration tests use the real helper and temporary state: pause/restart/
continue without a window, and form-started work finishing while its window is
hidden. The CLI fixture covers the complete recipe across fresh helper processes.

Generic agents, model-based summarization, imported files, arbitrary executables,
terminal sessions, sandbox enforcement, scheduled tasks, remote workers and output
sharing are not enabled by this recipe. General supervision and smart multi-source
segmentation remain follow-up product slices.
