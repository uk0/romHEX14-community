package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Non-Sql variant: should alias to the Sql one (both return tables)
arr = projectFindSimilarProjects(80, 5, ePrjFilename)
assert_true(type(arr) == "table")
Log("test_project_findsimilarprojects OK")
