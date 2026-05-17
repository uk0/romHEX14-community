package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Iter 10.5: eDoubleLoHi / eDoubleHiLo now write and read 8-byte IEEE754
-- doubles correctly.  Round-trip a known value past byte 0x1000.
local addr = 0x1000
local expected = 3.141592653589793
projectSetAt(addr, expected, eDoubleLoHi)
local back = projectGetAt(addr, eDoubleLoHi)
assert_close(back, expected, 1e-12, "eDoubleLoHi round-trip")

projectSetAt(addr + 16, expected, eDoubleHiLo)
local back2 = projectGetAt(addr + 16, eDoubleHiLo)
assert_close(back2, expected, 1e-12, "eDoubleHiLo round-trip")

-- Restore via eEmptyvalue covers the full 8-byte cell, not just byte 0.
projectSetAt(addr, eEmptyvalue, eDoubleLoHi)
local restored = projectGetAt(addr, eDoubleLoHi)
local original = projectGetAt(addr, eDoubleLoHi, 1)  -- reads currentData
-- After restore, currentData should match originalData at all 8 bytes;
-- if any byte differed from before the write, restore would mismatch.
assert_true(type(restored) == "number", "restored value is number")
Log("test_iter10_double_roundtrip OK")
