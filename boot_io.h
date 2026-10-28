/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "port.h"
EFI_STATUS GuBootOpen(EFI_HANDLE,EFI_SYSTEM_TABLE *);
VOID GuBootBanner(VOID);
VOID GuBootResult(BOOLEAN);
#define GU_BOOT_DEFAULT_NORMAL 0U
#define GU_BOOT_DEFAULT_UNLOCK 1U
UINT8 GuBootReadDefault(VOID);
EFI_STATUS GuBootLoadAssets(VOID);
EFI_STATUS GuBootLoadOptionRom(VOID **Data,UINTN *Bytes);
EFI_STATUS GuBootLoadWindows(VOID);
EFI_STATUS GuBootChoose(BOOLEAN *,BOOLEAN);
EFI_STATUS GuBootWindows(VOID);
EFI_STATUS GuBootClose(VOID);
VOID GuBootSay(const CHAR16 *);
VOID GuBootSilence(VOID);
VOID GuBootHalt(VOID);
/* v0.5.4: diagnostics log only while the USB log is open (GuLog faults otherwise). */
BOOLEAN GuBootLogOpen(VOID);
