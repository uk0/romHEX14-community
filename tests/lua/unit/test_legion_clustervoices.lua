package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.3a — voice clustering by Jaccard on address sets.
-- _legion_clusterVoices(voicesTbl, jaccardMin?) returns array of clusters,
-- sorted by memberCount descending.

local function voice(name, addrs)
    return { sourcePath = name, addrs = addrs }
end

-- Helper: pick the cluster that contains a given 1-based voice index.
local function clusterOf(clusters, voiceIdx)
    for _, c in ipairs(clusters) do
        for _, vi in ipairs(c.voiceIndices) do
            if vi == voiceIdx then return c end
        end
    end
    return nil
end

-- 1) Empty input → no clusters.
do
    local r = _legion_clusterVoices({}, 0.5)
    assert_equal(#r, 0)
end

-- 2) Single voice → one singleton cluster.
do
    local r = _legion_clusterVoices({voice("a", {1, 2, 3})}, 0.5)
    assert_equal(#r, 1)
    assert_equal(r[1].memberCount, 1)
    assert_equal(r[1].voiceIndices[1], 1)
end

-- 3) Two voices, identical address sets → one cluster of 2.
do
    local r = _legion_clusterVoices({
        voice("a", {10, 20, 30}),
        voice("b", {10, 20, 30}),
    }, 0.5)
    assert_equal(#r, 1)
    assert_equal(r[1].memberCount, 2)
end

-- 4) Two voices, disjoint address sets → two singleton clusters.
do
    local r = _legion_clusterVoices({
        voice("a", {1, 2, 3}),
        voice("b", {100, 101, 102}),
    }, 0.5)
    assert_equal(#r, 2)
    assert_equal(r[1].memberCount, 1)
    assert_equal(r[2].memberCount, 1)
end

-- 5) Three voices: two stage1-ish (high overlap) + one DPF-ish (disjoint).
do
    local stageA = {0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005}
    local stageB = {0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1006}  -- 5/7 overlap
    local dpf    = {0x8000, 0x8001, 0x8002}
    local r = _legion_clusterVoices({
        voice("stage1_a.ols",   stageA),
        voice("stage1_b.ols",   stageB),
        voice("DPF_off_x.ols",  dpf),
    }, 0.5)
    -- Expect: cluster {1,2} (stage-like) and singleton {3}.
    assert_equal(#r, 2)
    assert_equal(r[1].memberCount, 2)
    assert_equal(r[2].memberCount, 1)
    -- Largest cluster first.
    local cStage = r[1]
    assert_true(cStage.voiceIndices[1] == 1 or cStage.voiceIndices[1] == 2)
    -- Label picked up the "stage" keyword.
    assert_equal(cStage.label, "stage-like")
end

-- 6) Jaccard threshold respected: 0.51 should split partial-overlap pair.
do
    local a = {1, 2, 3, 4, 5}
    local b = {3, 4, 5, 6, 7}   -- 3/7 ≈ 0.43
    local r = _legion_clusterVoices({voice("a", a), voice("b", b)}, 0.51)
    assert_equal(#r, 2, "voices below threshold must stay separate")
end

-- 7) Sorted by member count descending.
do
    local big1 = {1, 2, 3, 4, 5, 6}
    local big2 = {1, 2, 3, 4, 5, 7}   -- 5/7 ≈ 0.71
    local big3 = {1, 2, 3, 4, 5, 8}   -- 5/7 ≈ 0.71 vs both
    local sm   = {100}                -- singleton
    local r = _legion_clusterVoices({
        voice("sm", sm),
        voice("b1", big1),
        voice("b2", big2),
        voice("b3", big3),
    }, 0.5)
    -- Order should be: 3-member cluster first, then singleton.
    assert_equal(r[1].memberCount, 3)
    assert_equal(r[2].memberCount, 1)
end

-- 8) addrRangeMin / addrRangeMax cover consensus addresses.
do
    local r = _legion_clusterVoices({
        voice("a", {0x4000, 0x4001, 0x4002}),
        voice("b", {0x4000, 0x4001, 0x4002}),
    }, 0.5)
    assert_equal(r[1].addrRangeMin, 0x4000)
    assert_equal(r[1].addrRangeMax, 0x4002)
    assert_equal(r[1].consensusAddrCount, 3)
end

-- 9) Mixed-intent filename → label is "mixed".
do
    local addrs = {1, 2, 3, 4, 5}
    local r = _legion_clusterVoices({
        voice("plain_a.ols", addrs),
        voice("plain_b.ols", addrs),
        voice("dpf_only.ols", addrs),
    }, 0.5)
    -- All in one cluster (identical sets); dpf appears in 1/3 < 50% → "mixed (dpf)".
    assert_equal(#r, 1)
    assert_equal(r[1].memberCount, 3)
    assert_true(r[1].label:find("mixed") ~= nil,
                "minority-keyword cluster should label as 'mixed'")
end

Log("test_legion_clustervoices OK")
