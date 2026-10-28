/* SPDX-License-Identifier: GPL-2.0-only */
#include "profile.h"
#include "ad103m_rom_profile.h"
#include "checks.h"
#include "sha256.h"
#include "compat/compat.h"

static const struct {const char *name,*hash;UINTN bytes;} Pins[5]={
 {"gpu-unlock/610.43.03/booter_load_ad102.bin","6ee05b20e1dc244c08c052f36ef28469b8d09f4955d1fe33e48a179e11a7fd49",57672},
 {"gpu-unlock/610.43.03/v67-ad102-610.43.03-four-plm.bin","44c36b2d6b3424e53e18f3e55db85c833655de071ce66ce42749588a4a7dc8cb",64000},
 {"gpu-unlock/610.43.03/gsp_ga10x.bin","572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a",84277400},
 {"gpu-unlock/610.43.03/gsp_boot_ad102.bin","45d6335a8185d6ff13e8d1484cea73f68d3d13aa1c19dc080ed1c3482dd5bac0",36864},
 {"gpu-unlock/610.43.03/gsp_boot_desc_ad102.bin","83f8f3f1f5f9496e0ea8fc7103c2beb64d0b6de2f5ffe725f13514de5a95fa42",84}
};
static const VOID *PinnedOptionRom;
static UINTN PinnedOptionRomBytes;

BOOLEAN GuAd103mAssetsValid(VOID) {
  for(UINTN i=0;i<5;i++)
    if(!GuAssets[i].name || strcmp(GuAssets[i].name,Pins[i].name) ||
       GuAssets[i].firmware.size!=Pins[i].bytes ||
       !GuHashMatches(GuAssets[i].firmware.data,Pins[i].bytes,Pins[i].hash))return FALSE;
  return TRUE;
}

static BOOLEAN Config(VOID) {
  UINT32 c[64];UINTN s,b,d,f;GU_CAPS caps;
  if(!Gu.pci || !Gu.pci->GetLocation || !Gu.pci->Pci.Read ||
     Gu.bar_base!=GU_BAR0_BASE || Gu.bar_size!=0x1000000 ||
     EFI_ERROR(Gu.pci->GetLocation(Gu.pci,&s,&b,&d,&f)))return FALSE;
  for(UINTN i=0;i<64;i++)if(EFI_ERROR(GuConfigRead(EfiPciIoWidthUint32,(UINT32)i*4,&c[i])))return FALSE;
  BOOLEAN checked=GuCheckConfig(c,s,b,d,f,&caps);
  if(!checked || c[2]!=0x030000a1 ||
     c[4]!=GU_BAR0_BASE || c[5]!=0xc || c[6]!=0x60 || c[7]!=0xc ||
     c[8]!=0x64 || c[9]!=0x4001) {
    GuLog("AD103M config mismatch checked=%u header=%#x bars=%#x/%#x/%#x/%#x/%#x/%#x\n",
      checked,c[2],c[4],c[5],c[6],c[7],c[8],c[9]);return FALSE;
  }
  BOOLEAN same=caps.pm==Gu.pm_cap && caps.pcie==Gu.pcie_cap &&
    caps.pcie_version==Gu.pcie_version && caps.msi==Gu.msi_cap && caps.msix==Gu.msix_cap;
  if(!same)GuLog("AD103M cap mismatch pm=%#x/%#x pcie=%#x/%#x v=%u/%u msi=%#x/%#x msix=%#x/%#x\n",
    caps.pm,Gu.pm_cap,caps.pcie,Gu.pcie_cap,caps.pcie_version,Gu.pcie_version,
    caps.msi,Gu.msi_cap,caps.msix,Gu.msix_cap);
  return same;
}

static BOOLEAN Registers(UINT32 *Ss0,UINT32 *Ss1) {
  /* Only immutable identity and transaction-safety state belong in cold
   * admission. The firmware-selected stock speed selectors and unrelated
   * status registers legitimately vary between boots on this fixed laptop.
   */
  static const UINT32 offsets[]={0,0x118128,0x118234,0x110100,0x1fa828,
    0x823804,0x1183a4};
  static const UINT32 values[]={0x193000a1,0x8b8f,0x3ff,0x10,0,
    0xffffff8f,0x3ff8};
  for(UINTN i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
    UINT32 actual=GuRead32(offsets[i]);
    if(actual!=values[i] || Gu.fault) {
      GuLog("AD103M cold register mismatch off=%#x actual=%#x expected=%#x fault=%u\n",
        offsets[i],actual,values[i],Gu.fault);
      return FALSE;
    }
  }
  UINT32 readout=GuRead32(0x823818);
  if(Gu.fault || (readout&0xffff0000U)==0xbadf0000U) {
    GuLog("AD103M cold readout inaccessible value=%#x fault=%u\n",readout,Gu.fault);
    return FALSE;
  }
  *Ss0=GuRead32(0x82381c);*Ss1=GuRead32(0x823820);
  if(Gu.fault || *Ss0==0xffffffffU || *Ss0==0x88888888U ||
     (*Ss0&0xffff0000U)==0xbadf0000U || *Ss1!=1U) {
    GuLog("AD103M cold selector invalid ss0=%#x ss1=%#x fault=%u\n",
      *Ss0,*Ss1,Gu.fault);
    return FALSE;
  }
  return TRUE;
}

