package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.7 — verify legion::applyVerdict writes per-cell mean deltas back
-- into a byte buffer with correct cellSize / endianness / clamping.

local function rep(n, ch) return string.rep(string.char(ch), n) end
local function bs(...)
    local out = ""
    for _, v in ipairs({...}) do out = out .. string.char(v) end
    return out
end
local function byteAt(s, i) return string.byte(s, i + 1) end   -- 0-based

-- 1) Single 1-byte cell with positive mean delta → byte value bumped.
do
    local data = rep(16, 0x10)
    local v = { startAddr = 5, cellSize = 1, bigEndian = false,
                cells = {{meanDelta = 7, sampleCount = 3}} }
    local out = _legion_applyVerdict(data, v)
    assert_equal(byteAt(out, 5), 0x17, "5th byte should become 0x10 + 7")
    assert_equal(byteAt(out, 4), 0x10, "untouched byte unchanged")
    assert_equal(#out, #data, "buffer size preserved")
end

-- 2) Cells with sampleCount=0 skipped.
do
    local data = rep(8, 0x20)
    local v = { startAddr = 0, cellSize = 1, bigEndian = false,
                cells = {
                    {meanDelta = 5,  sampleCount = 0},   -- skip
                    {meanDelta = 5,  sampleCount = 4},   -- apply
                }}
    local out = _legion_applyVerdict(data, v)
    assert_equal(byteAt(out, 0), 0x20, "cell with no samples untouched")
    assert_equal(byteAt(out, 1), 0x25, "sampled cell gets +5")
end

-- 3) Underflow → clamp to 0.
do
    local data = bs(0x02, 0x00)
    local v = { startAddr = 0, cellSize = 1, bigEndian = false,
                cells = {{meanDelta = -100, sampleCount = 1}} }
    local out = _legion_applyVerdict(data, v)
    assert_equal(byteAt(out, 0), 0x00, "should clamp at 0")
end

-- 4) Overflow → clamp to 0xFF.
do
    local data = bs(0xF0)
    local v = { startAddr = 0, cellSize = 1, bigEndian = false,
                cells = {{meanDelta = 100, sampleCount = 1}} }
    local out = _legion_applyVerdict(data, v)
    assert_equal(byteAt(out, 0), 0xFF, "should clamp at 0xFF")
end

-- 5) 16-bit LoHi cell — full encoded value bumped by mean delta.
do
    -- byte at offset 0..1 = 0x03 0xE8 (LoHi → 1000)
    local data = bs(0xE8, 0x03, 0x00, 0x00)
    local v = { startAddr = 0, cellSize = 2, bigEndian = false,
                cells = {{meanDelta = 500, sampleCount = 5}} }
    local out = _legion_applyVerdict(data, v)
    -- Expected: 1500 → LoHi bytes: 0xDC, 0x05
    assert_equal(byteAt(out, 0), 0xDC, "low byte of 1500")
    assert_equal(byteAt(out, 1), 0x05, "high byte of 1500")
end

-- 6) 16-bit HiLo cell — byte-order respected.
do
    -- byte at offset 0..1 = 0x13 0x88 (HiLo → 5000)
    local data = bs(0x13, 0x88, 0x00, 0x00)
    local v = { startAddr = 0, cellSize = 2, bigEndian = true,
                cells = {{meanDelta = 200, sampleCount = 5}} }
    local out = _legion_applyVerdict(data, v)
    -- Expected: 5200 → HiLo bytes: 0x14, 0x50
    assert_equal(byteAt(out, 0), 0x14, "high byte of 5200")
    assert_equal(byteAt(out, 1), 0x50, "low byte of 5200")
end

-- 7) Mean delta < 1 rounds to 0 → no change.
do
    local data = rep(4, 0x10)
    local v = { startAddr = 0, cellSize = 1, bigEndian = false,
                cells = {{meanDelta = 0.3, sampleCount = 5}} }
    local out = _legion_applyVerdict(data, v)
    assert_equal(byteAt(out, 0), 0x10, "rounded-to-zero delta is a no-op")
end

-- 8) Mean delta 0.5 rounds away from zero (positive) → +1.
do
    local data = rep(4, 0x10)
    local v = { startAddr = 0, cellSize = 1, bigEndian = false,
                cells = {{meanDelta = 0.6, sampleCount = 5}} }
    local out = _legion_applyVerdict(data, v)
    assert_equal(byteAt(out, 0), 0x11, "0.6 rounds to +1")
end

-- 9) Multi-cell curve — every cell applied in order.
do
    local data = rep(8, 0x80)
    local v = { startAddr = 0, cellSize = 1, bigEndian = false,
                cells = {
                    {meanDelta = 5, sampleCount = 4},
                    {meanDelta = 5, sampleCount = 4},
                    {meanDelta = -5, sampleCount = 4},
                    {meanDelta = -5, sampleCount = 4},
                }}
    local out = _legion_applyVerdict(data, v)
    assert_equal(byteAt(out, 0), 0x85)
    assert_equal(byteAt(out, 1), 0x85)
    assert_equal(byteAt(out, 2), 0x7B)
    assert_equal(byteAt(out, 3), 0x7B)
    assert_equal(byteAt(out, 4), 0x80, "beyond verdict range — untouched")
end

-- 10) Out-of-bounds verdict — silently skipped, returns same data.
do
    local data = rep(4, 0x10)
    local v = { startAddr = 100, cellSize = 1, bigEndian = false,
                cells = {{meanDelta = 5, sampleCount = 4}} }
    local out = _legion_applyVerdict(data, v)
    for i = 0, 3 do assert_equal(byteAt(out, i), 0x10) end
end

Log("test_legion_applyverdict OK")
