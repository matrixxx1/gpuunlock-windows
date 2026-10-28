/* SPDX-License-Identifier: GPL-2.0-only */
#include "unbound_claim.h"
#include "checks.h"
#include "compat/compat.h"

typedef struct {EFI_GUID protocol;EFI_OPEN_PROTOCOL_INFORMATION_ENTRY entry;} EDGE;
static EFI_SYSTEM_TABLE *Table;
static EFI_HANDLE Self,Target;
static BOOLEAN Claimed,DecodeEnabled,Mutated;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID;
static const UINT32 Cold[64]={
  0x275710de,0x00100000,0x030000a1,0x00800010,0x86000000,0x0000000c,0x00000060,0x0000000c,
  0x00000064,0x00004001,0x00000000,0x34d81043,0x87000000,0x00000060,0x00000000,0x000001ff,
  0x34d81043,0,0,0,0,1,0x0023d6ce,0,0x48036801,8,0x00807805,0,0,0,0x0012b410,0x112c8de1,
  0x00092930,0x00457904,0x10840140,0,0,0,0,0x00070813,0x00000400,0x0180001e,0x001e0004,0,0,
  0x01140009,0x06000001,0x00015e00,0,0,0x00050011,0x00b90000,0x00ba0000,0,0,0,0,0,0,0,0,0,0,0
};

static BOOLEAN Passive(UINT32 a) {
  return a==EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL || a==EFI_OPEN_PROTOCOL_GET_PROTOCOL ||
    a==EFI_OPEN_PROTOCOL_TEST_PROTOCOL;
}
static EFI_STATUS SnapshotNoActive(VOID) {
  EFI_GUID **protocols=NULL;UINTN protocol_count=0,total=0;
  EFI_STATUS s=Table->BootServices->ProtocolsPerHandle(Target,&protocols,&protocol_count);
  if(EFI_ERROR(s)||!protocols||!protocol_count||protocol_count>32)return EFI_UNSUPPORTED;
  for(UINTN i=0;!EFI_ERROR(s)&&i<protocol_count;i++) {
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN count=0;
    s=Table->BootServices->OpenProtocolInformation(Target,protocols[i],&entries,&count);
    if(!EFI_ERROR(s)&&(count>64-total||(count&&!entries)))s=EFI_COMPROMISED_DATA;
    for(UINTN j=0;!EFI_ERROR(s)&&j<count;j++) {
      total++;
      GuLog("unbound edge protocol=%08x attributes=%#x count=%u\n",protocols[i]->Data1,
        entries[j].Attributes,entries[j].OpenCount);
      if(!Passive(entries[j].Attributes))s=EFI_ACCESS_DENIED;
    }
    if(entries)Table->BootServices->FreePool(entries);
  }
  if(protocols)Table->BootServices->FreePool(protocols);
  GuLog("unbound graph status=%#llx passive_edges=%zu\n",(unsigned long long)s,(size_t)total);
  return s;
}
static EFI_STATUS ReadConfig(UINT32 c[64]) {
  return Gu.pci->Pci.Read(Gu.pci,EfiPciIoWidthUint32,0,64,c);
}
static BOOLEAN Exact(const UINT32 c[64],BOOLEAN memory) {
  for(UINTN i=0;i<64;i++)if(c[i]!=(i==1&&memory?(Cold[i]|2U):Cold[i]))return FALSE;
  return TRUE;
}
static EFI_STATUS Bar(VOID) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=NULL;
  EFI_STATUS s=Gu.pci->GetBarAttributes(Gu.pci,0,NULL,(VOID **)&r);
  if(!EFI_ERROR(s)&&(!r||r->Desc!=ACPI_ADDRESS_SPACE_DESCRIPTOR||r->Len!=sizeof(*r)-3||
    r->ResType!=0||r->AddrRangeMin!=GU_BAR0_BASE||r->AddrLen!=0x1000000||
    r->AddrTranslationOffset||r->AddrSpaceGranularity!=32||
    (r->AddrRangeMax!=0x86ffffff&&r->AddrRangeMax!=0x00ffffff)))s=EFI_UNSUPPORTED;
  if(r)Table->BootServices->FreePool(r);return s;
}

