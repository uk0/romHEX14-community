package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

projectAddCommentAt(0x600, 0x600, "del-test")
rc = projectDelCommentAt(0x600)
assert_equal(rc, true)
s = projectGetCommentAt(0x600)
assert_equal(s, "")
Log("test_project_delcommentat OK")
