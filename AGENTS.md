# Working on Hotbox

Build with `bin/build`. Run backend and QML tests offscreen with `bin/test`.
Tests must use temporary databases and local mock model endpoints.

Do not inject keyboard or mouse input into the user's active desktop. Use
offscreen Qt tests for interaction checks. The global activation gesture is Ctrl + Super held for 300 ms. Do not change
the user's desktop configuration without an explicit request. Validate bindings
without injecting input into the active desktop.

Keep conversation databases, credentials, runtime logs, generated builds, and
package archives out of Git. Screenshots committed under `docs/` must contain
only sample application content, never captures of the user's desktop.

Update the README when implementation or verification status changes. Model
chat supports scoped event lookup and focused handoffs, but cannot edit files,
execute commands, or push Git. Git worker tests must use temporary repositories
and local bare remotes. Background checkpoints apply only to managed worktrees
and explicitly selected files; keep session data outside every pushed branch.

Reusable segments retain original redacted text artifacts separately from derived
results and summaries. Never execute stored scripts automatically. Queue workers
must use valid, unexpired leases to finish or fail jobs. Preserve workspace
scoping and explicit segment grants when retrieving or composing context.
