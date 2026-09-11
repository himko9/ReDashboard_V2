/*
 * This file is part of the "ReDashboard_V2" distribution.
 *
 * Copyright (C) 2026 @himko9 <me@himko.dev>
 * Github: https://github.com/himko9/ReDashboard_V2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// AMI UEFI POST code definitions and labels.
#include "p80_post_codes.h"

// Code that is defined
const char *p80_post_code_label(uint8_t code)
{
    switch (code) {
    case 0x00U: return "NO CHECKPOINT";
    case 0x01U: return "RESET TYPE";
    case 0x02U: return "CPU AP INIT";
    case 0x03U: return "SYSTEM AGENT INIT";
    case 0x04U: return "PCH INIT";
    case 0x05U: return "OEM SEC INIT";
    case 0x06U: return "CPU MICROCODE";
    case 0x07U: return "CPU AP INIT";
    case 0x08U: return "SYSTEM AGENT INIT";
    case 0x09U: return "PCH INIT";
    case 0x0AU: return "OEM SEC INIT";
    case 0x0BU: return "CPU CACHE INIT";
    case 0x0CU:
    case 0x0DU: return "RESERVED SEC";
    case 0x0EU: return "MICROCODE NOT FOUND";
    case 0x0FU: return "MICROCODE LOAD FAIL";

    case 0x10U: return "PEI CORE";
    case 0x11U: return "CPU PRE MEM INIT";
    case 0x12U:
    case 0x13U:
    case 0x14U: return "CPU PRE MEM STEP";
    case 0x15U: return "SA PRE MEM INIT";
    case 0x16U:
    case 0x17U:
    case 0x18U: return "SA PRE MEM STEP";
    case 0x19U: return "PCH PRE MEM INIT";
    case 0x1AU:
    case 0x1BU:
    case 0x1CU: return "PCH PRE MEM STEP";
    case 0x2BU: return "READ DIMM SPD";
    case 0x2CU: return "DETECT MEMORY";
    case 0x2DU: return "PROGRAM MEM TIMING";
    case 0x2EU: return "CONFIG MEMORY";
    case 0x2FU: return "MEMORY INIT";

    case 0x30U: return "ASL RESERVED";
    case 0x31U: return "MEMORY INSTALLED";
    case 0x32U: return "CPU POST MEM INIT";
    case 0x33U: return "CPU CACHE INIT";
    case 0x34U: return "CPU AP INIT";
    case 0x35U: return "SELECT BSP";
    case 0x36U: return "SMM INIT";
    case 0x37U: return "SA POST MEM INIT";
    case 0x38U:
    case 0x39U:
    case 0x3AU: return "SA POST MEM STEP";
    case 0x3BU: return "PCH POST MEM INIT";
    case 0x3CU:
    case 0x3DU:
    case 0x3EU: return "PCH POST MEM STEP";
    case 0x4FU: return "DXE HANDOFF";

    case 0x50U: return "MEM TYPE SPEED ERR";
    case 0x51U: return "DIMM SPD READ FAIL";
    case 0x52U: return "MEM SIZE MISMATCH";
    case 0x53U: return "NO USABLE MEMORY";
    case 0x54U: return "MEMORY INIT FAIL";
    case 0x55U: return "NO MEMORY INSTALLED";
    case 0x56U: return "UNSUPPORTED CPU";
    case 0x57U: return "CPU MISMATCH";
    case 0x58U: return "CPU SELF TEST FAIL";
    case 0x59U: return "CPU MICROCODE FAIL";
    case 0x5AU: return "CPU INTERNAL ERROR";
    case 0x5BU: return "RESET PPI MISSING";
    case 0x5CU: return "BMC SELF TEST FAIL";
    case 0x5DU:
    case 0x5EU:
    case 0x5FU: return "UNASSIGNED";

    case 0x60U: return "DXE CORE";
    case 0x61U: return "NVRAM INIT";
    case 0x62U: return "PCH RUNTIME INIT";
    case 0x63U: return "CPU DXE INIT";
    case 0x68U: return "PCI HOST INIT";
    case 0x69U: return "SA DXE INIT";
    case 0x6AU: return "SA SMM INIT";
    case 0x70U: return "PCH DXE INIT";
    case 0x71U: return "PCH SMM INIT";
    case 0x72U: return "PCH DEVICE INIT";
    case 0x78U: return "ACPI INIT";
    case 0x79U: return "CSM INIT";

    case 0x90U: return "BDS START";
    case 0x91U: return "CONNECT DEVICES";
    case 0x92U: return "PCI BUS INIT";
    case 0x93U: return "PCI HOTPLUG INIT";
    case 0x94U: return "PCI DEVICE ENUM";
    case 0x95U: return "PCI RESOURCE QUERY";
    case 0x96U: return "PCI RESOURCE ASSIGN";
    case 0x97U: return "CONSOLE OUTPUT";
    case 0x98U: return "CONSOLE INPUT";
    case 0x99U: return "SUPER IO INIT";
    case 0x9AU: return "USB INIT";
    case 0x9BU: return "USB RESET";
    case 0x9CU: return "USB DETECT";
    case 0x9DU: return "USB ENABLE";

    case 0xA0U: return "IDE INIT";
    case 0xA1U: return "IDE RESET";
    case 0xA2U: return "IDE DETECT";
    case 0xA3U: return "IDE ENABLE";
    case 0xA4U: return "SCSI INIT";
    case 0xA5U: return "SCSI RESET";
    case 0xA6U: return "SCSI DETECT";
    case 0xA7U: return "SCSI ENABLE";
    case 0xA8U: return "VERIFY PASSWORD";
    case 0xA9U: return "SETUP START";
    case 0xAAU: return "ACPI APIC MODE";
    case 0xABU: return "WAIT SETUP INPUT";
    case 0xACU: return "ACPI PIC MODE";
    case 0xADU: return "READY TO BOOT";
    case 0xAEU: return "LEGACY BOOT";
    case 0xAFU: return "EXIT BOOT SERVICES";
    case 0xB0U: return "VIRTUAL MAP START";
    case 0xB1U: return "VIRTUAL MAP DONE";
    case 0xB2U: return "OPTION ROM INIT";
    case 0xB3U: return "SYSTEM RESET";
    case 0xB4U: return "USB HOTPLUG";
    case 0xB5U: return "PCI HOTPLUG";
    case 0xB6U: return "NVRAM CLEANUP";
    case 0xB7U: return "RESET NVRAM CONFIG";

    case 0xD0U: return "CPU INIT FAIL";
    case 0xD1U: return "SA INIT FAIL";
    case 0xD2U: return "PCH INIT FAIL";
    case 0xD3U: return "ARCH PROTOCOL MISS";
    case 0xD4U: return "PCI RESOURCE FAIL";
    case 0xD5U: return "OPTION ROM SPACE ERR";
    case 0xD6U: return "NO CONSOLE OUTPUT";
    case 0xD7U: return "NO CONSOLE INPUT";
    case 0xD8U: return "INVALID PASSWORD";
    case 0xD9U: return "BOOT LOAD FAIL";
    case 0xDAU: return "BOOT START FAIL";
    case 0xDBU: return "FLASH UPDATE FAIL";
    case 0xDCU: return "RESET PROTOCOL MISS";
    case 0xDDU: return "BMC SELF TEST FAIL";
    case 0xDEU:
    case 0xDFU: return "UNASSIGNED";

    case 0xE0U: return "S3 RESUME START";
    case 0xE1U: return "S3 BOOT SCRIPT";
    case 0xE2U: return "S3 VIDEO REPOST";
    case 0xE3U: return "S3 OS WAKE VECTOR";
    case 0xE8U: return "S3 RESUME FAIL";
    case 0xE9U: return "S3 PPI MISSING";
    case 0xEAU: return "S3 SCRIPT FAIL";
    case 0xEBU: return "S3 WAKE FAIL";

    case 0xF0U: return "AUTO RECOVERY";
    case 0xF1U: return "FORCED RECOVERY";
    case 0xF2U: return "RECOVERY START";
    case 0xF3U: return "RECOVERY IMAGE FOUND";
    case 0xF4U: return "RECOVERY IMAGE LOAD";
    case 0xF8U: return "RECOVERY PPI MISS";
    case 0xF9U: return "NO RECOVERY CAPSULE";
    case 0xFAU: return "BAD RECOVERY CAPSULE";
    default:
        break;
    }

    // Generic code labels for ranges of codes that are defined but not explicitly listed above.
    if ((code >= 0x1DU) && (code <= 0x2AU)) {
        return "OEM PRE MEM";
    }
    if ((code >= 0x3FU) && (code <= 0x4EU)) {
        return "OEM POST MEM";
    }
    if ((code >= 0x64U) && (code <= 0x67U)) {
        return "CPU DXE STEP";
    }
    if ((code >= 0x6BU) && (code <= 0x6FU)) {
        return "SA DXE STEP";
    }
    if ((code >= 0x73U) && (code <= 0x77U)) {
        return "PCH DXE STEP";
    }
    if ((code >= 0x7AU) && (code <= 0x7FU)) {
        return "RESERVED DXE";
    }
    if ((code >= 0x80U) && (code <= 0x8FU)) {
        return "OEM DXE";
    }
    if ((code >= 0x9EU) && (code <= 0x9FU)) {
        return "RESERVED BDS";
    }
    if ((code >= 0xB8U) && (code <= 0xBFU)) {
        return "RESERVED BDS";
    }
    if ((code >= 0xC0U) && (code <= 0xCFU)) {
        return "OEM BDS";
    }
    if (((code >= 0xE4U) && (code <= 0xE7U)) ||
        ((code >= 0xECU) && (code <= 0xEFU))) {
        return "RESERVED S3";
    }
    return "RESERVED RECOVERY";
}
