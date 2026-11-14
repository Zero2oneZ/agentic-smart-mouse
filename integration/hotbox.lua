-- Hotbox: Tab arms Q; both must remain held for 500 ms in the app.
-- Tab remains non-consuming. Q is intercepted only while Tab is held.
local hotbox_tab = false
local hotbox_active = false
local hotbox_q
local function release_hotbox()
  if hotbox_active then
    hotbox_active = false
    hl.exec_cmd("hotbox release")
  end
end
hotbox_q = hl.bind("Q", function()
  if hotbox_tab and not hotbox_active then
    hotbox_active = true
    hl.exec_cmd("hotbox hold")
  end
end, { description = "Hotbox (hold Tab + Q for 0.5 seconds)" })
hotbox_q:set_enabled(false)
hl.bind("TAB", function()
  hotbox_tab = true
  hotbox_q:set_enabled(true)
end, { non_consuming = true })
hl.bind("TAB", function()
  hotbox_tab = false
  hotbox_q:set_enabled(false)
  release_hotbox()
end, { release = true, transparent = true, ignore_mods = true, non_consuming = true })
hl.bind("Q", release_hotbox, { release = true, transparent = true, ignore_mods = true, non_consuming = true })
