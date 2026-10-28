/* SPDX-License-Identifier: GPL-2.0-only */
/* Keep the local ESP identifier out of the public source tree. */
#pragma once
#if defined(GU_HOST_TEST_PROFILE)
#define GU_WINDOWS_PARTITION_INITIALIZER {0x11111111,0x2222,0x3333,{0x84,0x44,0x55,0x66,0x77,0x88,0x99,0xaa}}
#define GU_INTERNAL_PARTITION_INITIALIZER {0xaaaaaaaa,0xbbbb,0xcccc,{0x8d,0xee,0xff,0x11,0x22,0x33,0x44,0x55}}
#else
#include "local/boot_profile.h"
#ifndef GU_WINDOWS_PARTITION_INITIALIZER
#error "Configure the reviewed ESP GUID in local/boot_profile.h before building."
#endif
#ifndef GU_INTERNAL_PARTITION_INITIALIZER
#error "Configure the dedicated internal ESP GUID in local/boot_profile.h before building."
#endif
#endif
