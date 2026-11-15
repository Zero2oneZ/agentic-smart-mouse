-- No desktop input: inspect callbacks with a stub compositor API.
local bindings, commands = {}, {}
hl = {
  bind = function(key, callback, options) bindings[key] = {callback=callback, options=options} end,
  exec_cmd = function(command) table.insert(commands,command) end,
}
dofile(arg[1] or "integration/hotbox.lua")
assert(bindings["SUPER + KP_Add"], "Missing explicit keypad entry point")
bindings["SUPER + KP_Add"].callback()
assert(commands[#commands] == "hotbox keypad")
for _, key in ipairs({"4","5","KP_4","KP_5","KP_Left","KP_Begin"}) do
  assert(not bindings[key], "Ordinary numpad key captured globally: " .. key)
end
assert(bindings["CTRL + Super_L"].options.non_consuming)
assert(bindings["SUPER + Control_R"].options.non_consuming)
print("Keypad entry and unchanged non-consuming hold bindings passed.")
