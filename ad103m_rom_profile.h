/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "port.h"
#include "vendor/vbios_610.h"

#define GU_AD103M_OPTION_ROM_BYTES 666624U

/* Offline admission profile for the exact ASUS G834JYR/N149G834JYR.001 ROM.
 * This does not qualify cold MMIO state or make an EFI write-eligible. */
BOOLEAN GuAd103mOptionRomMatches(const VOID *Data, UINTN Bytes);
BOOLEAN GuAd103mFwsecMatches(const struct gpu_unlock_fwsec *Fwsec);
