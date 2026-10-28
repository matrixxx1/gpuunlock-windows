/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "port.h"
#include "ad103m_rom_profile.h"
BOOLEAN GuAd103mAssetsValid(VOID);
EFI_STATUS GuAd103mPrepare(const VOID *OptionRom,UINTN OptionRomBytes);
EFI_STATUS GuAd103mPrepareStatic(const VOID *OptionRom,UINTN OptionRomBytes);
EFI_STATUS GuAd103mPrepareReleased(VOID);
BOOLEAN GuAd103mStaticPrepared(VOID);
BOOLEAN GuAd103mColdEntry(VOID);
BOOLEAN GuAd103mAlreadyUnlocked(VOID);
BOOLEAN GuAd103mOptionRom(const VOID **Data,UINTN *Bytes);
