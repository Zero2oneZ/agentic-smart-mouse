# Keyboard controls and recorded helpers

Open **Keyboard controls** in the sidebar, run `hotbox keypad`, or press **Super + numpad +** with the installed integration. Controls apply while this panel has focus; ordinary typing elsewhere remains available. NumLock-on and NumLock-off navigation aliases are supported. Tab and Shift+Tab navigate the forms.

| Numpad key | Action |
| --- | --- |
| 4 / 5 | Previous / next quick task |
| Enter | Open selected task |
| 8 / 2 | Select previous / next job |
| 6 | Activity |
| + | New local notes task |
| * | Pause / continue selected job |
| 7 | Start / pause / continue recording |
| 9 | Finish recording and review |
| 1 | Add a recording note |
| 3 | Open Teach helper |
| 0, minus, Escape | Close controls |

The executable control schema is [`src/numpad.json`](../src/numpad.json), bundled in the app. Research, News and Theme open focused guided chats; they do not fetch live news, browse, or apply themes automatically. Notes uses the existing deterministic private brief recipe. No model request is sent merely by selecting a quick task.

## Teach helper

Start a recording explicitly. Add notes and URLs, or perform supported Hotbox actions. A visible recording indicator remains available. Pause excludes subsequent actions; Finish closes the recording for review. Restart pauses unfinished recordings. Private recordings and drafts persist in the local Hotbox SQLite database, outside the repository.

Use **Import browser recording** for an export from the optional browser companion. Imports accept only bounded, recognized navigation, click, field and shortcut observations. Unsupported action kinds reject the import. Typed field values, cookies and extra JSON properties are not imported. URL queries, fragments and sensitive-looking paths are removed. Manual notes and labels can still contain private information; review them before sharing.

Choose **Make helper draft** to derive a named linear procedure with parameters for demonstrated input fields. Drafts retain source links, remain disabled and private, and require review of tools, permissions, session access and success checks. Recorded actions describe intent, not proof of successful execution. No replay engine, global publication, schedule, browser session binding or autonomous agent launch is implemented. Login cookies stay in the browser; a future runner must request an appropriate browser session rather than copy credentials.

Browser imports are capped at 256 KB and 500 steps. The companion additionally bounds exports. Live desktop keyboard activation and real-browser capture require manual verification; offscreen Qt, Lua callback and mocked browser tests cover the implementation without injecting desktop input.
