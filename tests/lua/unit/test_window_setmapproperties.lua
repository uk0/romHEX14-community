package.path = package.path .. ";tests/lua/?.lua"
require("test_helper")

projectAddMap()
windowSetMapProperties("Name", "TestX", TRUE)
windowSetMapProperties("Spalten", 16, TRUE)
windowSetMapProperties("Zeilen", 8, TRUE)
windowSetMapProperties("Feldwerte.StartAddr", 0x1000, TRUE)
windowSetMapProperties("StuetzX.DataAddr", 0x2000, TRUE)
windowSetMapProperties("bRueckwaerts", 0, TRUE)  -- side-map storage
assert_equal(windowGetMapProperties("Name"), "TestX")
assert_equal(1 * windowGetMapProperties("Spalten"), 16)
Log("test_window_setmapproperties OK")
