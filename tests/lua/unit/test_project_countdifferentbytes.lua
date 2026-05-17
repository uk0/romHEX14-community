package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Default returns IDENTICAL bytes count (misleading WinOLS default!)
same = projectCountDifferentBytes()
diff = projectCountDifferentBytes(TRUE)
assert_true(type(same) == "number")
assert_true(type(diff) == "number")
assert_true(diff >= 0)
Log("test_project_countdifferentbytes OK")
