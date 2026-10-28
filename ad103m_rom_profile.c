/* SPDX-License-Identifier: GPL-2.0-only */
#include "ad103m_rom_profile.h"
#include "sha256.h"

BOOLEAN GuAd103mOptionRomMatches(const VOID *Data, UINTN Bytes) {
  return Data != NULL && Bytes == GU_AD103M_OPTION_ROM_BYTES &&
    GuHashMatches(Data, Bytes,
      "3c07b334329aa17284137cd00c7613d566ab1de78e3fceca2df9fdbbb978a58e");
}

BOOLEAN GuAd103mFwsecMatches(const struct gpu_unlock_fwsec *f) {
  return f != NULL && f->image_off == 0x45e48 && f->image_size == 0x10500 &&
    f->imem_pa == 0 && f->imem_size == 0xf700 && f->imem_va == 0 &&
    f->dmem_pa == 0 && f->data_off == 0xf700 && f->dmem_size == 0xd80 &&
    f->pkc_off == 0xb24 && f->interface_off == 0x1c && f->ucode_id == 9 &&
    f->engine_mask == 0x400 && f->sig_count == 2 && f->sig_versions == 3 &&
    f->sig_off == 0x45b48 && f->sig_bytes == 768;
}
