package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- EVC sample 04 "Simple file processor" — straight-line script that
-- imports a binary, applies checksums, exports as BdmToGo, mails.
-- Cannot run end-to-end (paths point to "C:\\yourdirectory\\") but we
-- syntax-check and assert our documented contract for each API.
-- Sample loadfile is conditional on $ROMHEX14_LUA_SAMPLES being set.

local samplePath = env_path("ROMHEX14_LUA_SAMPLES",
                            "04 - Simple file processor/main.lua")
if samplePath then
    local f, err = loadfile(samplePath)
    assert_true(f ~= nil, "sample 04 must parse: "..tostring(err))
end

-- Iter 8.1: checksums must be honest-false / -1 (was fake true/0).
-- This is the flash-safety guarantee — the sample's `if (projectStatChecksums()~=0)`
-- check will now route to ErrorMail when it would have silently shipped.
assert_equal(projectApplyChecksums(), false)
assert_equal(projectStatChecksums(),  -1)

-- BdmToGo export is plugin-gated; we honest-false rather than silently
-- writing a binary file with the wrong layout.
local ok = projectExport("tests/lua/fixtures/tmp_bdm.bin", eFiletypeBdmToGo)
assert_equal(ok, false, "BdmToGo export must fail honestly")

-- projectMail is OOS in community build (no MAPI client).
assert_true(type(projectMail) == "function")
local mailOk = projectMail("a@b.com", "subj", "body", "attach.bin")
assert_equal(mailOk, false, "projectMail OOS in community")

-- Quit must be callable (we don't invoke it — would kill the engine).
assert_true(type(Quit) == "function")

Log("test_evc_04_file_processor OK"..(samplePath and "" or " (sample skipped)"))
