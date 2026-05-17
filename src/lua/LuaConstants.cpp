/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Sprint L §5.5 — bind all e* / MB_* / ID* / TRUE / FALSE constants as Lua
 * globals so WinOLS Lua scripts that reference them by name (e.g.
 * `projectExport(..., eFiletypeBdmToGo)`) just work.
 *
 * Numeric values come from src/lua/LuaPropertyIds.h so the C++ side
 * (Project::propertyById) and the Lua side agree.
 */

#include "LuaEngine.h"
#include "LuaPropertyIds.h"
#include "sol/sol.hpp"

namespace lua {

void bindConstants(sol::state &L)
{
    using namespace lua_ids;

    // ── TRUE / FALSE (WinOLS scripts use uppercase) ──
    L["TRUE"]  = 1;
    L["FALSE"] = 0;

    // ── MessageBox icons / buttons ──
    L["MB_OK"]                = 0x0;
    L["MB_OKCANCEL"]          = 0x1;
    L["MB_ABORTRETRYIGNORE"]  = 0x2;
    L["MB_YESNOCANCEL"]       = 0x3;
    L["MB_YESNO"]             = 0x4;
    L["MB_RETRYCANCEL"]       = 0x5;
    L["MB_ICONERROR"]         = 0x10;
    L["MB_ICONQUESTION"]      = 0x20;
    L["MB_ICONWARNING"]       = 0x30;
    L["MB_ICONINFORMATION"]   = 0x40;

    // ── MessageBox return values ──
    L["IDOK"]     = 1;
    L["IDCANCEL"] = 2;
    L["IDABORT"]  = 3;
    L["IDRETRY"]  = 4;
    L["IDIGNORE"] = 5;
    L["IDYES"]    = 6;
    L["IDNO"]     = 7;

    // ── TextEntryDialog modes ──
    L["eTextEntrySmall"]    = 0;
    L["eTextEntryPassword"] = 1;
    L["eTextEntryEdit"]     = 2;
    L["eTextEntryCheckbox"] = 3;

    // ── GetVersion ids ──
    L["eWinOLSMajor"] = 1;
    L["eWinOLSMinor"] = 2;
    L["ePluginMajor"] = 3;
    L["ePluginMinor"] = 4;

    // ── Project status ──
    L["ePrjDeveloping"] = ePrjDeveloping;
    L["ePrjFinished"]   = ePrjFinished;
    L["ePrjMaster"]     = ePrjMaster;

    // ── Datatypes ──
    L["eByte"]       = eByte;
    L["eLoHi"]       = eLoHi;
    L["eHiLo"]       = eHiLo;
    L["eLoHiLoHi"]   = eLoHiLoHi;
    L["eHiLoHiLo"]   = eHiLoHiLo;
    L["eFloatLoHi"]  = eFloatLoHi;
    L["eFloatHiLo"]  = eFloatHiLo;
    L["eDoubleLoHi"] = eDoubleLoHi;
    L["eDoubleHiLo"] = eDoubleHiLo;
    L["eBitLoHi"]    = eBitLoHi;
    L["eBitHiLo"]    = eBitHiLo;

    // ── Set mode (projectSetAt) ──
    L["eSetAbsolute"] = eSetAbsolute;
    L["eSetRelative"] = eSetRelative;
    L["eSetPercent"]  = eSetPercent;
    L["eEmptyvalue"]  = eEmptyvalue;

    // ── Element type ──
    L["eElementNone"]          = eElementNone;
    L["eElementHeader"]        = eElementHeader;
    L["eElementProcessor"]     = eElementProcessor;
    L["eElementEprom"]         = eElementEprom;
    L["eElementEprom2"]        = eElementEprom2;
    L["eElementEEprom"]        = eElementEEprom;
    L["eElementConfiguration"] = eElementConfiguration;

    // ── File types ──
    L["eFiletypeAuto"]                 = eFiletypeAuto;
    L["eFiletypeBinary"]               = eFiletypeBinary;
    L["eFiletypeWinOLS"]               = eFiletypeWinOLS;
    L["eFiletypeWinOLSAll"]            = eFiletypeWinOLSAll;
    L["eFiletypeIntelHex"]             = eFiletypeIntelHex;
    L["eFiletypeMotorolaHex"]          = eFiletypeMotorolaHex;
    L["eFiletypeEDX"]                  = eFiletypeEDX;
    L["eFiletypeBdmToGo"]              = eFiletypeBdmToGo;
    L["eFiletypeWinOLSX"]              = eFiletypeWinOLSX;
    L["eFiletypeWinOLSAllX"]           = eFiletypeWinOLSAllX;
    L["eFiletypeVBF"]                  = eFiletypeVBF;
    L["eFiletypeCMDSlave"]             = eFiletypeCMDSlave;
    L["eFiletypeFLS"]                  = eFiletypeFLS;
    L["eFiletypeNewGenius"]            = eFiletypeNewGenius;
    L["eFiletypeODX"]                  = eFiletypeODX;
    L["eFiletypeCFF"]                  = eFiletypeCFF;
    L["eFiletypeCRE"]                  = eFiletypeCRE;
    L["eFiletypeBFlashSlave"]          = eFiletypeBFlashSlave;
    L["eFiletypeSMRF"]                 = eFiletypeSMRF;
    L["eFiletypeAutotunerSlave"]       = eFiletypeAutotunerSlave;
    L["eFiletypeAutoflasherSlave"]     = eFiletypeAutoflasherSlave;
    L["eFiletypeMagicmotorsportSlave"] = eFiletypeMagicmotorsportSlave;
    L["eFiletypeIni"]                  = eFiletypeIni;

    // ── BdmToGo flags ──
    L["eBdmToGoProgramEprom"]     = 0x01;
    L["eBdmToGoProgramEprom2"]    = 0x02;
    L["eBdmToGoProgramEEprom"]    = 0x04;
    L["eBdmToGoProgramProcessor"] = 0x08;
    L["eBdmToGoNoReimport"]       = 0x10;

    // ── Export flags ──
    L["eExportRemoveChecksums"]   = 1;
    L["eExportActiveElementOnly"] = 2;

    // ── Import options ──
    L["eOptionCmdCutFF"]              = 1;
    L["eOptionCmdIgnore"]             = 2;
    L["eOptionWinOLSSkriptTestOnly"]  = 4;

    // ── Project property IDs ──
    L["ePrjPropClientName"]                  = ePrjPropClientName;
    L["ePrjPropClientNumber"]                = ePrjPropClientNumber;
    L["ePrjPropClientLicenceplace"]          = ePrjPropClientLicenceplace;
    L["ePrjPropVehicleType"]                 = ePrjPropVehicleType;
    L["ePrjPropVehicleProducer"]             = ePrjPropVehicleProducer;
    L["ePrjPropVehicleChassis"]              = ePrjPropVehicleChassis;
    L["ePrjVehicleBuild"]                    = ePrjVehicleBuild;
    L["ePrjPropVehicleModel"]                = ePrjPropVehicleModel;
    L["ePrjVehicleCharacteristic"]           = ePrjVehicleCharacteristic;
    L["ePrjPropVehicleModelyear"]            = ePrjPropVehicleModelyear;
    L["ePrjPropVehicleVIN"]                  = ePrjPropVehicleVIN;
    L["ePrjPropEcuProducer"]                 = ePrjPropEcuProducer;
    L["ePrjPropEcuBuild"]                    = ePrjPropEcuBuild;
    L["ePrjPropEcuProdNr"]                   = ePrjPropEcuProdNr;
    L["ePrjPropEcuStgNr"]                    = ePrjPropEcuStgNr;
    L["ePrjPropEcuSoftwareversion"]          = ePrjPropEcuSoftwareversion;
    L["ePrjPropEcuSoftwareversionVersion"]   = ePrjPropEcuSoftwareversionVersion;
    L["ePrjPropEcuChecksum"]                 = ePrjPropEcuChecksum;
    L["ePrjPropEcuSoftwaresize"]             = ePrjPropEcuSoftwaresize;
    L["ePrjPropEcuUse"]                      = ePrjPropEcuUse;
    L["ePrjPropEngineName"]                  = ePrjPropEngineName;
    L["ePrjPropEngineType"]                  = ePrjPropEngineType;
    L["ePrjPropEngineDisplacement"]          = ePrjPropEngineDisplacement;
    L["ePrjPropEngineTransmission"]          = ePrjPropEngineTransmission;
    L["ePrjPropEngineOutputPS"]              = ePrjPropEngineOutputPS;
    L["ePrjPropEngineOutputKW"]              = ePrjPropEngineOutputKW;
    L["ePrjPropEngineEmissionStd"]           = ePrjPropEngineEmissionStd;
    L["ePrjPropEngineTorque"]                = ePrjPropEngineTorque;
    L["ePrjPropCommunicationsReadhardware"]  = ePrjPropCommunicationsReadhardware;
    L["ePrjPropProjectType"]                 = ePrjPropProjectType;
    L["ePrjProjectStatus"]                   = ePrjProjectStatus;
    L["ePrjFileCreatedBy"]                   = ePrjFileCreatedBy;
    L["ePrjFileModifiedBy"]                  = ePrjFileModifiedBy;
    L["ePrjFileCreatedOn"]                   = ePrjFileCreatedOn;
    L["ePrjFileModifiedOn"]                  = ePrjFileModifiedOn;
    L["ePrjComment"]                         = ePrjComment;
    L["ePrjPropNoreadTag"]                   = ePrjPropNoreadTag;
    L["ePrjPropSpiTag"]                      = ePrjPropSpiTag;
    L["ePrjPropBdmTag"]                      = ePrjPropBdmTag;
    L["ePrjPropUserTag"]                     = ePrjPropUserTag;
    L["ePrjPropUserTagText"]                 = ePrjPropUserTagText;
    L["ePrjPropResellerCredits"]             = ePrjPropResellerCredits;
    L["ePrjPropResellerProjectType"]         = ePrjPropResellerProjectType;
    L["ePrjPropResellerProjectDetails"]      = ePrjPropResellerProjectDetails;
    L["ePrjUserdef1"]                        = ePrjUserdef1;
    L["ePrjUserdef2"]                        = ePrjUserdef2;
    L["ePrjUserdef3"]                        = ePrjUserdef3;
    L["ePrjUserdef4"]                        = ePrjUserdef4;
    L["ePrjUserdef5"]                        = ePrjUserdef5;
    L["ePrjImportFilename"]                  = ePrjImportFilename;
    L["ePrjImportPath"]                      = ePrjImportPath;
    L["ePrjFilename"]                        = ePrjFilename;
    L["ePrjPropChecksum8Bit"]                = ePrjPropChecksum8Bit;
    L["ePrjPropChecksum8BitCpu"]             = ePrjPropChecksum8BitCpu;
    L["ePrjPropChecksum8BitEpr"]             = ePrjPropChecksum8BitEpr;
    L["ePrjPropChecksumMD5"]                 = ePrjPropChecksumMD5;
    L["ePrjPropChecksumMD5Cpu"]              = ePrjPropChecksumMD5Cpu;
    L["ePrjPropChecksumMD5Epr"]              = ePrjPropChecksumMD5Epr;
    L["ePrjPropChecksumSHA1"]                = ePrjPropChecksumSHA1;
    L["ePrjPropChecksumSHA256"]              = ePrjPropChecksumSHA256;

    // ── Version property IDs ──
    L["eVerPropName"]      = eVerPropName;
    L["eVerPropComment"]   = eVerPropComment;
    L["eVerPropCreatedOn"] = eVerPropCreatedOn;
    L["eVerPropChangedOn"] = eVerPropChangedOn;
    L["eVerPropChecksum"]  = eVerPropChecksum;
    L["eVerPropCVN"]       = eVerPropCVN;
    L["eVerPropOutput"]    = eVerPropOutput;
    L["eVerPropTorque"]    = eVerPropTorque;
    L["eVerPropState"]     = eVerPropState;
    L["eVerPropCredits"]   = eVerPropCredits;

    // ── Version status values ──
    L["eVerStatNone"]              = eVerStatNone;
    L["eVerStatExperiment"]        = eVerStatExperiment;
    L["eVerStatToDo"]              = eVerStatToDo;
    L["eVerStatDev"]               = eVerStatDev;
    L["eVerStatTestable"]          = eVerStatTestable;
    L["eVerStatErrors"]            = eVerStatErrors;
    L["eVerStatFinished"]          = eVerStatFinished;
    L["eVerStatMaster"]            = eVerStatMaster;
    L["eVerAutoExport"]            = eVerAutoExport;
    L["eVerAutoImport"]            = eVerAutoImport;
    L["eVerAutoUpdate"]            = eVerAutoUpdate;
    L["eVerAutoUpdateAndExport"]   = eVerAutoUpdateAndExport;

    // ── Checksum option types ──
    L["eCOTNone"]                  = 0;
    L["eCOTCheckbox"]              = 1;
    L["eCOTText"]                  = 2;
    L["eCOTCheckboxSearchAgain"]   = 3;

    // ── EChkInfo ──
    L["eChecksumName"]             = 0;
    L["eChecksumNumber"]           = 1;
    L["eChecksumVersion"]          = 2;

    // ── Import flags ──
    L["eImportSkipEEprom"]                  = 0x01;
    L["eImportOnlyDataArea"]                = 0x02;
    L["eImportSkipMaps"]                    = 0x04;
    L["eImportMapsRelative"]                = 0x08;
    L["eImportMapsPercent"]                 = 0x10;
    L["eImportSkipInsideMaps"]              = 0x20;
    L["eImportSkipOutsideMaps"]             = 0x40;
    L["eImportOnlyChanged"]                 = 0x80;
    L["eImportUseElementInformation"]       = 0x100;
    L["eImportDontUseElementInformation"]   = 0x200;

    // ── ImportChanges flags ──
    L["eICImportMapStructure"] = 0x01;
    L["eICImportMapContents"]  = 0x02;
    L["eICImportDataAreas"]    = 0x04;
    L["eICOnlyChangedMaps"]    = 0x08;
    L["eICCompareById"]        = 0x10;
    L["eICCompareByName"]      = 0x20;
    L["eICAllowReturn4"]       = 0x40;
    L["eICRemoveDuplicates"]   = 0x80;
    L["eICTMAbsolute"]         = 0;
    L["eICTMRelative"]         = 1;
    L["eICTMPercent"]          = 2;
    L["eICTMAllFromOrg"]       = 3;
    L["eICTMAllFromVer"]       = 4;
    L["eAutmAbsolute"]         = 0;   // stale alias for eICTMAbsolute

    // ── projectImportFromOls flags (Iter 9.6) ──
    //
    // Manual documents flags but doesn't pin exact bit values; we expose
    // a stable set of names that scripts can `binaryor` together.  Unknown
    // bits are simply ignored.
    L["eIFOlsSkipMaps"]            = 0x01;
    L["eIFOlsSkipData"]            = 0x02;
    L["eIFOlsPreserveByteOrder"]   = 0x04;
    L["eIFOlsPreserveBaseAddress"] = 0x08;

    // ── FindSimilarProjectsSql flags ──
    //
    // Iter 10.8: moved to high bits so they cannot collide with property IDs
    // like ePrjPropClientName=1 / ePrjPropClientNumber=2 in the trailing
    // column list of projectFindSimilarProjectsSql.  Scripts referencing
    // the named constants are unaffected; scripts that hard-coded `1`/`2`
    // as flags (against the manual) will need to switch to the names.
    L["eFSPAllowPropertyMatches"] = 0x10000;
    L["eFSPTrippleRelevance"]     = 0x20000;

    // ── CloneVehicleData flags ──
    L["eClonePropCompareSoftware"]              = 0x001;
    L["eClonePropCompareSoftwareVersion"]       = 0x002;
    L["eClonePropCompareEngineDescription"]     = 0x004;
    L["eClonePropCompareECUProd"]               = 0x008;
    L["eClonePropVehicle"]                      = 0x010;
    L["eClonePropUserdef"]                      = 0x020;
    L["eClonePropECU"]                          = 0x040;
    L["eClonePropEngine"]                       = 0x080;
    L["eClonePropAcceptDissent"]                = 0x100;
    L["eClonePropSimpleMajority"]               = 0x200;
    L["eClonePropAllowOverwrite"]               = 0x400;

    // ── Project rights ──
    L["ePRExportBinary"]         = 0x0001;
    L["ePRExportOLS"]            = 0x0002;
    L["ePRExportBdm"]            = 0x0004;
    L["ePRExportKp"]             = 0x0008;
    L["ePRExportClipboard"]      = 0x0010;
    L["ePRExportOther"]          = 0x0020;
    L["ePRWriteEprom"]           = 0x0040;
    L["ePRWriteBdm"]             = 0x0080;
    L["ePROtherTransMapContent"] = 0x0100;
    L["ePROtherTransMapProp"]    = 0x0200;
    L["ePRAccessHexdump"]        = 0x0400;
    L["ePRAccessMaps"]           = 0x0800;
    L["ePRAccessMapList"]        = 0x1000;

    // ── Map create helpers (referenced by sample 08) ──
    L["eZweidim"]    = 2;    // MAP    (see mapTypeFromWinOlsEnum)
    L["eEindim"]     = 1;    // CURVE
    L["eAchse"]      = 3;    // AXIS
    L["eViewText"]   = 1;
    L["eViewHex"]    = 3;
    L["eBars"]       = 2;
    L["eRom"]        = 1;

    // ── ePrjFilename overload — also keep symbolic alias ──
    L["ePrjFilename"] = ePrjFilename;
}

} // namespace lua
