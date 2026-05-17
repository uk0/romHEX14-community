package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

arr = projectFindSimilarProjectsSql(80, 5, ePrjFilename)
assert_true(type(arr) == "table")
-- Negative MinPercent enables data-area sim (WinOLS 5.11+) — must not error
arr2 = projectFindSimilarProjectsSql(-80, 5, ePrjFilename)
assert_true(type(arr2) == "table")
Log("test_project_findsimilarprojectssql OK")
