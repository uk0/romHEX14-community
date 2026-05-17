package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- LEGION.10 — real-catalog smoke + heuristic tuning probe.
--
-- Runs the FULL pipeline (legionInvoke) against the user's actual
-- SimilarityIndex.  Skips gracefully when $ROMHEX14_CUSTOMER_BIN is unset.
--
-- Note on the dual-tier gate:  we harvest voices via projectImportChanges,
-- which means each voice's `originalBytes` is the user's baseline (not the
-- voice's source ROM Version 0).  The local-hamming gate is therefore
-- vacuous in this probe — it always passes.  The C++ LegionHarvestWorker
-- used by LegionDlg reads voice originals directly from OLS files; that
-- code path exercises the full gate.  Here we validate the rest of the
-- pipeline against real data.

local SRC_BIN = env_path("ROMHEX14_CUSTOMER_BIN")
if not SRC_BIN then
    Log("test_legion_real: ROMHEX14_CUSTOMER_BIN not set, skipping")
    return
end

local MAX_VOICES   = 30
local MIN_SIM_PCT  = 85
local CHUNK        = 65536

-- 1) Load user baseline + snapshot it in chunks.
NewProject()
local imported = projectImport(SRC_BIN, eFiletypeBinary)
if imported ~= true then
    Log("test_legion_real: projectImport failed (returned "..tostring(imported)
        ..") — skipping")
    return
end
local USER_SIZE = tonumber(projectGetProperty(ePrjPropEcuSoftwaresize))
if not USER_SIZE or USER_SIZE <= 0 then
    Log("test_legion_real: project has no size after import — skipping")
    return
end

-- projectGetAt(..., n>1) returns a TABLE of numbers (one per byte).
-- Convert + concatenate into a single bytes-string the C++ side can ingest.
local function readBytesAsString(size)
    local parts = {}
    for base = 0, size - 1, CHUNK do
        local n = math.min(CHUNK, size - base)
        local t = projectGetAt(base, eByte, n)
        local chars = {}
        for i = 1, n do chars[i] = string.char(t[i]) end
        parts[#parts + 1] = table.concat(chars)
    end
    return table.concat(parts)
end

local baseline = readBytesAsString(USER_SIZE)
assert_equal(#baseline, USER_SIZE)

-- 3) Find voices via SQL similarity search.
local hits = projectFindSimilarProjectsSql(MIN_SIM_PCT, MAX_VOICES,
                eFSPTrippleRelevance, ePrjFilename)
local nCols = 4
local nHits = math.floor(#hits / nCols)
if nHits == 0 then
    Log("test_legion_real: no matches in catalog, nothing to validate")
    return
end
Log(string.format("test_legion_real: %d voice candidates from catalog", nHits))

-- 4) For each voice path: snapshot modified bytes after projectImportChanges.
local voices = {}
local t0 = timeGetTime()
for i = 0, math.min(MAX_VOICES, nHits) - 1 do
    local fname = hits[i * nCols + 4]
    if type(fname) == "string" then
        NewProject()
        projectImport(SRC_BIN, eFiletypeBinary)   -- reset to baseline
        local rc = projectImportChanges(fname, 1)
        if rc and rc >= 1 and rc <= 4 then
            voices[#voices + 1] = {
                sourcePath    = fname,
                originalBytes = baseline,
                modifiedBytes = readBytesAsString(USER_SIZE),
            }
        end
    end
end
Log(string.format("test_legion_real: %d voices harvested in %d ms",
                   #voices, timeGetTime() - t0))

if #voices < 2 then
    Log("test_legion_real: not enough voices to cluster, skipping rest")
    return
end

-- 5) Run the full pipeline.
local t1 = timeGetTime()
local r = legionInvoke({
    baseline    = baseline,
    voices      = voices,
    jaccardMin  = 0.50,
    localSimMin = 0.90,
    detectK     = 16,
})
Log(string.format("test_legion_real: legionInvoke ran in %d ms",
                   timeGetTime() - t1))

-- 6) Sanity assertions.
assert_equal(r.voices, #voices,
             "every voice with a non-empty diff should be ingested")
assert_true(#r.clusters >= 1, "at least one cluster")

-- Clusters sorted by member count descending.
for i = 1, #r.clusters - 1 do
    assert_true(r.clusters[i].memberCount >= r.clusters[i + 1].memberCount,
                "clusters must be sorted by memberCount desc")
end

-- 7) Print stats + top verdicts for the biggest cluster.
Log("")
Log("=== LEGION clusters ===")
for ci, c in ipairs(r.clusters) do
    Log(string.format("  cluster %d: %d voices  %-18s  range[0x%X..0x%X]  consensus=%d",
        ci, c.memberCount, c.label,
        math.floor(c.addrRangeMin), math.floor(c.addrRangeMax),
        c.consensusAddrCount))
end

local biggest = r.clusters[1]
if biggest.memberCount >= 2 then
    Log("")
    Log(string.format("=== top verdicts in cluster 1 (%d voices) ===",
                       biggest.memberCount))
    -- Already sorted by consensusStrength after classify().
    local topN = math.min(10, #biggest.verdicts)
    -- Tag distribution.
    local tagCount = { Unanimous=0, StrongConsensus=0, Majority=0,
                       Contested=0, Heretic=0 }
    for _, v in ipairs(biggest.verdicts) do
        tagCount[v.tag] = (tagCount[v.tag] or 0) + 1
    end
    Log(string.format("  tag dist: Unanimous=%d StrongConsensus=%d Majority=%d Contested=%d Heretic=%d",
        tagCount.Unanimous, tagCount.StrongConsensus, tagCount.Majority,
        tagCount.Contested, tagCount.Heretic))

    for i = 1, topN do
        local v = biggest.verdicts[i]
        Log(string.format(
            "  #%-2d  [%-9s]  0x%X..0x%X  %s %dx%d  cs=%dB  maxN=%d/%d  strength=%.3f",
            i, v.tag,
            math.floor(v.startAddr), math.floor(v.endAddr),
            v.kind, v.rows, v.cols, v.cellSize,
            v.maxSampleCount, biggest.memberCount,
            v.consensusStrength))
    end

    -- 8) Heuristic sanity:  at least one verdict in the top-10 should
    --    rise above the Heretic level.  If not, our heuristics are off
    --    or the catalog has too much noise.
    local nonHereticTop = 0
    for i = 1, topN do
        if biggest.verdicts[i].tag ~= "Heretic" then
            nonHereticTop = nonHereticTop + 1
        end
    end
    Log(string.format("test_legion_real: %d/%d non-heretic top verdicts",
                       nonHereticTop, topN))
    assert_true(nonHereticTop >= 1,
                "biggest cluster should produce at least one credible verdict")
end

-- Restore baseline so we don't poison downstream tests.
NewProject()
projectImport(SRC_BIN, eFiletypeBinary)
Log("test_legion_real OK")
