package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: toJSON -> fromJSON full round-trip preserving array, nested object,
-- strings, numbers, and booleans.
local original = {
    name = "stress",
    count = 42,
    active = true,
    inactive = false,
    nums = { 1, 2, 3, 4 },
    nested = { x = 10, y = "deep" }
}
local s = toJSON(original)
assert_true(type(s) == "string", "toJSON returns string")
assert_true(#s > 10, "toJSON not trivially empty")

local back = fromJSON(s)
assert_true(type(back) == "table", "fromJSON parses to table")
assert_equal(back.name, "stress", "string round-trip")
assert_equal(back.count, 42, "number round-trip")
assert_equal(back.active, true, "bool true round-trip")
assert_equal(back.inactive, false, "bool false round-trip")
assert_equal(#back.nums, 4, "array length")
assert_equal(back.nums[1], 1, "array[1]")
assert_equal(back.nums[4], 4, "array[4]")
assert_equal(back.nested.x, 10, "nested.x")
assert_equal(back.nested.y, "deep", "nested.y")

-- JSON array round-trip
local arr_s = toJSON({ 10, 20, 30 })
local arr_back = fromJSON(arr_s)
assert_equal(#arr_back, 3, "array round-trip length")
assert_equal(arr_back[2], 20, "array round-trip element")

Log("test_stress_json_roundtrip OK")
