# Working on Hotbox

Build with `bin/build`. Run backend and QML tests offscreen with `bin/test`.
Tests must use temporary databases and local mock model endpoints.

Do not inject keyboard or mouse input into the user's active desktop. Use
offscreen Qt tests for interaction checks. The experimental Tab + Q global
shortcut is disabled on the development machine; do not re-enable it or change
the user's desktop configuration without an explicit request.

Keep conversation databases, credentials, runtime logs, generated builds, and
package archives out of Git. Screenshots committed under `docs/` must contain
only sample application content, never captures of the user's desktop.

Update the README when implementation or verification status changes. Model
chat currently supports replies and project context, but cannot edit files or
execute builds.
