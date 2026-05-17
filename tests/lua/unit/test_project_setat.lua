package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

orig = projectGetAt(0x100, eByte)
projectSetAt(0x100, 0xAB, eByte)
assert_equal(projectGetAt(0x100, eByte), 0xAB)
-- restore
projectSetAt(0x100, orig, eByte)
assert_equal(projectGetAt(0x100, eByte), orig)
-- string form
projectSetAt(0x200, "DE AD BE EF", eByte)
assert_equal(projectGetAt(0x200, eByte), 0xDE)
assert_equal(projectGetAt(0x203, eByte), 0xEF)
Log("test_project_setat OK")
