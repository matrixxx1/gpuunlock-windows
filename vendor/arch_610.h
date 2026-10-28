/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GPU_UNLOCK_ARCH_610_H
#define GPU_UNLOCK_ARCH_610_H

enum gpu_unlock_booter_arch {
	GU_BOOTER_GA10X = 0,
	GU_BOOTER_AD10X = 1,
};

/* Exact experimental Ada IDs admitted by their machine-specific callers. */
#define GU_AD103_RTX_4070_TI_SUPER 0x2705u
#define GU_AD103M_RTX_4090_MOBILE 0x2757u

#endif
