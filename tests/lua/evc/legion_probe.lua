-- LEGION pre-flight probe v2 — voice clustering + per-cluster aggregation.
--
-- Validates the full LEGION concept end-to-end on real data:
--   1) Find similar voices (>=85% overall).
--   2) For each voice: import, snapshot diff vs. user baseline.
--   3) Cluster voices by Jaccard similarity of their address sets.
--   4) For the LARGEST cluster, aggregate per-cell deltas.
--   5) Print top verdicts.

package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

local SRC_BIN = env_path("ROMHEX14_CUSTOMER_BIN")
if not SRC_BIN then Log("set ROMHEX14_CUSTOMER_BIN first"); return end

local MAX_VOICES   = 30           -- cap for probe runtime
local CHUNK        = 65536        -- 64 KB read window
local JACCARD_MIN  = 0.50         -- voices with set-similarity >= this go to same cluster
local CLUSTER_K    = 16           -- region clustering adjacency window

-- 1) Load user baseline.
projectImport(SRC_BIN, eFiletypeBinary)
local USER_SIZE = tonumber(projectGetProperty(ePrjPropEcuSoftwaresize))
Log(string.format("loaded user ROM (%d bytes)", USER_SIZE))

-- Snapshot baseline in chunks for fast comparison later.
local baseline = {}
for base = 0, USER_SIZE - 1, CHUNK do
    local n = math.min(CHUNK, USER_SIZE - base)
    baseline[base] = projectGetAt(base, eByte, n)
end
Log(string.format("baseline snapshot: %d chunks", USER_SIZE // CHUNK))

-- 2) Find voices.
local hits = projectFindSimilarProjectsSql(85, MAX_VOICES,
                eFSPTrippleRelevance, ePrjFilename)
local nCols = 4
local nHits = #hits / nCols
Log(string.format("voices found: %d", math.floor(nHits)))

-- 3) For each voice: snapshot its diff set (sparse list of (addr, delta)).
local voiceDiffs = {}   -- voiceDiffs[i] = { name=fname, addrs={addr1, addr2, ...}, deltas={d1, d2, ...} }

