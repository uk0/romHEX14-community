package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

-- Skipped at runtime (would block on dialog), but referenced for coverage:
assert_true(type(TextEntryDialog) == "function")
if false then TextEntryDialog(0, "t", "d", "") end   -- protected
Log("test_global_textentrydialog OK")
