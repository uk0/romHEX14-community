package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Without -1 startAddress, returns Number (-1 if not found in STUB)
rc = projectFindBytes(0, 1, 0x00)
assert_true(type(rc) == "number")
-- With -1 startAddress, returns table
arr = projectFindBytes(-1, 1, 0x00)
assert_true(type(arr) == "table")
Log("test_project_findbytes OK")
