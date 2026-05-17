package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectSetRightsOwner(33333)
assert_equal(rc, false)
Log("test_project_setrightsowner OK")
