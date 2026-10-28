/* SPDX-License-Identifier: GPL-2.0-only */
#include "../ad103m_rom_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Fail(const char *Expression, int Line) {
  fprintf(stderr, "FAIL line %d: %s\n", Line, Expression);
  exit(1);
}
#define CHECK(Expression) ((Expression) ? (void)0 : Fail(#Expression, __LINE__))

static UINT8 *ReadFile(const char *Path, size_t *Bytes) {
  FILE *File = fopen(Path, "rb");
  long End;
  UINT8 *Data;
  CHECK(File != NULL);
  CHECK(fseek(File, 0, SEEK_END) == 0);
  End = ftell(File);
  CHECK(End > 0 && fseek(File, 0, SEEK_SET) == 0);
  Data = malloc((size_t)End + 1U);
  CHECK(Data != NULL);
  CHECK(fread(Data, 1, (size_t)End, File) == (size_t)End);
  CHECK(fgetc(File) == EOF);
  CHECK(fclose(File) == 0);
  *Bytes = (size_t)End;
  return Data;
}

int main(int argc, char **argv) {
  struct gpu_unlock_fwsec Fwsec, Mutated;
  size_t Bytes;
  UINT8 *Rom;
  CHECK(argc == 2);
  Rom = ReadFile(argv[1], &Bytes);
  CHECK(Bytes == GU_AD103M_OPTION_ROM_BYTES);
  CHECK(GuAd103mOptionRomMatches(Rom, Bytes));
  CHECK(!GuAd103mOptionRomMatches(NULL, Bytes));
  CHECK(!GuAd103mOptionRomMatches(Rom, Bytes - 1U));
  CHECK(!GuAd103mOptionRomMatches(Rom, Bytes + 1U));
  CHECK(gpu_unlock_vbios_fwsec(Rom, Bytes, &Fwsec) == 0);
  CHECK(GuAd103mFwsecMatches(&Fwsec));
  CHECK(!GuAd103mFwsecMatches(NULL));

#define REJECT_MUTATION(Field) do { Mutated = Fwsec; Mutated.Field++; \
  CHECK(!GuAd103mFwsecMatches(&Mutated)); } while (0)
  REJECT_MUTATION(image_off);
  REJECT_MUTATION(image_size);
  REJECT_MUTATION(imem_pa);
  REJECT_MUTATION(imem_size);
  REJECT_MUTATION(imem_va);
  REJECT_MUTATION(dmem_pa);
  REJECT_MUTATION(data_off);
  REJECT_MUTATION(dmem_size);
  REJECT_MUTATION(pkc_off);
  REJECT_MUTATION(interface_off);
  REJECT_MUTATION(ucode_id);
  REJECT_MUTATION(engine_mask);
  REJECT_MUTATION(sig_count);
  REJECT_MUTATION(sig_versions);
  REJECT_MUTATION(sig_off);
  REJECT_MUTATION(sig_bytes);
#undef REJECT_MUTATION

  Rom[0] ^= 1U;
  CHECK(!GuAd103mOptionRomMatches(Rom, Bytes));
  Rom[0] ^= 1U;
  CHECK(GuAd103mOptionRomMatches(Rom, Bytes));
  free(Rom);
  puts("AD103M ASUS G834JYR exact ROM profile and negative admission tests PASS");
  return 0;
}
