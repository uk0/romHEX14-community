--------------------------------------------------------------------------------------------
-- create_map.lua - modifies / creates a map
--------------------------------------------------------------------------------------------

-- Example 1: Set the number of columns for the **current** (=>"false") window to "10".
windowSetMapProperties ("Spalten", 10, FALSE);

-- Example 2: Create a new map and set some properties. Not of the current window,
-- but of the last map created with lua. (=>"TRUE")
projectAddMap();
windowSetMapProperties ("Name", "Kennfeld", TRUE);
windowSetMapProperties ("IdName", "", TRUE);
windowSetMapProperties ("Typ", eZweidim, TRUE);
windowSetMapProperties ("FolderName", "My maps", TRUE);
windowSetMapProperties ("ViewMode", eViewText, TRUE);
windowSetMapProperties ("RWin", eBars, TRUE);
windowSetMapProperties ("DataOrg", eFloatLoHi, TRUE);
windowSetMapProperties ("bKehrwert", 0, TRUE);
windowSetMapProperties ("bVorzeichen", 0, TRUE);
windowSetMapProperties ("bDelta", 0, TRUE);
windowSetMapProperties ("bProzent", 0, TRUE);
windowSetMapProperties ("bOriginal", 0, TRUE);
windowSetMapProperties ("bOriginalWerte", 0, TRUE);
windowSetMapProperties ("Spalten", 16, TRUE);
windowSetMapProperties ("Zeilen", 3, TRUE);
windowSetMapProperties ("Radix", 10, TRUE);
windowSetMapProperties ("Kommentar", "Mein Kommentar", TRUE);
windowSetMapProperties ("Nachkommastellen", 0, TRUE);
windowSetMapProperties ("SkipBytes", 0, TRUE);
windowSetMapProperties ("LineSkipBytes", 0, TRUE);

windowSetMapProperties ("Feldwerte.Name", "-", TRUE);
windowSetMapProperties ("Feldwerte.Einheit", "-", TRUE);
windowSetMapProperties ("Feldwerte.Faktor", 1.000000, TRUE);
windowSetMapProperties ("Feldwerte.Offset", 0.000000, TRUE);
windowSetMapProperties ("Feldwerte.StartAddr", 7668, TRUE);

windowSetMapProperties ("StuetzX.Name", "-", TRUE);
windowSetMapProperties ("StuetzX.Einheit", "-", TRUE);
windowSetMapProperties ("StuetzX.Faktor", 1.000000, TRUE);
windowSetMapProperties ("StuetzX.Offset", 0.000000, TRUE);
windowSetMapProperties ("StuetzX.DataSrc", eRom, TRUE);
windowSetMapProperties ("StuetzX.DataHeader", 0, TRUE);
windowSetMapProperties ("StuetzX.DataAddr", 4096, TRUE);
windowSetMapProperties ("StuetzX.DataOrg", eFloatLoHi, TRUE);
windowSetMapProperties ("StuetzX.Radix", 10, TRUE);
windowSetMapProperties ("StuetzX.bRueckwaerts", 0, TRUE);
windowSetMapProperties ("StuetzX.bKehrwert", 0, TRUE);
windowSetMapProperties ("StuetzX.bVorzeichen", 0, TRUE);
windowSetMapProperties ("StuetzX.Nachkommastellen", 0, TRUE);
windowSetMapProperties ("StuetzX.SignaturByte", "0xFFFFFFFF", TRUE);
windowSetMapProperties ("StuetzX.SkipBytes", 0, TRUE);

windowSetMapProperties ("StuetzY.Name", "-", TRUE);
windowSetMapProperties ("StuetzY.Einheit", "-", TRUE);
windowSetMapProperties ("StuetzY.Faktor", 1.000000, TRUE);
windowSetMapProperties ("StuetzY.Offset", 0.000000, TRUE);
windowSetMapProperties ("StuetzY.DataSrc", eRom, TRUE);
windowSetMapProperties ("StuetzY.DataHeader", 0, TRUE);
windowSetMapProperties ("StuetzY.DataAddr", 8192, TRUE);
windowSetMapProperties ("StuetzY.DataOrg", eFloatLoHi, TRUE);
windowSetMapProperties ("StuetzY.Radix", 10, TRUE);
windowSetMapProperties ("StuetzY.bRueckwaerts", 0, TRUE);
windowSetMapProperties ("StuetzY.bKehrwert", 0, TRUE);
windowSetMapProperties ("StuetzY.bVorzeichen", 0, TRUE);
windowSetMapProperties ("StuetzY.Nachkommastellen", 0, TRUE);
windowSetMapProperties ("StuetzY.SignaturByte", "0xFFFFFFFF", TRUE);
windowSetMapProperties ("StuetzY.SkipBytes", 0, TRUE);