BOOLEAN GuAd103mColdEntry(VOID) {
  UINT32 ss0,ss1;
  return !Gu.started && !Gu.fault && !Gu.retained && Config() &&
    Registers(&ss0,&ss1) && GuEntrySelectorsMatch(ss0,ss1);
}

BOOLEAN GuAd103mAlreadyUnlocked(VOID) {
  if(!Gu.prom_verified||Gu.started||Gu.fault||Gu.retained||Gu.allocations||!Config())return FALSE;
  UINT32 ss0=GuRead32(0x82381c),ss1=GuRead32(0x823820);
  BOOLEAN unlocked=!Gu.fault&&ss0==0x88888888U&&ss1==8U;
  if(unlocked)GuLog("AD103M already unlocked at entry: ss0=%#x ss1=%#x; transaction bypassed\n",ss0,ss1);
  return unlocked;
}

EFI_STATUS GuAd103mPrepareStatic(const VOID *OptionRom,UINTN OptionRomBytes) {
  struct gpu_unlock_fwsec fw;
  if(Gu.started || Gu.fault || Gu.retained || Gu.allocations || Gu.profile_verified ||
     Gu.prom_verified){GuLog("AD103M static admission refused: dirty runtime state\n");return EFI_NOT_READY;}
  if(!GuAd103mAssetsValid()){GuLog("AD103M admission refused: assets\n");return EFI_CRC_ERROR;}
  if(!Gu.pci->RomImage || Gu.pci->RomSize!=151040 ||
     !GuHashMatches(Gu.pci->RomImage,151040,"857bc5cdb293844c604afa8f6971aa2b18079f78f5ade188d387557b79437632")) {
    GuLog("AD103M admission refused: firmware PCI ROM image\n");return EFI_CRC_ERROR;
  }
  if(!GuAd103mOptionRomMatches(OptionRom,OptionRomBytes) ||
     gpu_unlock_vbios_fwsec(OptionRom,OptionRomBytes,&fw) || !GuAd103mFwsecMatches(&fw)) {
    GuLog("AD103M admission refused: complete ROM/FWSEC\n");return EFI_COMPROMISED_DATA;
  }
  PinnedOptionRom=OptionRom;PinnedOptionRomBytes=OptionRomBytes;
  Gu.prom_verified=TRUE;return EFI_SUCCESS;
}

EFI_STATUS GuAd103mPrepareReleased(VOID) {
  UINT32 ss0,ss1,after0,after1;
  if(!Gu.prom_verified || Gu.started || Gu.fault || Gu.retained ||
     Gu.allocations || Gu.profile_verified){GuLog("AD103M released admission refused: dirty runtime state\n");return EFI_NOT_READY;}
  if(!Config()){GuLog("AD103M admission refused: released PCI config\n");return EFI_NOT_READY;}
  if(!Registers(&ss0,&ss1)){GuLog("AD103M admission refused: released cold registers\n");return EFI_NOT_READY;}
  if(!Config() || !Registers(&after0,&after1) || ss0!=after0 || ss1!=after1) {
    GuLog("AD103M admission refused: state changed during read-only checks\n");return EFI_NOT_READY;
  }
  if(!GuLatchEntrySelectors(ss0,ss1))return EFI_NOT_READY;
  Gu.profile_verified=TRUE;return EFI_SUCCESS;
}

EFI_STATUS GuAd103mPrepare(const VOID *OptionRom,UINTN OptionRomBytes) {
  EFI_STATUS s=GuAd103mPrepareStatic(OptionRom,OptionRomBytes);
  return s==EFI_SUCCESS?GuAd103mPrepareReleased():s;
}
BOOLEAN GuAd103mStaticPrepared(VOID){return Gu.prom_verified&&Gu.profile_verified&&PinnedOptionRom;}
BOOLEAN GuAd103mOptionRom(const VOID **Data,UINTN *Bytes) {
  if(!Data||!Bytes||!Gu.prom_verified||!Gu.profile_verified||!PinnedOptionRom)return FALSE;
  *Data=PinnedOptionRom;*Bytes=PinnedOptionRomBytes;return TRUE;
}
