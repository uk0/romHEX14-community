package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.4 — verdict classification + tags + ranking.

-- Build a synthetic verdict with given per-cell stats.
local function verdict(startAddr, cellsSpec)
    -- cellsSpec = {{count, mean, std}, ...}
    local cells = {}
    local maxC = 0
    for i, c in ipairs(cellsSpec) do
        cells[i] = { sampleCount = c[1], meanDelta = c[2], stdDevDelta = c[3] }
        if c[1] > maxC then maxC = c[1] end
    end
    return {
        startAddr = startAddr, endAddr = startAddr + #cellsSpec - 1,
        cellSize = 1, rows = 1, cols = #cellsSpec,
        kind = "Curve", maxSampleCount = maxC,
        cells = cells,
    }
end

-- 1) Empty list — no crash.
do
    local r = _legion_classify({}, 10)
    assert_equal(#r, 0)
end

-- 2) totalVoices=0 — no-op (returns empty / no classification).
do
    local r = _legion_classify({verdict(0, {{5, 16.0, 0.0}})}, 0)
    -- Still gets some default tag — at least no crash.
    assert_equal(#r, 1)
end

-- 3) Unanimous: every voice contributed, all gave same delta (std=0).
do
    local r = _legion_classify({verdict(0, {{20, 16.0, 0.0}})}, 20)
    assert_equal(#r, 1)
    assert_equal(r[1].tag, "Unanimous")
    assert_true(r[1].consensusStrength >= 0.99,
                "unanimous → consensusStrength near 1.0")
end

-- 4) StrongConsensus: 80% voices, low CV.
do
    local r = _legion_classify({verdict(0, {{16, 10.0, 0.5}})}, 20)  -- 80%, CV=0.05
    assert_equal(r[1].tag, "StrongConsensus")
end

-- 5) Majority: 50% voices contributed.
do
    local r = _legion_classify({verdict(0, {{10, 5.0, 4.0}})}, 20)  -- 50%, CV=0.8
    assert_equal(r[1].tag, "Majority")
end

-- 6) Heretic: <10% voices contributed.
do
    local r = _legion_classify({verdict(0, {{1, 16.0, 0.0}})}, 20)   -- 5%
    assert_equal(r[1].tag, "Heretic")
end

-- 7) Contested: between heretic and majority threshold (≥10% but <50%).
do
    local r = _legion_classify({verdict(0, {{5, 5.0, 20.0}})}, 20)   -- 25%, mixed
    assert_equal(r[1].tag, "Contested")
end

-- 8) Many voices but disagreement (high CV) → Majority not Strong.
do
    local r = _legion_classify({verdict(0, {{18, 1.0, 5.0}})}, 20)   -- 90% but CV=5
    -- ≥80% by count but CV too high → falls through to Majority.
    assert_equal(r[1].tag, "Majority")
end

-- 9) All voices but voices disagree → not Unanimous (CV too high).
do
    local r = _legion_classify({verdict(0, {{20, 1.0, 5.0}})}, 20)   -- 100% but CV=5
    -- Fails Unanimous tight check; fails StrongConsensus CV<0.2; falls to Majority.
    assert_equal(r[1].tag, "Majority")
end

-- 10) Ranking: stronger consensus comes first after classify.
do
    local v_weak   = verdict(100, {{5, 1.0, 10.0}})   -- contested
    local v_strong = verdict(200, {{20, 16.0, 0.0}})  -- unanimous
    local r = _legion_classify({v_weak, v_strong}, 20)
    assert_equal(#r, 2)
    assert_equal(r[1].tag, "Unanimous", "strongest first")
    assert_equal(r[1].startAddr, 200)
    assert_true(r[1].consensusStrength > r[2].consensusStrength)
end

-- 11) Multi-cell verdict: pick densest cell for tagging.
do
    -- Two cells: sparse cell + dense cell. Tag should follow dense one.
    local v = verdict(50, {{2, 1.0, 5.0}, {20, 16.0, 0.0}})
    local r = _legion_classify({v}, 20)
    assert_equal(r[1].tag, "Unanimous", "densest cell governs the tag")
end

Log("test_legion_classify OK")
