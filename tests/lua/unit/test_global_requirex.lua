package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

assert_true(type(requirex) == "function")
rc = requirex("nonexistent_module_xyz")
assert_true(rc == false or rc == nil)
Log("test_global_requirex OK")
