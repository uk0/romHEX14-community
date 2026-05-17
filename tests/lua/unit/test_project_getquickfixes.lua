package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

arr = projectGetQuickFixes()
assert_true(type(arr) == "table")
Log("test_project_getquickfixes OK")
