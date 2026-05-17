package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

rc = projectAddCommentAt(0x400, 0x410, "lua test comment")
assert_equal(rc, true)
Log("test_project_addcommentat OK")
