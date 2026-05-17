package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- .winolsskript must STUB to false per §5.7
rc = projectImport("any.winolsskript")
assert_equal(rc, false)
Log("test_project_import OK")
