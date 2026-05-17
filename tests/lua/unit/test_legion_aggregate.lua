package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.3 — cross-voice aggregation with dual-tier similarity gate.

local function bs(...)
    local out = ""
    for _, v in ipairs({...}) do out = out .. string.char(v) end
    return out
end

local function rep(n, ch) return string.rep(string.char(ch), n) end

-- Build a voice with a single region.
local function voice(name, startAddr, originalBytes, modifiedBytes)
    return {
        sourcePath = name,
        regions = {{
            startAddr     = startAddr,
            originalBytes = originalBytes,
            modifiedBytes = modifiedBytes,
        }}
    }
end

-- 1) Empty cluster → empty verdicts.
do
    local r = _legion_aggregate({}, "", {}, 0.90)
    assert_equal(#r, 0)
end

-- 2) Single voice, single byte changed → one verdict, count=1, mean=delta.
do
    local baseline = rep(32, 0x10)
    -- Voice: at addr 10, original 0x10 (matches baseline), modified 0x20.
    local v1 = voice("a.ols", 10, bs(0x10), bs(0x20))
    local r  = _legion_aggregate({v1}, baseline, {1}, 0.90)
    assert_equal(#r, 1)
    assert_equal(r[1].startAddr, 10)
    assert_equal(r[1].endAddr,   10)
    assert_equal(r[1].cellSize,  1)
    assert_equal(r[1].maxSampleCount, 1)
    assert_equal(#r[1].cells, 1)
    assert_equal(r[1].cells[1].sampleCount, 1)
    assert_true(math.abs(r[1].cells[1].meanDelta - 16) < 1e-6,
                "mean delta should be +16")
    assert_true(r[1].cells[1].stdDevDelta < 1e-6, "std must be 0 with 1 sample")
end

-- 3) Two voices with identical change → mean=delta, std=0, count=2.
do
    local baseline = rep(32, 0x10)
    local v1 = voice("a.ols", 10, bs(0x10), bs(0x20))
    local v2 = voice("b.ols", 10, bs(0x10), bs(0x20))
    local r  = _legion_aggregate({v1, v2}, baseline, {1, 2}, 0.90)
    assert_equal(#r, 1)
    assert_equal(r[1].cells[1].sampleCount, 2)
    assert_true(math.abs(r[1].cells[1].meanDelta - 16) < 1e-6)
    assert_true(r[1].cells[1].stdDevDelta < 1e-6,
                "two identical voices → stdDev 0")
    assert_equal(r[1].maxSampleCount, 2)
    assert_equal(#r[1].contributingVoices, 2)
end

-- 4) Two voices with conflicting deltas → mean = avg, std > 0.
do
    local baseline = rep(32, 0x10)
    local v1 = voice("a.ols", 10, bs(0x10), bs(0x20))   -- +16
    local v2 = voice("b.ols", 10, bs(0x10), bs(0x00))   -- -16
    local r  = _legion_aggregate({v1, v2}, baseline, {1, 2}, 0.90)
    assert_equal(#r, 1)
    assert_equal(r[1].cells[1].sampleCount, 2)
    assert_true(math.abs(r[1].cells[1].meanDelta) < 1e-6,
                "opposing deltas should cancel in mean")
    assert_true(r[1].cells[1].stdDevDelta > 10,
                "opposing deltas → large stdDev")
end

-- 5) Local hamming gate — voice whose original diverges from baseline is rejected.
do
    -- baseline at addrs 10..14 = 0x10 each.  Voice has 5-byte region; voice's
    -- original is 0xFF for 4 bytes and 0x10 for 1 byte → 1/5 = 20% match,
    -- well below 0.90 gate.
    local baseline = rep(32, 0x10)
    local v1 = voice("a.ols", 10, bs(0xFF, 0xFF, 0xFF, 0xFF, 0x10), bs(0xFE, 0xFE, 0xFE, 0xFE, 0x11))
    local r  = _legion_aggregate({v1}, baseline, {1}, 0.90)
    -- Voice rejected → no verdicts.
    assert_equal(#r, 0, "voice failing local gate must produce no verdict")
end

-- 6) Lower the gate to 0.10 — voice now contributes.
do
    local baseline = rep(32, 0x10)
    local v1 = voice("a.ols", 10, bs(0xFF, 0xFF, 0xFF, 0xFF, 0x10), bs(0xFE, 0xFE, 0xFE, 0xFE, 0x11))
    local r  = _legion_aggregate({v1}, baseline, {1}, 0.10)
    assert_equal(#r, 1)
    assert_equal(r[1].maxSampleCount, 1)
end

-- 7) Two far-apart changes (gap > K=16) → two distinct verdicts.
do
    local baseline = rep(128, 0x10)
    local v1 = {
        sourcePath = "a.ols",
        regions = {
            { startAddr = 10, originalBytes = bs(0x10), modifiedBytes = bs(0x20) },
            { startAddr = 80, originalBytes = bs(0x10), modifiedBytes = bs(0x30) },
        }
    }
    local r = _legion_aggregate({v1}, baseline, {1}, 0.90)
    assert_equal(#r, 2)
    assert_equal(r[1].startAddr, 10)
    assert_equal(r[2].startAddr, 80)
end

-- 8) Range out-of-bounds vs baseline → skipped.
do
    local baseline = rep(8, 0x10)
    local v1 = voice("a.ols", 100, bs(0x10), bs(0x20))   -- beyond baseline
    local r  = _legion_aggregate({v1}, baseline, {1}, 0.90)
    assert_equal(#r, 0, "out-of-range addresses must not produce a verdict")
end

-- 9) Multiple cells in one verdict (curve, 8 contiguous bytes).
do
    local baseline = rep(32, 0x00)
    -- 8 bytes at addr 5: voice changes each byte from 0x00 to (10 + i)
    local origStr = rep(8, 0x00)
    local modStr  = bs(10, 11, 12, 13, 14, 15, 16, 17)
    local v1 = voice("a.ols", 5, origStr, modStr)
    local r  = _legion_aggregate({v1}, baseline, {1}, 0.90)
    assert_equal(#r, 1)
    assert_equal(r[1].startAddr, 5)
    assert_equal(r[1].endAddr,   12)
    -- Cell decoding depends on inferred cell-size — verdict spans the right
    -- 8 bytes either way.
    local total = 0
    for _, c in ipairs(r[1].cells) do total = total + c.sampleCount end
    assert_true(total >= 1, "at least one cell sampled")
end

Log("test_legion_aggregate OK")
