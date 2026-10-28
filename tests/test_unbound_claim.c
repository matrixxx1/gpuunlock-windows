/* SPDX-License-Identifier: GPL-2.0-only */
#include "../port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef EPROTO
#undef EBADMSG
#undef EOPNOTSUPP
#undef ETIMEDOUT
#undef EOVERFLOW
#undef EALREADY
#include "../unbound_claim.c"

#define CHECK(x) do{if(!(x)){fprintf(stderr,"check failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
GU_PLATFORM Gu;
static EFI_BOOT_SERVICES Bs;
static EFI_SYSTEM_TABLE St;
static EFI_PCI_IO_PROTOCOL Pci;
static UINT32 Cfg[64];
static UINT64 Attributes;
static BOOLEAN Active,PartialEnable,Opened,Closed;
static EFI_HANDLE Image=(EFI_HANDLE)(UINTN)0x11,Handle=(EFI_HANDLE)(UINTN)0x22;

VOID GuLog(const char *f,...){(void)f;}
BOOLEAN GuSafeToReturn(VOID){return !Gu.retained&&!Gu.allocations&&!Gu.dma_count&&!Gu.master_attributes_pending;}
UINT32 GuRead32(UINT32 o){(void)o;return 0;}

static EFI_TPL EFIAPI Raise(EFI_TPL t){return t;}
static VOID EFIAPI Restore(EFI_TPL t){(void)t;}
static EFI_STATUS EFIAPI Free(VOID *p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *count){
  CHECK(h==Handle);*out=malloc(sizeof(**out));CHECK(*out);(*out)[0]=&PciGuid;*count=1;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Info(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *count){
  CHECK(h==Handle&&g==&PciGuid);*out=calloc(1,sizeof(**out));CHECK(*out);*count=1;
  (*out)[0].Attributes=Active?EFI_OPEN_PROTOCOL_BY_DRIVER:EFI_OPEN_PROTOCOL_GET_PROTOCOL;
  (*out)[0].OpenCount=1;(*out)[0].AgentHandle=(EFI_HANDLE)(UINTN)0x33;(*out)[0].ControllerHandle=Handle;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Open(EFI_HANDLE h,EFI_GUID *g,VOID **out,EFI_HANDLE image,EFI_HANDLE c,UINT32 a){
  CHECK(h==Handle&&g==&PciGuid&&image==Image&&!c&&a==EFI_OPEN_PROTOCOL_EXCLUSIVE);
  Opened=TRUE;*out=&Pci;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Close(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE image,EFI_HANDLE c){
  CHECK(h==Handle&&g==&PciGuid&&image==Image&&!c&&Opened);Closed=TRUE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN count,VOID *out){
  CHECK(p==&Pci&&w==EfiPciIoWidthUint32&&off==0&&count==64);memcpy(out,Cfg,sizeof(Cfg));return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Attr(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out){
  CHECK(p==&Pci);
  if(op==EfiPciIoAttributeOperationSupported){*out=EFI_PCI_IO_ATTRIBUTE_MEMORY|EFI_PCI_IO_ATTRIBUTE_BUS_MASTER;return EFI_SUCCESS;}
  if(op==EfiPciIoAttributeOperationGet){*out=Attributes;return EFI_SUCCESS;}
  CHECK(mask==EFI_PCI_IO_ATTRIBUTE_MEMORY&&!out);
  if(op==EfiPciIoAttributeOperationEnable){Attributes|=mask;Cfg[1]|=2;return PartialEnable?EFI_DEVICE_ERROR:EFI_SUCCESS;}
  CHECK(op==EfiPciIoAttributeOperationDisable);Attributes&=~mask;Cfg[1]&=~2U;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI BarMock(EFI_PCI_IO_PROTOCOL *p,UINT8 index,UINT64 *supports,VOID **resources){
  CHECK(p==&Pci&&!index&&!supports);UINT8 *raw=calloc(1,sizeof(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR)+2);CHECK(raw);
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *r=(VOID *)raw;r->Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;r->Len=sizeof(*r)-3;
  r->ResType=0;r->AddrSpaceGranularity=32;r->AddrRangeMin=GU_BAR0_BASE;r->AddrRangeMax=0x86ffffff;
  r->AddrLen=0x1000000;raw[sizeof(*r)]=ACPI_END_TAG_DESCRIPTOR;*resources=r;return EFI_SUCCESS;
}
static VOID Reset(VOID){
  memset(&Gu,0,sizeof(Gu));memset(&Bs,0,sizeof(Bs));memset(&St,0,sizeof(St));memset(&Pci,0,sizeof(Pci));
  memcpy(Cfg,Cold,sizeof(Cfg));Attributes=0;Active=PartialEnable=Opened=Closed=FALSE;
  Bs.ProtocolsPerHandle=Protocols;Bs.OpenProtocolInformation=Info;Bs.OpenProtocol=Open;Bs.CloseProtocol=Close;
  Bs.RaiseTPL=Raise;Bs.RestoreTPL=Restore;Bs.FreePool=Free;St.BootServices=&Bs;
  Pci.Pci.Read=Read;Pci.Attributes=Attr;Pci.GetBarAttributes=BarMock;
  Gu.bs=&Bs;Gu.pci=&Pci;Gu.image=Image;Gu.handle=Handle;
  Table=NULL;Self=Target=NULL;Claimed=DecodeEnabled=Mutated=FALSE;
}
int main(void){
  Reset();CHECK(GuUnboundClaim(&St,Image,Handle)==EFI_SUCCESS);CHECK(Opened&&Gu.owned&&(Cfg[1]&2));
  Gu.profile_verified=Gu.prom_verified=TRUE;CHECK(GuUnboundReadyForTransaction());
  CHECK(GuUnboundFinish(TRUE)==EFI_SUCCESS&&Closed&&!Gu.owned&&Cfg[1]==Cold[1]);
  Reset();Active=TRUE;CHECK(GuUnboundClaim(&St,Image,Handle)==EFI_ACCESS_DENIED);CHECK(!Opened&&!Mutated&&Cfg[1]==Cold[1]);
  Reset();Cfg[4]^=0x1000;CHECK(GuUnboundClaim(&St,Image,Handle)==EFI_NOT_READY);CHECK(!Opened&&!Mutated);
  Reset();PartialEnable=TRUE;CHECK(GuUnboundClaim(&St,Image,Handle)==EFI_DEVICE_ERROR);CHECK(Opened&&Gu.owned&&DecodeEnabled);
  CHECK(GuUnboundFinish(TRUE)==EFI_SUCCESS&&Closed&&Cfg[1]==Cold[1]);
  puts("AD103M unbound ownership, exact config, and partial-enable containment PASS");return 0;
}
