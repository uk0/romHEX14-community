package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.9 — verify the one-shot legionInvoke() pipeline.

local function rep(n, ch) return string.rep(string.char(ch), n) end
local function bs(...)
    local out = ""
    for _, v in ipairs({...}) do out = out .. string.char(v) end
    return out
end

local function makeBuf(size, fillByte, mods)
    -- mods = {{addr, byte}, ...}
    local t = {}
    for i = 1, size do t[i] = string.char(fillByte) end
    for _, m in ipairs(mods or {}) do
        t[m[1] + 1] = string.char(m[2])
    end
    return table.concat(t)
end

-- 1) Empty input → empty result.
do
    local r = legionInvoke({ baseline = "", voices = {} })
    assert_equal(r.voices, 0)
    assert_equal(#r.clusters, 0)
end

-- 2) Voice with no diff → not counted (addressSet empty).
do
    local same = rep(64, 0x10)
    local r = legionInvoke({
        baseline = same,
        voices = {
            { sourcePath = "no_change.ols",
              originalBytes = same, modifiedBytes = same },
        },
    })
    assert_equal(r.voices, 0, "voice with no diff should not be ingested")
end

-- 3) Two voices identical change → one 2-member cluster, verdict tagged.
do
    local baseline = rep(64, 0x10)
    local v_orig = baseline
    -- modify byte at 20 to +16
    local v_mod  = makeBuf(64, 0x10, {{20, 0x20}})
    local r = legionInvoke({
        baseline = baseline,
        voices = {
            { sourcePath = "a.ols",
              originalBytes = v_orig, modifiedBytes = v_mod },
            { sourcePath = "b.ols",
              originalBytes = v_orig, modifiedBytes = v_mod },
        },
    })
    assert_equal(r.voices, 2)
    assert_equal(#r.clusters, 1, "expected one cluster")
    local c = r.clusters[1]
    assert_equal(c.memberCount, 2)
    assert_equal(#c.verdicts, 1, "one verdict")
    local v = c.verdicts[1]
    assert_equal(v.tag, "Unanimous")
    assert_true(v.consensusStrength > 0.9)
    assert_equal(v.maxSampleCount, 2)
    assert_equal(#v.contributingVoices, 2)
end

-- 4) Three voices: 2 stage-like (overlap) + 1 dpf (disjoint) → 2 clusters.
do
    local baseline = rep(0x100, 0x00)
    -- stage1 voices touch addrs 10..15
    local stageMods = {}
    for a = 10, 15 do table.insert(stageMods, {a, 0x40}) end
    local stage_v0 = baseline
    local stage_v1 = makeBuf(0x100, 0x00, stageMods)
    -- dpf voice touches 0x80..0x84
    local dpfMods = {}
    for a = 0x80, 0x84 do table.insert(dpfMods, {a, 0x80}) end
    local dpf_v0 = baseline
    local dpf_v1 = makeBuf(0x100, 0x00, dpfMods)

    local r = legionInvoke({
        baseline = baseline,
        voices = {
            { sourcePath = "stage1_a.ols",
              originalBytes = stage_v0, modifiedBytes = stage_v1 },
            { sourcePath = "stage1_b.ols",
              originalBytes = stage_v0, modifiedBytes = stage_v1 },
            { sourcePath = "DPF_off.ols",
              originalBytes = dpf_v0,   modifiedBytes = dpf_v1   },
        },
    })
    assert_equal(r.voices, 3)
    assert_equal(#r.clusters, 2, "stage cluster + dpf singleton")
    -- Largest cluster first; should be the 2-voice stage cluster.
    assert_equal(r.clusters[1].memberCount, 2)
    assert_equal(r.clusters[1].label, "stage-like")
    -- Singleton has no verdicts (we skip aggregation for <2 members).
    assert_equal(#r.clusters[2].verdicts, 0)
    -- Stage cluster has at least one verdict.
    assert_true(#r.clusters[1].verdicts >= 1)
end

-- 5) Result shape sanity-check on a verdict (all expected keys present).
do
    local baseline = rep(32, 0x20)
    local v_mod    = makeBuf(32, 0x20, {{5, 0x30}})
    local r = legionInvoke({
        baseline = baseline,
        voices = {
            { sourcePath = "x.ols", originalBytes = baseline, modifiedBytes = v_mod },
            { sourcePath = "y.ols", originalBytes = baseline, modifiedBytes = v_mod },
        },
    })
    assert_equal(#r.clusters, 1)
    local v = r.clusters[1].verdicts[1]
    assert_true(v.startAddr      ~= nil)
    assert_true(v.endAddr        ~= nil)
    assert_true(v.cellSize       ~= nil)
    assert_true(v.bigEndian      ~= nil)
    assert_true(v.rows           ~= nil)
    assert_true(v.cols           ~= nil)
    assert_true(v.kind           ~= nil)
    assert_true(v.tag            ~= nil)
    assert_true(v.consensusStrength ~= nil)
    assert_true(type(v.cells) == "table")
    assert_true(type(v.contributingVoices) == "table")
end

-- 6) Options threshold: jaccardMin=0.99 forces split even on near-identical sets.
do
    local baseline = rep(64, 0x00)
    -- two voices, 4/5 address overlap → Jaccard 4/6 ≈ 0.67
    local v1 = makeBuf(64, 0x00, {{10, 0x10}, {11, 0x10}, {12, 0x10}, {13, 0x10}, {14, 0x10}})
    local v2 = makeBuf(64, 0x00, {{10, 0x10}, {11, 0x10}, {12, 0x10}, {13, 0x10}, {15, 0x10}})
    local r = legionInvoke({
        baseline = baseline,
        jaccardMin = 0.99,
        voices = {
            { sourcePath = "a.ols", originalBytes = baseline, modifiedBytes = v1 },
            { sourcePath = "b.ols", originalBytes = baseline, modifiedBytes = v2 },
        },
    })
    assert_equal(#r.clusters, 2,
                 "0.99 threshold should split partial-overlap voices")
end

Log("test_legion_invoke OK")
