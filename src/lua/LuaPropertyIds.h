/*
 * This file is part of romHEX14.
 * Copyright (C) 2026 Cristian Tabuyo <contact@romhex14.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * WinOLS-compatible numeric IDs for ePrjProp* / eVerProp* / element / set-mode /
 * datatype / file-type enums.  Exposed as Lua globals by LuaConstants.cpp AND
 * consumed by Project::propertyById / setPropertyById in src/project.cpp so
 * both sides agree on what `ePrjPropVehicleProducer` numerically equals.
 *
 * Numeric values are assigned by us (WinOLS-internal values are not publicly
 * documented) — they only need to be stable within our build.  Grouped in
 * blocks of 10 so future additions don't disturb existing ones.
 */

#pragma once

namespace lua_ids {

// ── Client ────────────────────────────────────────────────────────────────
constexpr int ePrjPropClientName            = 1;
constexpr int ePrjPropClientNumber          = 2;
constexpr int ePrjPropClientLicenceplace    = 3;

// ── Vehicle ───────────────────────────────────────────────────────────────
constexpr int ePrjPropVehicleType           = 10;
constexpr int ePrjPropVehicleProducer       = 11;
constexpr int ePrjPropVehicleChassis        = 12;
constexpr int ePrjVehicleBuild              = 13;
constexpr int ePrjPropVehicleModel          = 14;
constexpr int ePrjVehicleCharacteristic     = 15;
constexpr int ePrjPropVehicleModelyear      = 16;
constexpr int ePrjPropVehicleVIN            = 17;

// ── ECU ───────────────────────────────────────────────────────────────────
constexpr int ePrjPropEcuProducer           = 20;
constexpr int ePrjPropEcuBuild              = 21;
constexpr int ePrjPropEcuProdNr             = 22;
constexpr int ePrjPropEcuStgNr              = 23;
constexpr int ePrjPropEcuSoftwareversion    = 24;
constexpr int ePrjPropEcuSoftwareversionVersion = 25;
constexpr int ePrjPropEcuChecksum           = 26;
constexpr int ePrjPropEcuSoftwaresize       = 27;
constexpr int ePrjPropEcuUse                = 28;

// ── Engine ────────────────────────────────────────────────────────────────
constexpr int ePrjPropEngineName            = 30;
constexpr int ePrjPropEngineType            = 31;
constexpr int ePrjPropEngineDisplacement    = 32;
constexpr int ePrjPropEngineTransmission    = 33;
constexpr int ePrjPropEngineOutputPS        = 34;
constexpr int ePrjPropEngineOutputKW        = 35;
constexpr int ePrjPropEngineEmissionStd     = 36;
constexpr int ePrjPropEngineTorque          = 37;

// ── Comms / status ────────────────────────────────────────────────────────
constexpr int ePrjPropCommunicationsReadhardware = 40;
constexpr int ePrjPropProjectType           = 41;
constexpr int ePrjProjectStatus             = 42;

// ── File metadata ─────────────────────────────────────────────────────────
constexpr int ePrjFileCreatedBy             = 50;
constexpr int ePrjFileModifiedBy            = 51;
constexpr int ePrjFileCreatedOn             = 52;
constexpr int ePrjFileModifiedOn            = 53;
constexpr int ePrjComment                   = 54;

// ── Tags (STUB-MISSING in romHEX14) ───────────────────────────────────────
constexpr int ePrjPropNoreadTag             = 60;
constexpr int ePrjPropSpiTag                = 61;
constexpr int ePrjPropBdmTag                = 62;
constexpr int ePrjPropUserTag               = 63;
constexpr int ePrjPropUserTagText           = 64;

// ── Reseller (STUB-MISSING) ───────────────────────────────────────────────
constexpr int ePrjPropResellerCredits       = 70;
constexpr int ePrjPropResellerProjectType   = 71;
constexpr int ePrjPropResellerProjectDetails = 72;

// ── Userdef ───────────────────────────────────────────────────────────────
constexpr int ePrjUserdef1                  = 80;
constexpr int ePrjUserdef2                  = 81;
constexpr int ePrjUserdef3                  = 82;
constexpr int ePrjUserdef4                  = 83;
constexpr int ePrjUserdef5                  = 84;

// ── Read-only file path info ──────────────────────────────────────────────
constexpr int ePrjImportFilename            = 90;
constexpr int ePrjImportPath                = 91;
constexpr int ePrjFilename                  = 92;

// ── Original-version checksums (iOrgVer=1, computed on demand) ────────────
constexpr int ePrjPropChecksum8Bit          = 100;
constexpr int ePrjPropChecksum8BitCpu       = 101;
constexpr int ePrjPropChecksum8BitEpr       = 102;
constexpr int ePrjPropChecksumMD5           = 103;
constexpr int ePrjPropChecksumMD5Cpu        = 104;
constexpr int ePrjPropChecksumMD5Epr        = 105;
constexpr int ePrjPropChecksumSHA1          = 106;
constexpr int ePrjPropChecksumSHA256        = 107;

// ── Project status values (return for ePrjProjectStatus) ──────────────────
constexpr int ePrjDeveloping                = 1;
constexpr int ePrjFinished                  = 2;
constexpr int ePrjMaster                    = 3;

// ── Version property IDs ──────────────────────────────────────────────────
constexpr int eVerPropName                  = 200;
constexpr int eVerPropComment               = 201;
constexpr int eVerPropCreatedOn             = 202;
constexpr int eVerPropChangedOn             = 203;
constexpr int eVerPropChecksum              = 204;
constexpr int eVerPropCVN                   = 205;
constexpr int eVerPropOutput                = 206;
constexpr int eVerPropTorque                = 207;
constexpr int eVerPropState                 = 208;
constexpr int eVerPropCredits               = 209;

// ── Version status values (eVerPropState return) ──────────────────────────
constexpr int eVerStatNone                  = 0;
constexpr int eVerStatExperiment            = 1;
constexpr int eVerStatToDo                  = 2;
constexpr int eVerStatDev                   = 3;
constexpr int eVerStatTestable              = 4;
constexpr int eVerStatErrors                = 5;
constexpr int eVerStatFinished              = 6;
constexpr int eVerStatMaster                = 7;
constexpr int eVerAutoExport                = 8;
constexpr int eVerAutoImport                = 9;
constexpr int eVerAutoUpdate                = 10;
constexpr int eVerAutoUpdateAndExport       = 11;

// ── Datatype enums (used by projectGetAt / SetAt / FindBytes) ─────────────
constexpr int eByte                         = 1;
constexpr int eLoHi                         = 2;
constexpr int eHiLo                         = 3;
constexpr int eLoHiLoHi                     = 4;
constexpr int eHiLoHiLo                     = 5;
constexpr int eFloatLoHi                    = 6;
constexpr int eFloatHiLo                    = 7;
constexpr int eDoubleLoHi                   = 8;
constexpr int eDoubleHiLo                   = 9;
constexpr int eBitLoHi                      = 10;
constexpr int eBitHiLo                      = 11;

// ── Set mode (projectSetAt 4th arg) ───────────────────────────────────────
constexpr int eSetAbsolute                  = 0;
constexpr int eSetRelative                  = 1;
constexpr int eSetPercent                   = 2;

// ── Element type (projectGet/SetElement) ──────────────────────────────────
constexpr int eElementNone                  = 0;
constexpr int eElementHeader                = 1;
constexpr int eElementProcessor             = 2;
constexpr int eElementEprom                 = 3;
constexpr int eElementEprom2                = 4;
constexpr int eElementEEprom                = 5;
constexpr int eElementConfiguration         = 6;

// ── File-type enums (projectExport / projectImport) ───────────────────────
constexpr int eFiletypeAuto                 = 0;
constexpr int eFiletypeBinary               = 1;
constexpr int eFiletypeWinOLS               = 2;
constexpr int eFiletypeWinOLSAll            = 3;
constexpr int eFiletypeIntelHex             = 4;
constexpr int eFiletypeMotorolaHex          = 5;
constexpr int eFiletypeEDX                  = 6;
constexpr int eFiletypeBdmToGo              = 7;
constexpr int eFiletypeWinOLSX              = 8;
constexpr int eFiletypeWinOLSAllX           = 9;
constexpr int eFiletypeVBF                  = 10;
constexpr int eFiletypeCMDSlave             = 11;
constexpr int eFiletypeFLS                  = 12;
constexpr int eFiletypeNewGenius            = 13;
constexpr int eFiletypeODX                  = 14;
constexpr int eFiletypeCFF                  = 15;
constexpr int eFiletypeCRE                  = 16;
constexpr int eFiletypeBFlashSlave          = 17;
constexpr int eFiletypeSMRF                 = 18;
constexpr int eFiletypeAutotunerSlave       = 19;
constexpr int eFiletypeAutoflasherSlave     = 20;
constexpr int eFiletypeMagicmotorsportSlave = 21;
constexpr int eFiletypeIni                  = 22;

// ── Set of file-types our impl handles natively (rest are STUB-FAIL) ──────
constexpr bool isUngatedFiletype(int t) {
    return t == eFiletypeBinary
        || t == eFiletypeAuto
        || t == eFiletypeIntelHex
        || t == eFiletypeMotorolaHex
        || t == eFiletypeWinOLS
        || t == eFiletypeWinOLSAll
        || t == eFiletypeWinOLSX
        || t == eFiletypeWinOLSAllX
        || t == eFiletypeBdmToGo
        || t == eFiletypeEDX;
}

// ── Misc constants ────────────────────────────────────────────────────────
constexpr int eEmptyvalue                   = -99999;
constexpr int ePluginMajor                  = 1;
constexpr int ePluginMinor                  = 0;

// ── Special — Windows error codes echoed from GetLastError ────────────────
constexpr int ERROR_READ_FAULT              = 30;

} // namespace lua_ids
