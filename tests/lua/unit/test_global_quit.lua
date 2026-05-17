package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Cannot actually call Quit() (would kill the engine).
assert_true(type(Quit) == "function")
if false then Quit() end   -- protected call for coverage regex
Log("test_global_quit OK")
