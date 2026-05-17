package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: fromhex/tohex bidirectional round-trip across magnitudes,
-- case variations, and edge values.
local cases = { 0, 1, 0xF, 0x10, 0xFF, 0x100, 0xABCD, 0xDEADBEEF, 0x12345678 }
for _, n in ipairs(cases) do
    local s = tohex(n)
    local back = fromhex(s)
    assert_equal(back, n, "tohex->fromhex round-trip n=" .. n)
end

-- tohex output should be uppercase
assert_equal(tohex(0xabc), "ABC", "tohex uppercases hex digits")
assert_equal(tohex(0xDEADBEEF), "DEADBEEF", "32-bit tohex")

-- fromhex must be case-insensitive
assert_equal(fromhex("deadbeef"), 0xDEADBEEF, "fromhex lowercase")
assert_equal(fromhex("DeAdBeEf"), 0xDEADBEEF, "fromhex mixed case")
assert_equal(fromhex("DEADBEEF"), 0xDEADBEEF, "fromhex uppercase")

-- Empty/zero edge
assert_equal(tohex(0), "0", "tohex(0) is '0'")
assert_equal(fromhex("0"), 0, "fromhex('0') is 0")

Log("test_stress_hex_roundtrip OK")
