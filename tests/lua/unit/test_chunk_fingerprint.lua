package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- CHUNK.1 — verify the byte-twin fingerprint primitive.

local CHUNK = 16384

-- 1) Empty input → empty fingerprint.
do
    local fp = _chunkFingerprint("")
    assert_equal(fp.nChunks, 0)
    assert_equal(fp.fileSize, 0)
end

-- 2) Sub-chunk input → one chunk.
do
    local fp = _chunkFingerprint(string.rep("\xAB", 1024))
    assert_equal(fp.nChunks, 1)
    assert_equal(fp.fileSize, 1024)
    assert_equal(#fp.hashesHex, 16)
end

-- 3) Exact 2 chunks.
do
    local data = string.rep("\x00", CHUNK) .. string.rep("\xFF", CHUNK)
    local fp = _chunkFingerprint(data)
    assert_equal(fp.nChunks, 2)
    assert_equal(fp.fileSize, 2 * CHUNK)
    assert_equal(#fp.hashesHex, 32)
    -- Two different chunks → two different hashes.
    local h1 = fp.hashesHex:sub(1, 16)
    local h2 = fp.hashesHex:sub(17, 32)
    assert_true(h1 ~= h2, "different chunk content → different hash")
end

-- 4) Identical content → 100% containment.
do
    local data = string.rep("\xAA\xBB\xCC\xDD", 4096)   -- 16 KB
    assert_equal(_chunkContainment(data, data), 1.0)
end

-- 5) Subset: needle inside haystack at any offset → 100% containment
--    (because chunks are aligned to the CHUNK_SIZE grid and same bytes
--    produce same hash regardless of where they sit).
do
    local block = string.rep("\xDE\xAD\xBE\xEF", 4096)  -- 1 chunk
    local needle = block
    local haystack = string.rep("\x00", CHUNK) .. block .. string.rep("\x00", CHUNK)
    -- Haystack has 3 chunks: zero, block, zero.  Needle = block.  Should
    -- be 1 / 1 = 100%.
    assert_equal(_chunkContainment(needle, haystack), 1.0)
end

-- 6) Disjoint content → 0% containment.
do
    local a = string.rep("\xAA", CHUNK)
    local b = string.rep("\xBB", CHUNK)
    assert_equal(_chunkContainment(a, b), 0.0)
end

-- 7) Partial overlap.  2-chunk needle, only 1 chunk shared → 50%.
do
    local shared = string.rep("\x01\x02\x03\x04", 4096)
    local needle = shared .. string.rep("\x55", CHUNK)
    local hay    = shared .. string.rep("\x99", CHUNK)
    assert_equal(_chunkContainment(needle, hay), 0.5)
end

-- 8) Small tuning edit (one byte changed inside one chunk) ruins
--    EXACTLY that chunk and no other.  For a 4-chunk needle that
--    means 3/4 = 75% containment.
do
    local base = ""
    for i = 0, 3 do base = base .. string.rep(string.char(i + 1), CHUNK) end
    -- Patch byte 5 inside chunk 0 (any byte change breaks just that hash)
    local mod = string.sub(base, 1, 4) .. "\xFF" .. string.sub(base, 6)
    local c = _chunkContainment(mod, base)
    assert_true(math.abs(c - 0.75) < 1e-9,
                "single-byte edit must break exactly one chunk (got "..c..")")
end

Log("test_chunk_fingerprint OK")
