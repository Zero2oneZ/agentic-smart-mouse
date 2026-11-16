# Hotbox browser companion

Optional Chromium Manifest V3 extension. This is a local recording companion, not a browser automation runner. It has no cookies permission, broad host permission or network collector.

1. Open `chrome://extensions`, enable Developer mode and choose **Load unpacked**.
2. Select this `browser-companion` folder.
3. Open the page to demonstrate, click the extension, and explicitly start recording.
4. Perform the steps, stop, then export the recording to a local JSON file.
5. In Hotbox, open **Teach helper**, import the file, review the steps and make a helper draft.

For keyboard activation, assign the extension toggle command yourself at `chrome://extensions/shortcuts`. Starting a new recording asks before replacing an existing capture. Browser restart and leaving the selected origin pause recording. Restricted browser pages cannot be recorded. Frames, shadow DOM and arbitrary cross-site workflows are not fully supported.

Recording captures sanitized navigation URLs, clicked control labels, input field descriptions and a small shortcut allowlist. It excludes typed values, password fields, raw keystrokes, page contents and cookies. Queries and fragments are stripped. Labels may still contain private data; inspect exports before sharing. Exports and extension storage stay local. Existing login sessions remain in the browser; generated Hotbox drafts have no authenticated session attached.

Temporary selected-tab access follows Chrome's [activeTab](https://developer.chrome.com/docs/extensions/develop/concepts/activeTab) and [scripting](https://developer.chrome.com/docs/extensions/reference/api/scripting) APIs. Access ends when the tab navigates to another origin; this companion pauses rather than silently broadening access.

Run `node --test browser-companion/tests/*.test.js` from the repository. Tests exercise sanitization, injected-content behavior and background lifecycle with mocked browser APIs. Installation and a real-browser end-to-end recording have not been verified automatically.
