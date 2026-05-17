package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectSetQuickFixState("DoesNotExist", true)
assert_equal(rc, -2)
Log("test_project_setquickfixstate OK")