EFI_STATUS GuUnboundClaim(EFI_SYSTEM_TABLE *t,EFI_HANDLE self,EFI_HANDLE target) {
  if(Table||!t||!t->BootServices||!self||!target||Gu.handle!=target||Gu.image!=self)return EFI_INVALID_PARAMETER;
  Table=t;Self=self;Target=target;EFI_BOOT_SERVICES *b=t->BootServices;
  if(!b->ProtocolsPerHandle||!b->OpenProtocolInformation||!b->OpenProtocol||!b->CloseProtocol||
     !b->RaiseTPL||!b->RestoreTPL||!Gu.pci->Attributes)return EFI_UNSUPPORTED;
  UINT32 c[64];EFI_STATUS s=ReadConfig(c);if(!EFI_ERROR(s)&&!Exact(c,FALSE))s=EFI_NOT_READY;
  if(!EFI_ERROR(s))s=SnapshotNoActive();
  EFI_PCI_IO_PROTOCOL *io=NULL;EFI_TPL old=b->RaiseTPL(TPL_NOTIFY);
  if(!EFI_ERROR(s))s=b->OpenProtocol(target,&PciGuid,(VOID **)&io,self,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE);
  b->RestoreTPL(old);
  if(!EFI_ERROR(s)){Claimed=TRUE;Gu.owned=TRUE;if(io!=Gu.pci)s=EFI_COMPROMISED_DATA;}
  UINT64 supported=0,attrs=0;
  if(!EFI_ERROR(s))s=io->Attributes(io,EfiPciIoAttributeOperationSupported,0,&supported);
  if(!EFI_ERROR(s)&&(supported&EFI_PCI_IO_ATTRIBUTE_MEMORY)==0)s=EFI_UNSUPPORTED;
  if(!EFI_ERROR(s))s=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&attrs);
  if(!EFI_ERROR(s)&&(attrs&(EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER)))s=EFI_NOT_READY;
  if(!EFI_ERROR(s)){
    /* Track the possible effect before the firmware call: an error may be
       returned after memory decoding was partially enabled. */
    Mutated=TRUE;DecodeEnabled=TRUE;
    s=io->Attributes(io,EfiPciIoAttributeOperationEnable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
  }
  if(!EFI_ERROR(s))s=ReadConfig(c);
  if(!EFI_ERROR(s)&&!Exact(c,TRUE))s=EFI_COMPROMISED_DATA;
  GU_CAPS caps;
  if(!EFI_ERROR(s)&&!GuCheckConfig(c,0,1,0,0,&caps))s=EFI_UNSUPPORTED;
  if(!EFI_ERROR(s))s=Bar();
  if(!EFI_ERROR(s)){Gu.pm_cap=caps.pm;Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;
    Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;Gu.bar_base=GU_BAR0_BASE;Gu.bar_size=0x1000000;}
  GuLog("unbound exclusive claim status=%#llx decode=%u BME=0\n",(unsigned long long)s,DecodeEnabled);
  return s;
}

EFI_STATUS GuUnboundFinish(BOOLEAN restore) {
  if(!Table||!Claimed||(Gu.started&&!GuSafeToReturn()))return EFI_NOT_READY;
  EFI_STATUS s=EFI_SUCCESS;
  if(restore&&DecodeEnabled) {
    s=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationDisable,EFI_PCI_IO_ATTRIBUTE_MEMORY,NULL);
    UINT32 c[64];if(!EFI_ERROR(s))s=ReadConfig(c);if(!EFI_ERROR(s)&&!Exact(c,FALSE))s=EFI_COMPROMISED_DATA;
    if(!EFI_ERROR(s))DecodeEnabled=FALSE;
  }
  if(!EFI_ERROR(s))s=Table->BootServices->CloseProtocol(Target,&PciGuid,Self,NULL);
  if(!EFI_ERROR(s)){Claimed=FALSE;Gu.owned=FALSE;}
  return s;
}
BOOLEAN GuUnboundReadyForTransaction(VOID) {
  UINT32 c[64];return Claimed&&DecodeEnabled&&Gu.owned&&!Gu.fault&&Gu.profile_verified&&
    Gu.prom_verified&&ReadConfig(c)==EFI_SUCCESS&&Exact(c,TRUE);
}
BOOLEAN GuUnboundReadyForWindows(VOID) {
  if(Claimed||Gu.owned||Gu.fault||!Gu.quiesced||Gu.flrs!=1||!GuSafeToReturn())return FALSE;
  UINT32 chip=GuRead32(0),cpu=GuRead32(0x110100),hi=GuRead32(0x1fa828);
  UINT32 ss0=GuRead32(0x82381c),ss1=GuRead32(0x823820),readout=GuRead32(0x823818),plm=GuRead32(0x823804);
  return !Gu.fault&&chip==0x193000a1&&(cpu&0x10)&&cpu!=0xffffffff&&
    (cpu&0xffff0000)!=0xbadf0000&&!(hi>>4)&&ss0==0x88888888&&ss1==8&&
    !(readout&0x07ffffff)&&readout!=0xffffffff&&plm==0xffffffff;
}
BOOLEAN GuUnboundMutated(VOID){return Mutated;}
