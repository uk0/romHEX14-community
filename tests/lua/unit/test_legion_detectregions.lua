package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.1 — verify legion::detectRegions covers the documented edge cases.
-- _legion_detectRegions(originalBytes, stage1Bytes, kAdjacency) returns
-- {{startAddr, endAddr, size}, ...}.

local function mkstr(n, ch) return string.rep(string.char(ch), n) end

-- 1) Empty diff (identical buffers) → no regions.
do
    local r = _legion_detectRegions(mkstr(32, 0xFF), mkstr(32, 0xFF), 16)
    assert_equal(#r, 0, "identical buffers must produce 0 regions")
end

-- 2) Empty input → no regions.
do
    local r = _legion_detectRegions("", "", 16)
    assert_equal(#r, 0)
end

-- 3) Size mismatch → no regions (caller must align first).
do
    local r = _legion_detectRegions(mkstr(32, 0x00), mkstr(30, 0x00), 16)
    assert_equal(#r, 0)
end

-- 4) Single byte changed at offset 10.
do
    local orig  = mkstr(32, 0x00)
    local mod   = mkstr(10, 0x00) .. string.char(0xAA) .. mkstr(21, 0x00)
    local r = _legion_detectRegions(orig, mod, 16)
    assert_equal(#r, 1, "expected single region")
    assert_equal(r[1].startAddr, 10)
    assert_equal(r[1].endAddr,   10)
    assert_equal(r[1].size,      1)
end

-- 5) Two diffs within K=16 → merged into one region spanning gap.
do
    local orig = mkstr(64, 0x00)
    -- diffs at 10 and 20 (gap 10), K=16
    local mod  = mkstr(10, 0x00) .. string.char(0xAA) ..
                 mkstr(9, 0x00)  .. string.char(0xBB) ..
                 mkstr(43, 0x00)
    local r = _legion_detectRegions(orig, mod, 16)
    assert_equal(#r, 1, "two close diffs must merge")
    assert_equal(r[1].startAddr, 10)
    assert_equal(r[1].endAddr,   20)
    assert_equal(r[1].size,      11)
end

-- 6) Two diffs separated by > K → two distinct regions.
do
    local orig = mkstr(128, 0x00)
    -- diffs at 10 and 80 (gap 70), K=16
    local mod  = mkstr(10, 0x00) .. string.char(0xAA) ..
                 mkstr(69, 0x00) .. string.char(0xBB) ..
                 mkstr(47, 0x00)
    local r = _legion_detectRegions(orig, mod, 16)
    assert_equal(#r, 2, "far-apart diffs must split into two regions")
    assert_equal(r[1].startAddr, 10)
    assert_equal(r[1].endAddr,   10)
    assert_equal(r[2].startAddr, 80)
    assert_equal(r[2].endAddr,   80)
end

-- 7) Whole buffer differs → single region [0..n-1].
do
    local n = 16
    local r = _legion_detectRegions(mkstr(n, 0x00), mkstr(n, 0xFF), 1)
    assert_equal(#r, 1)
    assert_equal(r[1].startAddr, 0)
    assert_equal(r[1].endAddr,   n - 1)
    assert_equal(r[1].size,      n)
end

-- 8) K=0 → each diff is its own region (no adjacency tolerance).
do
    local orig = mkstr(8, 0x00)
    -- diffs at 0, 2, 4 — with K=0, gaps of 2 each break runs.
    local mod  = string.char(0xAA) .. string.char(0x00) ..
                 string.char(0xAA) .. string.char(0x00) ..
                 string.char(0xAA) .. mkstr(3, 0x00)
    local r = _legion_detectRegions(orig, mod, 0)
    assert_equal(#r, 3, "K=0 means every isolated diff is its own region")
end

-- 9) Sparse modification (8 diffs within 16-byte window, K=16) → 1 region.
do
    local n = 64
    local orig = mkstr(n, 0x00)
    local mb = {}
    for i = 1, n do mb[i] = 0x00 end
    -- Plant diffs at 5,7,9,11,13,15,17,19 — all within K=16 chains.
    for _, idx in ipairs({5, 7, 9, 11, 13, 15, 17, 19}) do
        mb[idx + 1] = 0xAA   -- Lua is 1-indexed; mb[i+1] = byte at address i
    end
    local modStr = ""
    for i = 1, n do modStr = modStr .. string.char(mb[i]) end
    local r = _legion_detectRegions(orig, modStr, 16)
    assert_equal(#r, 1, "tight cluster must collapse to one region")
    assert_equal(r[1].startAddr, 5)
    assert_equal(r[1].endAddr,   19)
end

Log("test_legion_detectregions OK")
