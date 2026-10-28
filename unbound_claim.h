/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "port.h"
EFI_STATUS GuUnboundClaim(EFI_SYSTEM_TABLE *,EFI_HANDLE,EFI_HANDLE);
EFI_STATUS GuUnboundFinish(BOOLEAN RestoreColdDecode);
BOOLEAN GuUnboundReadyForTransaction(VOID);
BOOLEAN GuUnboundReadyForWindows(VOID);
BOOLEAN GuUnboundMutated(VOID);