local function applyAndSnapshot(voiceIdx, fname)
    projectImport(SRC_BIN, eFiletypeBinary)   -- reset
    local rc = projectImportChanges(fname, 1)
    if not (rc >= 1 and rc <= 4) then return nil end

    local diff = { name = fname, addrs = {}, deltas = {}, addrSet = {} }
    for base = 0, USER_SIZE - 1, CHUNK do
        local n = math.min(CHUNK, USER_SIZE - base)
        local cur = projectGetAt(base, eByte, n)
        local bs  = baseline[base]
        for i = 1, n do
            if cur[i] ~= bs[i] then
                local addr = base + i - 1
                diff.addrs[#diff.addrs + 1]  = addr
                diff.deltas[#diff.deltas + 1] = cur[i] - bs[i]
                diff.addrSet[addr] = true
            end
        end
    end
    return diff
end

local t0 = timeGetTime()
for i = 0, math.min(MAX_VOICES, math.floor(nHits)) - 1 do
    local fname = hits[i * nCols + 4]
    if type(fname) == "string" then
        local diff = applyAndSnapshot(i + 1, fname)
        if diff then
            voiceDiffs[#voiceDiffs + 1] = diff
            if (i + 1) % 5 == 0 then
                Log(string.format("  ... %d/%d voices processed (%d ms elapsed)",
                                  i + 1, MAX_VOICES, timeGetTime() - t0))
            end
        end
    end
end
Log(string.format("voice diff capture: %d voices, %d ms total",
                  #voiceDiffs, timeGetTime() - t0))

-- 4) Jaccard clustering.
-- |A ∩ B| / |A ∪ B|.  Greedy: seed = voice with largest set, gather all neighbors >= JACCARD_MIN.
local function jaccard(a, b)
    local inter = 0
    for addr in pairs(a.addrSet) do
        if b.addrSet[addr] then inter = inter + 1 end
    end
    local sizeA = 0; for _ in pairs(a.addrSet) do sizeA = sizeA + 1 end
    local sizeB = 0; for _ in pairs(b.addrSet) do sizeB = sizeB + 1 end
    local union = sizeA + sizeB - inter
    if union == 0 then return 0.0 end
    return inter / union
end

-- Sort voices by diff size descending — start clustering from biggest.
table.sort(voiceDiffs, function(a, b) return #a.addrs > #b.addrs end)

local clusters = {}
local claimed = {}
for i, v in ipairs(voiceDiffs) do
    if not claimed[i] then
        local cluster = { members = { i }, name = v.name }
        claimed[i] = true
        for j = i + 1, #voiceDiffs do
            if not claimed[j] then
                if jaccard(v, voiceDiffs[j]) >= JACCARD_MIN then
                    cluster.members[#cluster.members + 1] = j
                    claimed[j] = true
                end
            end
        end
        clusters[#clusters + 1] = cluster
    end
end
table.sort(clusters, function(a, b) return #a.members > #b.members end)

-- 5) Report clusters.
Log("")
Log("=== voice clusters ===")
for ci, cluster in ipairs(clusters) do
    if #cluster.members < 2 then break end   -- skip singletons
    -- Address signature: count how many addresses are shared across cluster
    local sharedAddrs = {}
    for _, mi in ipairs(cluster.members) do
        for addr in pairs(voiceDiffs[mi].addrSet) do
            sharedAddrs[addr] = (sharedAddrs[addr] or 0) + 1
        end
    end
    -- Find dominant address range
    local minA, maxA = math.huge, 0
    local consensusAddrs = 0
    for addr, count in pairs(sharedAddrs) do
        if count >= #cluster.members / 2 then
            consensusAddrs = consensusAddrs + 1
            if addr < minA then minA = addr end
            if addr > maxA then maxA = addr end
        end
    end
    -- Filename keyword scan
    local keywords = { stage=0, dpf=0, egr=0, adblue=0, immo=0, lambda=0 }
    for _, mi in ipairs(cluster.members) do
        local fname = voiceDiffs[mi].name:lower()
        for kw, _ in pairs(keywords) do
            if fname:find(kw, 1, true) then keywords[kw] = keywords[kw] + 1 end
        end
    end
    local kwTags = ""
    for kw, count in pairs(keywords) do
        if count > 0 then
            kwTags = kwTags .. string.format(" %s×%d", kw, count)
        end
    end
    Log(string.format("Cluster %d: %d voices  range[0x%X..0x%X]  consensus_addrs=%d  tags:%s",
        ci, #cluster.members, minA, maxA, consensusAddrs,
        kwTags == "" and " (no keywords in filenames)" or kwTags))
end

-- 6) Aggregate within LARGEST cluster, print top verdicts.
local largest = clusters[1]
if largest and #largest.members >= 2 then
    Log("")
    Log(string.format("=== aggregating largest cluster (%d voices) ===", #largest.members))
    local agg = {}   -- agg[addr] = { count, sumDelta, sumSqDelta }
    for _, mi in ipairs(largest.members) do
        local d = voiceDiffs[mi]
        for i = 1, #d.addrs do
            local a = d.addrs[i]
            local e = agg[a] or { c = 0, s = 0, sq = 0 }
            e.c  = e.c + 1
            e.s  = e.s + d.deltas[i]
            e.sq = e.sq + d.deltas[i] * d.deltas[i]
            agg[a] = e
        end
    end
    -- Cluster contiguous addrs into verdicts.
    local sorted = {}; for a in pairs(agg) do sorted[#sorted + 1] = a end
    table.sort(sorted)
    local verdicts = {}
    local cur = nil
    for _, a in ipairs(sorted) do
        if cur and a - cur.endA <= CLUSTER_K then
            cur.endA = a
            cur.addrs[#cur.addrs + 1] = a
        else
            cur = { startA = a, endA = a, addrs = { a } }
            verdicts[#verdicts + 1] = cur
        end
    end
    -- Score: max sample-count within verdict
    for _, v in ipairs(verdicts) do
        local maxC, sumC = 0, 0
        local nCells = #v.addrs
        for _, a in ipairs(v.addrs) do
            local e = agg[a]
            if e.c > maxC then maxC = e.c end
            sumC = sumC + e.c
        end
        v.maxSample = maxC
        v.coverage  = sumC / (nCells * #largest.members)
        v.size      = v.endA - v.startA + 1
        v.cells     = nCells
    end
    table.sort(verdicts, function(a, b) return a.maxSample > b.maxSample end)

    Log(string.format("found %d candidate verdicts. Top 15:", #verdicts))
    for i = 1, math.min(15, #verdicts) do
        local v = verdicts[i]
        -- Sample mean+stdev at the densest address in this verdict
        local densest = v.addrs[1]
        for _, a in ipairs(v.addrs) do
            if agg[a].c > agg[densest].c then densest = a end
        end
        local e = agg[densest]
        local mean = e.s / e.c
        local var  = e.sq / e.c - mean * mean
        local stdev = math.sqrt(math.max(0, var))
        local tag = ""
        if v.maxSample == #largest.members then tag = "UNANIMOUS"
        elseif v.maxSample >= #largest.members * 0.8 then tag = "STRONG"
        elseif v.maxSample >= #largest.members * 0.5 then tag = "MAJORITY"
        else tag = "CONTESTED" end
        Log(string.format("  #%d  0x%X..0x%X  size=%dB  cells=%d  maxSample=%d/%d  meanΔ=%+.1f  std=%.1f  %s",
            i, v.startA, v.endA, v.size, v.cells, v.maxSample, #largest.members,
            mean, stdev, tag))
    end
end

-- Final restore
projectImport(SRC_BIN, eFiletypeBinary)
Log("")
Log("LEGION probe done.")
