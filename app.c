/* SPDX-License-Identifier: GPL-2.0-only */
#include "port.h"
#include "checks.h"
#include "profile.h"
#include "asus_release.h"
#include "boot_io.h"
#include "compat/compat.h"
#include "vendor/unlock_610.h"
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID GlobalGuid={0x8be4df61,0x93ca,0x11d2,{0xaa,0x0d,0,0xe0,0x98,0x03,0x2b,0x8c}};

static EFI_STATUS Discover(EFI_HANDLE image,EFI_SYSTEM_TABLE *t) {
  EFI_HANDLE *h=NULL,target=NULL;EFI_PCI_IO_PROTOCOL *io=NULL;UINTN count=0,found=0;
  EFI_STATUS s=t->BootServices->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&count,&h);
  if(s==EFI_SUCCESS&&(!h||!count||count>1024))s=EFI_UNSUPPORTED;
  for(UINTN i=0;s==EFI_SUCCESS&&i<count;i++) {
    EFI_PCI_IO_PROTOCOL *p=NULL;UINT32 id=0;UINTN seg,bus,dev,fun;
    s=t->BootServices->HandleProtocol(h[i],&PciGuid,(VOID **)&p);
    if(s!=EFI_SUCCESS||!p||!p->Pci.Read||!p->GetLocation){s=EFI_UNSUPPORTED;break;}
    s=p->Pci.Read(p,EfiPciIoWidthUint32,0,1,&id);
    if(s==EFI_SUCCESS&&id==GU_BOARD_ID) {
      s=p->GetLocation(p,&seg,&bus,&dev,&fun);
      if(s!=EFI_SUCCESS||seg||bus!=1||dev||fun){s=EFI_UNSUPPORTED;break;}
      found++;target=h[i];io=p;
    }
  }
  if(h)t->BootServices->FreePool(h);
  if(s!=EFI_SUCCESS||found!=1||!io)return EFI_UNSUPPORTED;
  GuInit(t->BootServices,io);Gu.image=image;Gu.handle=target;Gu.bar_base=GU_BAR0_BASE;Gu.bar_size=0x1000000;
  UINT32 c[64];UINTN seg,bus,dev,fun;GU_CAPS caps;
  s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,c);
  if(s==EFI_SUCCESS)s=io->GetLocation(io,&seg,&bus,&dev,&fun);
  if(s!=EFI_SUCCESS||!GuCheckConfig(c,seg,bus,dev,fun,&caps)||c[2]!=0x030000a1||
     c[4]!=GU_BAR0_BASE||c[5]!=0xc||c[6]!=0x60||c[7]!=0xc||c[8]!=0x64||c[9]!=0x4001)return EFI_UNSUPPORTED;
  Gu.pm_cap=caps.pm;Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;
  Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE image,EFI_SYSTEM_TABLE *t) {
  if(!image||!t||!t->BootServices||!t->RuntimeServices||!t->RuntimeServices->GetVariable)return EFI_INVALID_PARAMETER;
  UINT8 secure=1;UINTN n=1;UINT32 attrs=0;
  EFI_STATUS s=t->RuntimeServices->GetVariable(L"SecureBoot",&GlobalGuid,&attrs,&n,&secure);
  if(s!=EFI_SUCCESS||n!=1||secure)return EFI_SECURITY_VIOLATION;
  s=GuBootOpen(image,t);if(s!=EFI_SUCCESS)return s;
  GuBootBanner();
  const char *stage="boot-default";UINT8 boot_default=GuBootReadDefault();BOOLEAN unlock=FALSE;VOID *rom=NULL;UINTN rom_bytes=0;
  if(Gu.fault)goto failed;
  stage="discover";
  s=Discover(image,t);if(s!=EFI_SUCCESS)goto failed;
  GuLog("ASUS G834JYR AD103M experimental v0.4.10: NVIDIA vendor PCI state restore with observed status-bit handoff; already-unlocked bypass\n");
  stage="assets";s=GuBootLoadAssets();if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  stage="option-ROM";s=GuBootLoadOptionRom(&rom,&rom_bytes);if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  stage="static-profile";s=GuAd103mPrepareStatic(rom,rom_bytes);if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  stage="stage-Windows";s=GuBootLoadWindows();if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  stage="manual-choice";s=GuBootChoose(&unlock,boot_default==GU_BOOT_DEFAULT_UNLOCK);if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  if(!unlock) {
    GuLog("Normal boot selected before GPU claim; starting stock Windows\n");
    GuFree(rom);rom=NULL;
    GuBootResult(FALSE);
    if(GuBootClose()!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
    return GuBootWindows();
  }
  if(GuAd103mAlreadyUnlocked()) {
    GuLog("Unlock requested but GPU selectors are already unlocked; starting Windows without GPU mutation\n");
    GuFree(rom);rom=NULL;GuBootResult(TRUE);
    if(GuBootClose()!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
    return GuBootWindows();
  }
  stage="cold-profile-before-release";s=GuAd103mPrepareReleased();if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  stage="ASUS-Ultimate-preflight";s=GuAsusPreflight(t,image,Gu.handle);if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  GuBootSay(L"\r\nExact ASUS display owners verified. Releasing display; the screen may go blank.\r\n");
  GuLog("Operator U: pinned Ultimate topology accepted; no retries; display callbacks will be disabled\n");
  GuBootSilence();
  stage="ASUS-Ultimate-release";s=GuAsusRelease();if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  stage="final-read-only-admission";
  if(!GuAsusReleased()||!GuAd103mColdEntry()){s=EFI_NOT_READY;goto failed;}
  Gu.display_check=GuAsusReleased;
  Gu.display_ok=TRUE;Gu.armed=TRUE;
  UINT64 mask=0xffffffffffffULL;
  struct pci_dev dev={.vendor=0x10de,.device=0x2757,
    .dev={.dma_mask=&mask,.coherent_dma_mask=0xfffffffffULL},.current_state=PCI_D0,.pm_cap=Gu.pm_cap,.refs=1};
  struct gpu_unlock_610_shared shared={0};
  stage="shared-assets";int result=gpu_unlock_610_load_shared(&dev,&shared);
  if(!result){stage="firmware-transaction";result=gpu_unlock_610_unlock(&dev,&shared);}
  gpu_unlock_610_release_shared(&shared);
  GuLog("Transaction result=%d safe=%u retained=%u started=%u quiesced=%u flrs=%u DMA_live=%u\n",
    result,GuSafeToReturn(),Gu.retained,Gu.started,Gu.quiesced,Gu.flrs,Gu.dma_count);
  if(result||Gu.fault||!GuSafeToReturn()){s=EFI_DEVICE_ERROR;goto failed;}
  stage="release-exclusive-claim";s=GuAsusFinish();if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  GuFree(rom);rom=NULL;
  GuLog("TRANSACTION APPLIED: volatile readbacks passed and DMA released; Windows benchmark verification remains required\n");
  GuBootResult(TRUE);
  stage="close-log";s=GuBootClose();if(s!=EFI_SUCCESS||Gu.fault)goto failed;
  stage="final-Windows-verification";if(!GuAsusReadyForWindows()){s=EFI_NOT_READY;goto failed;}
  stage="start-Windows";s=GuBootWindows();GuBootHalt();return s==EFI_SUCCESS?EFI_ABORTED:s;
failed:
  GuFree(rom);if(s==EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  GuLog("STOP: stage=%s status=%#llx mutated=%u retained=%u started=%u fault=%u\n",stage,
    (unsigned long long)s,GuAsusMutated(),Gu.retained,Gu.started,Gu.fault);
  if(GuAsusMutated()||Gu.started||Gu.retained) {
    if(Gu.started||Gu.master_attributes_pending)(VOID)GuClearMaster();
    (VOID)GuBootClose();GuBootHalt();return EFI_ABORTED;
  }
  GuBootResult(FALSE);
  (VOID)GuBootClose();return GuBootWindows();
}
