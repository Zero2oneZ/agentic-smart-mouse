# Tool menu API reference

Generated from [tool-menu-api-v1.json](../templates/tool-menu-api-v1.json).

Source SHA-256: `28d1ba19bae385b9be89e2ef815bcc9c5cc77de6e344481dde740e8434993fac`.

The core owns definition identity and metadata. Qt owns the mapped private
development chat and replayable projection. These operations do not execute
tools, create verified releases, copy source or grant host permissions.

| Command | Fields |
| --- | --- |
| `tool_menu_register` | Nested `request`: `schema_version`, `request_id`, `actor`, `tool_id`, `resource`, `name`, `purpose`, `menu_category`, `originating_chat`, `initial` |
| `tool_menu_update` | Nested `request`: `schema_version`, `request_id`, `actor`, `tool_id`, `expected_revision`, `name`, `purpose`, `menu_category` |
| `tool_menu_read` | `actor`, `tool_id` |
| `tool_menu_list` | `actor`, `resource`, `after` |

`initial` is null for an unversioned draft, or an immutable `ToolVersion`.
Registering an existing tool requires its exact version and originating chat.
Mutations require schema version 1 and a unique request ID. Exact replays return
the original receipt without overwriting newer metadata. Changed input with the
same ID conflicts. Updates require `expected_revision`; only display name,
purpose and menu category change. Context and source identity remain fixed.

Reads and mutations are resource-owner-only. Lists use `after` as an exclusive
tool-ID cursor and return at most 100 entries. An empty list ends pagination.
The trusted local host supplies actor identity; this is not authentication.

`tool-menu-catalog` returns this catalog without opening a database.
The action set is Edit plus unavailable Run. No runtime is launched by
registration, menu discovery, navigation, or metadata updates.

Core schema 4 adds menu metadata and idempotent request receipts. Existing
tool/version records and workflow state remain in their original tables.

Implementation: [toolmenu.rs](../src/toolmenu.rs), [lineage.rs](../src/lineage.rs),
[CLI](../src/main.rs). Checks: [library tests](../tests/toolmenu.rs),
[CLI checks](../tests/test_tool_menu_cli.py).
