-- Hotbox: hold Ctrl + Super in either order for 300 ms in the app.
-- Observe modifier keys without consuming them or binding ordinary letters.
local hotbox_active = false
local function hold_hotbox()
  if not hotbox_active then
    hotbox_active = true
    hl.exec_cmd("hotbox hold")
  end
end
local function release_hotbox()
  if hotbox_active then
    hotbox_active = false
    hl.exec_cmd("hotbox release")
  end
end
for _, key in ipairs({ "Super_L", "Super_R" }) do
  hl.bind("CTRL + " .. key, hold_hotbox, {
    non_consuming = true,
    description = "Hotbox (hold Ctrl + Super for 0.3 seconds)",
  })
end
for _, key in ipairs({ "Control_L", "Control_R" }) do
  hl.bind("SUPER + " .. key, hold_hotbox, {
    non_consuming = true,
    description = "Hotbox (hold Ctrl + Super for 0.3 seconds)",
  })
end
for _, key in ipairs({ "Control_L", "Control_R", "Super_L", "Super_R" }) do
  hl.bind(key, release_hotbox, {
    release = true, transparent = true, ignore_mods = true, non_consuming = true,
  })
end

-- A focused keypad panel; ordinary numpad keys are never bound globally.
hl.bind("SUPER + KP_Add", function()
  hl.exec_cmd("hotbox keypad")
end, { description = "Hotbox keyboard controls" })
