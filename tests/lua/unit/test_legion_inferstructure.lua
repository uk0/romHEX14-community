package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.2 — verify legion::inferStructure picks reasonable cell-size,
-- endianness, dimensions, and kind from a region's original bytes.

local function bytes(...)
    local out = ""
    for _, v in ipairs({...}) do out = out .. string.char(v) end
    return out
end

local function rep(n, ch) return string.rep(string.char(ch), n) end

-- 1) Single byte → Scalar, cellSize=1, 1×1.
do
    local h = _legion_inferStructure(bytes(0xAB))
    assert_equal(h.kind, "Scalar")
    assert_equal(h.cellSize, 1)
    assert_equal(h.rows, 1)
    assert_equal(h.cols, 1)
end

-- 2) Two bytes → Scalar, cellSize=2 (16-bit limiter default), 1×1.
do
    local h = _legion_inferStructure(bytes(0xE8, 0x03))     -- 1000 LoHi
    assert_equal(h.kind, "Scalar")
    assert_equal(h.cellSize, 2)
    assert_equal(h.bigEndian, false)
    assert_equal(h.rows * h.cols, 1)
end

-- 3) Four bytes → Scalar, cellSize=4 (32-bit limiter), 1×1.
do
    local h = _legion_inferStructure(bytes(0x40, 0x42, 0x0F, 0x00))   -- 1_000_000 LoHi
    assert_equal(h.kind, "Scalar")
    assert_equal(h.cellSize, 4)
    assert_equal(h.rows * h.cols, 1)
end

-- 4) Smooth monotonic byte curve, 10 cells → Curve, cellSize=1, 1×10.
do
    local s = ""
    for i = 0, 9 do s = s .. string.char(i * 10) end
    local h = _legion_inferStructure(s)
    assert_equal(h.kind, "Curve")
    assert_equal(h.cellSize, 1, "monotonic byte sequence → cellSize 1")
    assert_equal(h.rows, 1)
    assert_equal(h.cols, 10)
end

-- 5) Smooth 16-bit LoHi curve, 8 cells (16 bytes).  Encoding values 1000..1700
--    as LoHi makes bytes [0xE8,0x03, 0xC2,0x03, ...].  Decoded as bytes the
--    sequence is jumpy; as LoHi it's smooth → algorithm should pick cellSize=2.
do
    local s = ""
    for i = 0, 7 do
        local v = 1000 + i * 100
        s = s .. string.char(v % 256) .. string.char(v // 256)
    end
    local h = _legion_inferStructure(s)
    assert_equal(h.cellSize, 2, "smooth 16-bit curve → cellSize 2")
    assert_equal(h.bigEndian, false, "values stored LoHi → LSB-first")
    assert_equal(h.kind, "Curve")
    assert_equal(h.cols, 8)
end

-- 6) Smooth 16-bit HiLo curve, 8 cells.
do
    local s = ""
    for i = 0, 7 do
        local v = 5000 + i * 200
        s = s .. string.char(v // 256) .. string.char(v % 256)
    end
    local h = _legion_inferStructure(s)
    assert_equal(h.cellSize, 2)
    assert_equal(h.bigEndian, true, "smooth HiLo encoding detected")
end

-- 7) 16×16 byte map with smooth surface (256 bytes) → SmallMap, 16×16-ish.
--    Surface: cells[r][c] = r*8 + c   (smooth in both directions).
do
    local s = ""
    for r = 0, 15 do
        for c = 0, 15 do
            s = s .. string.char((r * 8 + c) % 256)
        end
    end
    local h = _legion_inferStructure(s)
    assert_equal(h.cellSize, 1, "byte map kept as byte cells")
    assert_equal(h.kind, "SmallMap")
    assert_true(h.rows * h.cols == 256, "dimensions multiply to cell count")
    assert_true(h.rows >= 2 and h.cols >= 2, "dimensions are 2D")
end

-- 8) Large 32×32 byte region (1024B) → LargeMap.
do
    local s = ""
    for i = 0, 1023 do s = s .. string.char(i % 256) end
    local h = _legion_inferStructure(s)
    assert_equal(h.kind, "LargeMap")
    assert_true(h.rows * h.cols == 1024)
end

-- 9) Pure-zero region — degenerate, all interpretations roughness=inf.
--    Must still return *something* valid (default to byte / 1×N).
do
    local h = _legion_inferStructure(rep(64, 0x00))
    assert_true(h.cellSize >= 1 and h.cellSize <= 8)
    assert_true(h.rows * h.cols == 64 / h.cellSize)
    assert_true(h.kind == "SmallMap" or h.kind == "Curve")
end

Log("test_legion_inferstructure OK")
