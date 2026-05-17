package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- P0-1 sandbox: verify dangerous stdlib functions are unreachable.
-- Note: legitimate uses (os.remove of a tmpfile, io.open inside cwd) still
-- work; see test_iter9_intelhex_roundtrip.lua etc. for coverage of those.

-- 1) Code-execution gateways are nil.
assert_equal(os.execute,        nil, "os.execute must be sandboxed")
assert_equal(os.exit,           nil, "os.exit must be sandboxed")
assert_equal(io.popen,          nil, "io.popen must be sandboxed")
assert_equal(package.loadlib,   nil, "package.loadlib must be sandboxed")
assert_equal(dofile,            nil, "dofile must be sandboxed")
assert_equal(loadfile,          nil, "loadfile must be sandboxed")
assert_equal(load,              nil, "load must be sandboxed")

-- 2) Path-guarded ops: parent-traversal and absolute escape both rejected.
do
    -- Create a sentinel file inside cwd; sandbox must allow its removal.
    local p = "tests/lua/fixtures/tmp_sandbox_ok.bin"
    local f = io.open(p, "wb"); assert_true(f ~= nil); f:write("x"); f:close()
    assert_equal(os.remove(p), true, "os.remove inside cwd is allowed")
end

do
    -- Outside cwd + outside tempPath: rejected (returns false, no throw).
    -- Try a Windows-style absolute that's definitely outside our build dir.
    local r = os.remove("C:/Windows/System32/drivers/etc/hosts")
    assert_equal(r, false, "absolute path outside cwd/tmp must be rejected")
end

do
    -- Traversal attempt: even relative-looking paths with `..` are rejected.
    local r = os.remove("../../../etc/passwd")
    assert_equal(r, false, "parent-directory traversal must be rejected")
end

do
    -- os.tmpname must yield a path that os.remove (sandboxed) accepts.
    local tmp = os.tmpname()
    assert_true(type(tmp) == "string" and #tmp > 0)
    -- The tmpname helper creates the file already; remove it and confirm.
    assert_equal(os.remove(tmp), true)
end

Log("test_sandbox OK")
