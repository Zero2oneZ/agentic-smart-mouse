![Hotbox](docs/hotbox.png)

# Hotbox

A native Omarchy hotbox with persistent category chats, project briefs, folder links, and live shortcut search.

The intended global gesture is to hold **Tab first, then Q** for 0.5 seconds. The menu expands at the pointer; release either key to retract. Click a choice, right-click for deeper actions, or double-click the center for master chat.

The global shortcut is experimental and currently disabled on the development machine after unexpected input during desktop keyboard simulation. Physical-keyboard activation remains unverified. Use `hotbox open` for chat and its **Preview hotbox** button for the menu. Tab reaches the focused app and may change its focus; the proposed integration consumes Q while Tab is held.

## Build and run

`bin/build`, `bin/test`, then `build/hotbox open`. Install with `bin/install`. `hotbox daemon` runs in the background; `hotbox open` opens the chat; `hotbox menu` previews the hotbox; `hotbox release` dismisses it; `hotbox keybindings` opens shortcut search; `hotbox quit` stops the resident app.

`hotbox check --json` reports database and model configuration status without a display. `hotbox --smoke OUTPUT_DIRECTORY` renders sample chat and menu screenshots offscreen, using an isolated database.

The development machine uses a user-local install: executable in `~/.local/bin/hotbox`, desktop entry in `~/.local/share/applications/`, and experimental integration in `~/.local/share/hotbox/hotbox.lua`. Startup is configured in the user's Hyprland autostart; the integration source line in their bindings is commented out. System package installation was not completed because sudo authentication was required.

`integration/hotbox.lua` contains the proposed global bindings for later repair and manual verification. The Arch package installs that file at `/usr/share/hotbox/hotbox.lua`. Configuration changes require `hyprctl reload` and `hyprctl configerrors`. Do not enable the global integration automatically.

## Verification

Eight Qt Test checks passed during development, covering persistence and archive restoration, hold timing, settings validation, streamed replies returning to their original workspace, failed connections retaining messages, and the QML center interaction. Offscreen screenshots use sample content. The live overlay rendered successfully; global shortcut verification did not pass. Keep desktop keyboard injection out of further automated checks.

## Model connection

Open Model settings. Enter an OpenAI-compatible **chat completions** endpoint, model name, and API-key environment variable (default `HOTBOX_API_KEY`). The default endpoint is local Ollama at `http://127.0.0.1:11434/v1/chat/completions`; choose an installed model. Remote endpoints require HTTPS. Secrets are read from the process environment, never saved to SQLite. Restart the daemon after changing its environment. Sending transmits the current project brief and up to 40 recent messages. Master chat also receives the names and briefs of visible workspaces.

Without a model, messages still save locally and the app explains how to connect replies. Model responses stream and survive restart. Stop preserves partial responses. This version cannot execute model-generated file edits or builds; Generate opens the category chat for discussing a request.

## Memory

SQLite in `~/.local/share/hotbox/hotbox.sqlite3` stores category hierarchy, messages, project folders, user-maintained briefs, and non-secret model settings. The entire chat history is retained. The editable Project brief supplies durable context to each new model request; automatic semantic recall is not implemented. Archives retain chats and folder links. Restore from the sidebar. Creating a category links a folder without creating or modifying it.

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
