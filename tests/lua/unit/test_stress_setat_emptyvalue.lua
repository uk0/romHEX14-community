package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Stress: projectSetAt with eEmptyvalue sentinel restores the byte at addr
-- from originalData (i.e. undo a previous edit).
local addr = 0x103000

-- Capture the original byte (this is what eEmptyvalue should restore to).
local orig = projectGetAt(addr, eByte)

-- Mutate to a definitely-different value.
local mutated = (orig + 0x37) % 256
projectSetAt(addr, mutated, eByte)
assert_equal(projectGetAt(addr, eByte), mutated, "mutation took effect")
assert_true(projectGetAt(addr, eByte) ~= orig, "mutated value differs from orig")

-- Now restore via eEmptyvalue.
projectSetAt(addr, eEmptyvalue, eByte)
assert_equal(projectGetAt(addr, eByte), orig, "eEmptyvalue restored original")

Log("test_stress_setat_emptyvalue OK")
