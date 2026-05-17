package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Returns -2 ("unknown") for any name when not implemented
rc = projectGetQuickFixState("DoesNotExist")
assert_equal(rc, -2)
Log("test_project_getquickfixstate OK")
