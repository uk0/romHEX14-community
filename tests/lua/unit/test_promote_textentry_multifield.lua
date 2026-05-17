package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- New-form TextEntryDialog: title, then (mode, desc, default) triplets.
-- In RX14_LUA_TEST mode the dialog is bypassed and string defaults are
-- returned as an array. Defaults must be strings — sol2 casts them via
-- as<std::string>() per the binding.
local r = TextEntryDialog("Pick options",
    eTextEntryNormal,   "RPM",          "3500",
    eTextEntryPassword, "PIN",          "1234",
    eTextEntryCheckbox, "Enable DPF",   "1",
    eTextEntryCheckbox, "Enable EGR",   "0")

assert_true(type(r) == "table", "expected table, got "..type(r))
assert_equal(r[1], "3500")
assert_equal(r[2], "1234")
assert_equal(r[3], "1")
assert_equal(r[4], "0")
Log("test_promote_textentry_multifield OK")
