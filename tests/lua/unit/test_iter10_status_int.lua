package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 10.3: ePrjProjectStatus now returns string-of-int (1/2/3) so the
-- manual idiom `1*projectGetProperty(ePrjProjectStatus) == ePrjDeveloping`
-- works.  Default project type is "Developing" → 1.
local s = projectGetProperty(ePrjProjectStatus)
local n = 1 * s
assert_true(n == ePrjDeveloping or n == ePrjFinished or n == ePrjMaster,
            "expected 1/2/3 got "..tostring(s))
Log("test_iter10_status_int OK n="..n)
