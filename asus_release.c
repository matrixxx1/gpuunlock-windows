/* SPDX-License-Identifier: GPL-2.0-only */
/* ASUS G834JYR/AD103M Ultimate-mode ownership adapter. The snapshot/delta
 * strategy is derived from impacifist/Windows-GPU-UNLOCK 9352abd and the
 * upstream AD104 MSI port. All device paths, driver metadata, and executable
 * code hashes below were captured read-only on this exact machine/BIOS.
 * Protocol-delta checks are scoped to the GPU, GOP, console, loader, and their
 * owner drivers so unrelated firmware activity cannot cause a false refusal.
 * No driver code or data is patched and no firmware variable is written.
 */
#include "asus_release.h"
#include "boot_io.h"
#include "profile.h"
#include "sha256.h"
#include "compat/compat.h"
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <IndustryStandard/Acpi10.h>

#define MAX_EDGES 4096
typedef struct {EFI_HANDLE handle,agent,controller;EFI_GUID protocol;UINT32 attributes,count;} EDGE;
typedef struct {UINTN count;EDGE edges[MAX_EDGES];} GRAPH;
typedef struct {
  EFI_SYSTEM_TABLE *table;EFI_HANDLE self,pci,gop,out_agent,gfx_agent,edid_agent,nv_agent;
  EFI_HANDLE console,input,error;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *text,*err;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL *keys;
  UINTN key_reset,key_read;EFI_EVENT key_wait;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop_interface;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mode;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION info;
  UINT32 config[64];UINT64 attributes;
  UINTN stage;BOOLEAN ready,mutated,claimed;
} PLAN;
static PLAN Plan;
static GRAPH Original,Current;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
static EFI_GUID InputGuid=EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
static EFI_GUID BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID,ImageGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID EdidGuid={0x1c0c34f6,0xd380,0x41fa,{0xa0,0x49,0x8a,0xd0,0x6c,0x1a,0x66,0xaa}};
static const UINT8 PciPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,1,1,1,6,0,0,0,127,255,4,0};
static const UINT8 GopPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,0,1,1,1,6,0,0,0,2,3,8,0,0,4,1,0x80,127,255,4,0};
static const UINT8 OutFile[]={4,6,20,0,0x7d,0x49,0x8a,0x62,0xf6,0x2b,0x64,0x42,0x87,0x41,6,0x9d,0xbd,0x33,0x99,0xd6,127,255,4,0};
static const UINT8 GfxFile[]={4,6,20,0,0xdd,0xab,0xe7,0x43,0x52,0xe3,0xfb,0x4c,0xa2,0x30,0x4c,0xdc,0x1d,0x35,0x0e,0x5c,127,255,4,0};
static const UINT8 EdidFile[]={4,6,20,0,0x0c,0x03,0xa7,0xc7,0xd8,0xc3,0xee,0x45,0xbe,0xd9,0x5d,0x9e,0x76,0x76,0x29,0x53,127,255,4,0};
static const UINT8 NvFile[]={4,4,28,0,'n',0,'v',0,'g',0,'o',0,'p',0,'-',0,'a',0,'d',0,'1',0,'x',0,'x',0,0,0,127,255,4,0};

static BOOLEAN Equal(const VOID *a,const VOID *b,UINTN n){return !memcmp(a,b,n);}
static BOOLEAN Guid(const EFI_GUID *a,const EFI_GUID *b){return Equal(a,b,16);}
static EFI_STATUS Interface(EFI_HANDLE h,EFI_GUID *g,VOID **p) {
  *p=NULL;EFI_STATUS s=Plan.table->BootServices->HandleProtocol(h,g,p);
  return s==EFI_SUCCESS&&!*p?EFI_COMPROMISED_DATA:s;
}
static BOOLEAN Path(EFI_HANDLE h,const UINT8 *bytes,UINTN count) {
  EFI_DEVICE_PATH_PROTOCOL *p=NULL;
  if(Interface(h,&PathGuid,(VOID **)&p)!=EFI_SUCCESS)return FALSE;
  UINTN at=0;
  while(at+4<=count) {
    UINTN n=p->Length[0]+((UINTN)p->Length[1]<<8);
    if(n<4||n>count-at||!Equal(p,bytes+at,n))return FALSE;
    if(p->Type==END_DEVICE_PATH_TYPE)return n==4&&p->SubType==END_ENTIRE_DEVICE_PATH_SUBTYPE&&at+4==count;
    at+=n;p=(VOID *)((UINT8 *)p+n);
  }
  return FALSE;
}
static EFI_STATUS Snapshot(GRAPH *g) {
  EFI_BOOT_SERVICES *b=Plan.table->BootServices;EFI_HANDLE *handles=NULL;UINTN count=0;g->count=0;
  EFI_STATUS s=b->LocateHandleBuffer(AllHandles,NULL,NULL,&count,&handles);
  if(s==EFI_SUCCESS&&(!handles||!count||count>4096))s=EFI_BAD_BUFFER_SIZE;
  for(UINTN i=0;s==EFI_SUCCESS&&i<count;i++) {
    EFI_GUID **protocols=NULL;UINTN pc=0;s=b->ProtocolsPerHandle(handles[i],&protocols,&pc);
    if(s==EFI_SUCCESS&&(!protocols||!pc||pc>64))s=EFI_BAD_BUFFER_SIZE;
    for(UINTN j=0;s==EFI_SUCCESS&&j<pc;j++) {
      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *entries=NULL;UINTN ec=0;
      if(!protocols[j]){s=EFI_COMPROMISED_DATA;break;}
      s=b->OpenProtocolInformation(handles[i],protocols[j],&entries,&ec);
      if(s==EFI_SUCCESS&&(ec>128||(ec&&!entries)))s=EFI_BAD_BUFFER_SIZE;
      for(UINTN k=0;s==EFI_SUCCESS&&k<ec;k++) {
        EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&entries[k];
        if(e->Attributes==EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL||e->Attributes==EFI_OPEN_PROTOCOL_GET_PROTOCOL||
           e->Attributes==EFI_OPEN_PROTOCOL_TEST_PROTOCOL)continue;
        BOOLEAN own=Plan.claimed&&handles[i]==Plan.pci&&e->AgentHandle==Plan.self&&
          !e->ControllerHandle&&e->Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE&&e->OpenCount==1;
        if(g->count==MAX_EDGES||!e->AgentHandle||(!e->ControllerHandle&&!own)||!e->OpenCount){s=EFI_COMPROMISED_DATA;break;}
        g->edges[g->count++]=(EDGE){handles[i],e->AgentHandle,e->ControllerHandle,*protocols[j],e->Attributes,e->OpenCount};
      }
      if(entries&&b->FreePool(entries)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
    }
    if(protocols&&b->FreePool(protocols)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  }
  if(handles&&b->FreePool(handles)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  return s;
}
static BOOLEAN SameEdge(const EDGE *a,const EDGE *b) {
  return a->handle==b->handle&&a->agent==b->agent&&a->controller==b->controller&&
    a->attributes==b->attributes&&a->count==b->count&&Guid(&a->protocol,&b->protocol);
}
static BOOLEAN Removed(const EDGE *e,UINTN stage) {
  if(stage>=1&&e->handle==Plan.gop&&e->agent==Plan.out_agent&&Guid(&e->protocol,&TextGuid))return TRUE;
  if(stage>=2&&e->handle==Plan.gop&&e->agent==Plan.gfx_agent&&Guid(&e->protocol,&GopGuid))return TRUE;
  if(stage>=3&&e->handle==Plan.gop&&e->agent==Plan.edid_agent&&Guid(&e->protocol,&EdidGuid))return TRUE;
  return stage>=4&&e->handle==Plan.pci&&e->agent==Plan.nv_agent;
}
static BOOLEAN ProtectedHandle(EFI_HANDLE h) {
  return h&&(h==Plan.pci||h==Plan.gop||h==Plan.self||h==Plan.out_agent||h==Plan.gfx_agent||h==Plan.edid_agent||
    h==Plan.nv_agent||h==Plan.console||h==Plan.input||h==Plan.error);
}
static BOOLEAN ProtectedEdge(const EDGE *e) {
  return ProtectedHandle(e->handle)||ProtectedHandle(e->agent)||ProtectedHandle(e->controller);
}
static BOOLEAN OwnEdge(const EDGE *e) {
  return e->handle==Plan.pci&&e->agent==Plan.self&&!e->controller&&Guid(&e->protocol,&PciGuid)&&
    e->attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE&&e->count==1;
}
static BOOLEAN KnownEdge(const EDGE *e,UINTN stage) {
  for(UINTN i=0;i<Original.count;i++)if(!Removed(&Original.edges[i],stage)&&SameEdge(&Original.edges[i],e))return TRUE;
  return FALSE;
}
static BOOLEAN Expected(UINTN stage,BOOLEAN claimed) {
  UINTN own=0;
  for(UINTN i=0;i<Original.count;i++) {
    if(!ProtectedEdge(&Original.edges[i]))continue;
    if(Removed(&Original.edges[i],stage))continue;
    UINTN matches=0;for(UINTN j=0;j<Current.count;j++)if(SameEdge(&Original.edges[i],&Current.edges[j]))matches++;
    if(matches!=1)return FALSE;
  }
  for(UINTN j=0;j<Current.count;j++) {
    const EDGE *e=&Current.edges[j];if(OwnEdge(e)){own++;continue;}
    if(!KnownEdge(e,stage)&&ProtectedEdge(e))return FALSE;
    if(ProtectedEdge(e))for(UINTN k=j+1;k<Current.count;k++)if(SameEdge(e,&Current.edges[k]))return FALSE;
  }
  return own==(claimed?1u:0u);
}
#define NOTE(...) do{if(GuBootLogOpen())GuLog(__VA_ARGS__);}while(0)
static VOID NoteEdge(const char *kind,const EDGE *e) {
  NOTE("ASUS graph %s: handle=%#llx agent=%#llx controller=%#llx protocol=%#x attributes=%#x count=%u\n",kind,
    (unsigned long long)(UINTN)e->handle,(unsigned long long)(UINTN)e->agent,(unsigned long long)(UINTN)e->controller,
    e->protocol.Data1,e->attributes,e->count);
}
static VOID GraphDiff(BOOLEAN claimed) {
  for(UINTN i=0;i<Original.count;i++) {
    if(!ProtectedEdge(&Original.edges[i]))continue;
    if(Removed(&Original.edges[i],4))continue;
    UINTN matches=0;for(UINTN j=0;j<Current.count;j++)if(SameEdge(&Original.edges[i],&Current.edges[j]))matches++;
    if(matches!=1){NoteEdge(matches?"duplicate":"missing",&Original.edges[i]);return;}
  }
  for(UINTN j=0;j<Current.count;j++) {
    const EDGE *e=&Current.edges[j];
    if(!KnownEdge(e,4)&&!(claimed&&OwnEdge(e))&&ProtectedEdge(e)){NoteEdge("unexpected",e);return;}
    if(ProtectedEdge(e))for(UINTN k=j+1;k<Current.count;k++)if(SameEdge(e,&Current.edges[k])){NoteEdge("duplicate",e);return;}
  }
}
static EFI_STATUS Driver(UINTN which) {
  EFI_HANDLE a=which==0?Plan.out_agent:which==1?Plan.gfx_agent:which==2?Plan.edid_agent:Plan.nv_agent;
  EFI_DRIVER_BINDING_PROTOCOL *d=NULL;EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
  EFI_STATUS s=Interface(a,&BindingGuid,(VOID **)&d);
  if(s!=EFI_SUCCESS||!d->Stop||d->DriverBindingHandle!=a||!d->ImageHandle||
     d->Version!=(which==3?0x70011u:which==2?1u:0x10u))return EFI_UNSUPPORTED;
  s=Interface(d->ImageHandle,&ImageGuid,(VOID **)&im);if(s!=EFI_SUCCESS)return s;
  UINTN size=which==0?0x5ec0:which==1?0x3be0:which==2?0x9000:0x27ac0;
  UINTN stop=which==0?0x1008:which==1?0xb5c:which==2?0x14d4:0x6ec;
  UINTN code=which==3?0x280:which==2?0x1000:0x2a0;
  UINTN bytes=which==0?0x4cb0:which==1?0x1e20:which==2?0x372e:0x21bc7;
  const char *hash=which==0?"981a2b6ba148f6b4b0e2128af68e81e5bbf6b0e0c72ddbf722497474057aaa92":
    which==1?"ece28d4e20f28f5ca16bbe0a49ad6b0ad33956d55851c4ed620f617a6c237097":
    which==2?"6eff4e410bd398eb11996bab343aef15ee8da5ac34b6adcd774f11416643f789":
    "216c7270fa08ad9e4c3694d34d825e9e904554a1af5ffa2aeead4317c5537b37";
  const UINT8 *file=which==0?OutFile:which==1?GfxFile:which==2?EdidFile:NvFile;
  UINTN file_bytes=which==3?sizeof(NvFile):sizeof(OutFile);
  if(!im->ImageBase||im->ImageSize!=size||im->ImageCodeType!=(which==2?EfiRuntimeServicesCode:EfiBootServicesCode)||
     (UINTN)d->Stop!=(UINTN)im->ImageBase+stop||!im->FilePath||!Equal(im->FilePath,file,file_bytes)||
     code+bytes>size||((which==2||which==3)&&d->ImageHandle!=a)||
     (which==3&&im->DeviceHandle!=Plan.pci))return EFI_UNSUPPORTED;
  /* DIAG10 proved every relocation lies outside these executable ranges. */
  return GuHashMatches((UINT8 *)im->ImageBase+code,bytes,hash)?EFI_SUCCESS:EFI_CRC_ERROR;
}
static EFI_STATUS Alive(VOID) {
  EFI_SYSTEM_TABLE *t=Plan.table;VOID *in=NULL,*out=NULL,*err=NULL;
  if(t->ConsoleOutHandle!=Plan.console||t->ConsoleInHandle!=Plan.input||t->StandardErrorHandle!=Plan.error||
     t->ConIn!=Plan.keys||t->ConOut!=Plan.text||t->StdErr!=Plan.err||!t->ConIn||
     (UINTN)t->ConIn->Reset!=Plan.key_reset||(UINTN)t->ConIn->ReadKeyStroke!=Plan.key_read||
     t->ConIn->WaitForKey!=Plan.key_wait)return EFI_NOT_READY;
  EFI_STATUS s=Interface(Plan.input,&InputGuid,&in);
  if(s==EFI_SUCCESS)s=Interface(Plan.console,&TextGuid,&out);
  if(s==EFI_SUCCESS)s=Interface(Plan.error,&TextGuid,&err);
  return s==EFI_SUCCESS&&(in!=Plan.keys||out!=Plan.text||err!=Plan.err)?EFI_NOT_READY:s;
}
static EFI_STATUS Gops(BOOLEAN first) {
  EFI_BOOT_SERVICES *b=Plan.table->BootServices;EFI_HANDLE *h=NULL;UINTN count=0,physical=0;
  EFI_STATUS s=b->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&h);
  NOTE("ASUS preflight GOP locate: status=%#llx count=%zu first=%u stage=%zu\n",
    (unsigned long long)s,(size_t)count,first,(size_t)Plan.stage);
  if(Plan.stage>=4&&s==EFI_NOT_FOUND)return EFI_SUCCESS;
  if(s==EFI_SUCCESS&&(count>2||(count&&!h)))s=EFI_UNSUPPORTED;
  for(UINTN i=0;s==EFI_SUCCESS&&i<count;i++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *g=NULL;s=Interface(h[i],&GopGuid,(VOID **)&g);if(s!=EFI_SUCCESS)break;
    NOTE("ASUS preflight GOP endpoint: index=%zu handle=%#llx console=%u interface=%#llx mode=%#llx fb=%#llx bytes=%#llx width=%u height=%u stride=%u format=%u\n",
      (size_t)i,(unsigned long long)(UINTN)h[i],h[i]==Plan.console,
      (unsigned long long)(UINTN)g,(unsigned long long)(UINTN)(g?g->Mode:NULL),
      (unsigned long long)(g&&g->Mode?g->Mode->FrameBufferBase:0),
      (unsigned long long)(g&&g->Mode?g->Mode->FrameBufferSize:0),
      g&&g->Mode&&g->Mode->Info?g->Mode->Info->HorizontalResolution:0,
      g&&g->Mode&&g->Mode->Info?g->Mode->Info->VerticalResolution:0,
      g&&g->Mode&&g->Mode->Info?g->Mode->Info->PixelsPerScanLine:0,
      g&&g->Mode&&g->Mode->Info?g->Mode->Info->PixelFormat:PixelFormatMax);
    if(h[i]==Plan.console&&g->Mode&&!g->Mode->FrameBufferBase&&!g->Mode->FrameBufferSize)continue;
    UINT64 needed=0;
    if(g->Mode&&g->Mode->Info)needed=(UINT64)g->Mode->Info->PixelsPerScanLine*
      g->Mode->Info->VerticalResolution*sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
    /* FrameBufferSize is the active GOP mode's mapped byte count on this BIOS:
     * the observed 1024x600 console reports 0x258000 while the native-mode
     * capture exposed a 0x1900000 maximum allocation. Identity comes from the
     * exact device path and base. Validate the live 32-bpp geometry, bound it
     * by the captured maximum, then pin the full mode structure below.
     */
    if(Plan.stage>=4||!Path(h[i],GopPath,sizeof(GopPath))||++physical!=1||!g->Mode||!g->Mode->Info||
       g->Mode->SizeOfInfo<sizeof(*g->Mode->Info)||g->Mode->FrameBufferBase!=0x6000000000ULL||
       g->Mode->Info->Version||g->Mode->Info->HorizontalResolution<640||
       g->Mode->Info->HorizontalResolution>2560||g->Mode->Info->VerticalResolution<480||
       g->Mode->Info->VerticalResolution>1600||
       g->Mode->Info->PixelsPerScanLine<g->Mode->Info->HorizontalResolution||
       g->Mode->Info->PixelsPerScanLine>4096||g->Mode->Info->PixelFormat>=PixelBltOnly||
       needed<0x12c000||needed>0x1900000||g->Mode->FrameBufferSize<needed||
       g->Mode->FrameBufferSize>0x1900000){s=EFI_UNSUPPORTED;break;}
    if(first){Plan.gop=h[i];Plan.gop_interface=g;Plan.mode=*g->Mode;Plan.info=*g->Mode->Info;}
    else if(h[i]!=Plan.gop||g!=Plan.gop_interface||!Equal(g->Mode,&Plan.mode,sizeof(Plan.mode))||
            !Equal(g->Mode->Info,&Plan.info,sizeof(Plan.info)))s=EFI_NOT_READY;
  }
  if(h&&b->FreePool(h)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS&&physical!=(Plan.stage>=4?0u:1u))s=EFI_NOT_READY;
  return s;
}
static EFI_STATUS ReadConfig(UINT32 c[64]) {
  EFI_PCI_IO_PROTOCOL *io=NULL;UINTN seg,bus,dev,fun;EFI_STATUS s=Interface(Plan.pci,&PciGuid,(VOID **)&io);
  if(s!=EFI_SUCCESS||io!=Gu.pci||!io->Pci.Read||!io->GetLocation)return EFI_NOT_READY;
  s=io->GetLocation(io,&seg,&bus,&dev,&fun);if(s!=EFI_SUCCESS||seg||bus!=1||dev||fun)return EFI_UNSUPPORTED;
  s=io->Pci.Read(io,EfiPciIoWidthUint32,0,64,c);
  if(s==EFI_SUCCESS&&(!GuTargetConfig(c,seg,bus,dev,fun)||(c[1]&4)))s=EFI_UNSUPPORTED;
  return s;
}
static BOOLEAN ConfigStable(const UINT32 c[64]) {
  for(UINTN i=0;i<64;i++) {
    UINT32 drift=c[i]^Plan.config[i];if(!drift)continue;
    BOOLEAN pcie_status=Gu.flrs==1&&i==0x80/4&&Plan.config[0x78/4]==0x0012b410&&
      !(c[i]&~Plan.config[i])&&!(drift&~0x00090000u);
    /* The NVIDIA vendor capability payload is restored immediately after FLR.
     * On this fixed G834JYR device, bit 0 at 0xb8 subsequently self-clears. It
     * is a hardware-owned status bit; every writable bit and the 0xbc payload
     * must still match the captured cold configuration exactly.
     */
    BOOLEAN vendor_status=Gu.flrs==1&&i==0xb8/4&&
      Plan.config[0xb4/4]==0x01140009U&&c[0xb4/4]==0x01140009U&&
      Plan.config[i]==0x06000001U&&c[i]==0x06000000U;
    BOOLEAN allowed=pcie_status||vendor_status;
    if(!allowed){NOTE("PCI config drift refused: offset=%#x captured=%#x now=%#x bits=%#x flrs=%u\n",
      (unsigned)(i*4),Plan.config[i],c[i],drift,Gu.flrs);return FALSE;}
    NOTE("%s status drift tolerated: offset=%#x captured=%#x now=%#x bits=%#x flrs=%u\n",
      vendor_status?"NVIDIA vendor":"PCI",
      (unsigned)(i*4),Plan.config[i],c[i],drift,Gu.flrs);
  }
  return TRUE;
}
static EFI_STATUS Bar(VOID) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=NULL;UINT64 support=0;if(!Gu.pci->GetBarAttributes)return EFI_UNSUPPORTED;
  EFI_STATUS s=Gu.pci->GetBarAttributes(Gu.pci,0,&support,(VOID **)&d);
  NOTE("ASUS preflight BAR: status=%#llx support=%#llx descriptor=%#llx desc=%#x len=%u type=%u granularity=%#llx min=%#llx max=%#llx translation=%#llx bytes=%#llx\n",
    (unsigned long long)s,(unsigned long long)support,(unsigned long long)(UINTN)d,
    d?d->Desc:0,d?d->Len:0,d?d->ResType:0,
    (unsigned long long)(d?d->AddrSpaceGranularity:0),(unsigned long long)(d?d->AddrRangeMin:0),
    (unsigned long long)(d?d->AddrRangeMax:0),(unsigned long long)(d?d->AddrTranslationOffset:0),
    (unsigned long long)(d?d->AddrLen:0));
  if(s==EFI_SUCCESS&&(!d||d->Desc!=0x8a||d->Len!=43||d->ResType||d->AddrSpaceGranularity!=32||
     d->AddrRangeMin!=GU_BAR0_BASE||d->AddrRangeMax!=0x86ffffff||d->AddrTranslationOffset||
     d->AddrLen!=0x1000000))s=EFI_UNSUPPORTED;
  if(d&&Plan.table->BootServices->FreePool(d)!=EFI_SUCCESS)s=EFI_DEVICE_ERROR;return s;
}
static EFI_STATUS Consumers(VOID) {
  UINTN pci=0,child=0,text=0,virtual=0,gfx=0,edid=0;
  for(UINTN i=0;i<Original.count;i++) {
    const EDGE *e=&Original.edges[i];
    if(e->handle==Plan.pci) {
      NoteEdge("target-pci",e);
      if(Guid(&e->protocol,&PciGuid)&&e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER&&e->controller==Plan.pci){Plan.nv_agent=e->agent;pci++;}
      else if(Guid(&e->protocol,&PathGuid)&&e->attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER&&e->controller==Plan.gop){child++;}
      else {NoteEdge("refused-pci",e);return EFI_ACCESS_DENIED;}
    } else if(e->handle==Plan.gop) {
      NoteEdge("target-gop",e);
      if(Guid(&e->protocol,&TextGuid)&&e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER&&e->controller==Plan.gop){Plan.out_agent=e->agent;text++;}
      else if(Guid(&e->protocol,&TextGuid)&&e->attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER&&e->controller==Plan.console){virtual++;}
      else if(Guid(&e->protocol,&GopGuid)&&e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER&&e->controller==Plan.gop){Plan.gfx_agent=e->agent;gfx++;}
      else if(Guid(&e->protocol,&EdidGuid)&&e->attributes==EFI_OPEN_PROTOCOL_BY_DRIVER&&e->controller==Plan.gop){Plan.edid_agent=e->agent;edid++;}
      else {NoteEdge("refused-gop",e);return EFI_ACCESS_DENIED;}
    }
  }
  GuLog("ASUS owner summary: pci=%zu child=%zu text=%zu virtual=%zu gfx=%zu edid=%zu\n",
    (size_t)pci,(size_t)child,(size_t)text,(size_t)virtual,(size_t)gfx,(size_t)edid);
  if(pci!=1||child!=1||text!=1||virtual!=1||gfx!=1||edid!=1||
     Plan.nv_agent==Plan.out_agent||Plan.nv_agent==Plan.gfx_agent||Plan.nv_agent==Plan.edid_agent||
     Plan.out_agent==Plan.gfx_agent||Plan.out_agent==Plan.edid_agent||Plan.gfx_agent==Plan.edid_agent)return EFI_ACCESS_DENIED;
  for(UINTN i=0;i<4;i++){EFI_STATUS s=Driver(i);if(s!=EFI_SUCCESS)return s;}return EFI_SUCCESS;
}
EFI_STATUS GuAsusPreflight(EFI_SYSTEM_TABLE *t,EFI_HANDLE self,EFI_HANDLE pci) {
  if(Plan.mutated||Plan.ready)return EFI_ALREADY_STARTED;
  memset(&Plan,0,sizeof(Plan));Plan.table=t;Plan.self=self;Plan.pci=pci;
  if(!t||!t->BootServices||!self||!pci||Gu.handle!=pci||Gu.image!=self||Gu.bs!=t->BootServices||
     !GuAd103mStaticPrepared()||Gu.started||Gu.owned||Gu.armed)return EFI_INVALID_PARAMETER;
  EFI_BOOT_SERVICES *b=t->BootServices;
  if(!b->DisconnectController||!b->OpenProtocol||!b->CloseProtocol||!b->RaiseTPL||!b->RestoreTPL||
     !b->ProtocolsPerHandle||!b->OpenProtocolInformation||!b->LocateHandleBuffer||!b->HandleProtocol)return EFI_UNSUPPORTED;
  Plan.console=t->ConsoleOutHandle;Plan.input=t->ConsoleInHandle;Plan.error=t->StandardErrorHandle;
  Plan.text=t->ConOut;Plan.err=t->StdErr;Plan.keys=t->ConIn;
  GuLog("ASUS preflight console: out=%#llx in=%#llx err=%#llx text=%#llx keys=%#llx stderr=%#llx\n",
    (unsigned long long)(UINTN)Plan.console,(unsigned long long)(UINTN)Plan.input,
    (unsigned long long)(UINTN)Plan.error,(unsigned long long)(UINTN)Plan.text,
    (unsigned long long)(UINTN)Plan.keys,(unsigned long long)(UINTN)Plan.err);
  if(!Plan.console||Plan.console!=Plan.input||Plan.console!=Plan.error||!Plan.text||!Plan.err||!Plan.keys||
     !Plan.keys->ReadKeyStroke||!Plan.keys->Reset||!Plan.keys->WaitForKey)return EFI_UNSUPPORTED;
  Plan.key_reset=(UINTN)Plan.keys->Reset;Plan.key_read=(UINTN)Plan.keys->ReadKeyStroke;Plan.key_wait=Plan.keys->WaitForKey;
  if(!Path(pci,PciPath,sizeof(PciPath))){GuLog("ASUS preflight failed: step=pci-path\n");return EFI_NOT_READY;}
  EFI_STATUS s=Gops(TRUE);GuLog("ASUS preflight step: gops=%#llx\n",(unsigned long long)s);
  if(s==EFI_SUCCESS){s=Alive();GuLog("ASUS preflight step: console-alive=%#llx\n",(unsigned long long)s);}
  if(s==EFI_SUCCESS){s=Bar();GuLog("ASUS preflight step: BAR=%#llx\n",(unsigned long long)s);}
  if(s==EFI_SUCCESS){s=ReadConfig(Plan.config);GuLog("ASUS preflight step: PCI-config=%#llx command=%#x\n",
    (unsigned long long)s,s==EFI_SUCCESS?Plan.config[1]&0xffffu:0);}
  if(s==EFI_SUCCESS&&(Plan.config[1]&7)!=3)s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=Gu.pci->Attributes?Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&Plan.attributes):EFI_UNSUPPORTED;
  GuLog("ASUS preflight step: PCI-attributes=%#llx value=%#llx\n",
    (unsigned long long)s,(unsigned long long)Plan.attributes);
  if(s==EFI_SUCCESS&&Plan.attributes!=(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS){s=Snapshot(&Original);GuLog("ASUS preflight step: graph-snapshot=%#llx edges=%zu\n",
    (unsigned long long)s,(size_t)Original.count);}
  if(s==EFI_SUCCESS){s=Consumers();GuLog("ASUS preflight step: consumers=%#llx\n",(unsigned long long)s);}
  GuLog("ASUS Ultimate preflight: status=%#llx graph=%zu code_profiles=4\n",(unsigned long long)s,(size_t)Original.count);
  if(s==EFI_SUCCESS){Plan.ready=TRUE;GuLog("ASUS endpoints: pci=%#llx gop=%#llx console=%#llx\n",
    (unsigned long long)(UINTN)Plan.pci,(unsigned long long)(UINTN)Plan.gop,(unsigned long long)(UINTN)Plan.console);}
  return s;
}
static BOOLEAN DecodeMask(const UINT32 c[64],UINT64 *mask) {
  *mask=0;UINT32 missing=Plan.config[1]&~c[1]&3;if((Plan.config[1]&7)!=3||(c[1]&4))return FALSE;
  for(UINTN i=0;i<64;i++)if((c[i]|(i==1?missing:0))!=Plan.config[i])return FALSE;
  *mask=(missing&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(missing&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0);return TRUE;
}
static EFI_STATUS ClaimRestore(VOID) {
  EFI_BOOT_SERVICES *b=Plan.table->BootServices;EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 c[64];UINT64 mask=0,attrs=0,supported=0;
  EFI_STATUS s=ReadConfig(c);if(s==EFI_SUCCESS&&!DecodeMask(c,&mask))s=EFI_NOT_READY;
  GuLog("ASUS decode restore intent: status=%#llx IO_MEM_mask=%#llx\n",(unsigned long long)s,(unsigned long long)mask);
  EFI_TPL old=b->RaiseTPL(TPL_NOTIFY);
  if(s==EFI_SUCCESS)s=ReadConfig(c);if(s==EFI_SUCCESS&&!DecodeMask(c,&mask))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=Snapshot(&Current);if(s==EFI_SUCCESS&&!Expected(4,FALSE))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=b->OpenProtocol(Plan.pci,&PciGuid,(VOID **)&io,Plan.self,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE);
  if(s==EFI_SUCCESS){Plan.claimed=TRUE;Gu.owned=TRUE;if(io!=Gu.pci)s=EFI_COMPROMISED_DATA;}
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&attrs);
  if(s==EFI_SUCCESS&&attrs!=(Plan.attributes&~mask))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationSupported,0,&supported);
  if(s==EFI_SUCCESS&&(supported&mask)!=mask)s=EFI_UNSUPPORTED;
  if(s==EFI_SUCCESS)s=Snapshot(&Current);if(s==EFI_SUCCESS&&!Expected(4,TRUE))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS&&mask)s=io->Attributes(io,EfiPciIoAttributeOperationEnable,mask,NULL);
  b->RestoreTPL(old);
  if(s==EFI_SUCCESS)s=ReadConfig(c);if(s==EFI_SUCCESS&&!Equal(c,Plan.config,sizeof(c)))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&attrs);
  if(s==EFI_SUCCESS&&attrs!=Plan.attributes)s=EFI_NOT_READY;if(s==EFI_SUCCESS)s=Bar();
  GuLog("ASUS exclusive claim/decode result: status=%#llx\n",(unsigned long long)s);return s;
}
EFI_STATUS GuAsusRelease(VOID) {
  if(!Plan.ready||Plan.mutated||Gu.started||Gu.owned)return EFI_NOT_READY;EFI_BOOT_SERVICES *b=Plan.table->BootServices;
  for(UINTN stage=1;stage<=4;stage++) {
    GuLog("ASUS driver detach intent: stage=%zu\n",(size_t)stage);EFI_STATUS s=Alive();UINT32 c[64];
    if(s==EFI_SUCCESS)s=ReadConfig(c);if(s==EFI_SUCCESS&&!Equal(c,Plan.config,sizeof(c)))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Gops(FALSE);EFI_TPL old=b->RaiseTPL(TPL_NOTIFY);
    if(s==EFI_SUCCESS)s=Snapshot(&Current);if(s==EFI_SUCCESS&&!Expected(stage-1,FALSE))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Driver(stage-1);b->RestoreTPL(old);
    if(s!=EFI_SUCCESS||Gu.fault)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
    Plan.mutated=TRUE;s=b->DisconnectController(stage==4?Plan.pci:Plan.gop,
      stage==1?Plan.out_agent:stage==2?Plan.gfx_agent:stage==3?Plan.edid_agent:Plan.nv_agent,NULL);
    GuLog("ASUS driver detach return: stage=%zu status=%#llx\n",(size_t)stage,(unsigned long long)s);
    if(s!=EFI_SUCCESS||Gu.fault)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;Plan.stage=stage;
    s=Snapshot(&Current);if(s==EFI_SUCCESS&&!Expected(stage,FALSE))s=EFI_NOT_READY;
    if(s==EFI_SUCCESS)s=Alive();if(s==EFI_SUCCESS)s=Gops(FALSE);if(s!=EFI_SUCCESS)return s;
  }
  EFI_STATUS s=ClaimRestore();if(s==EFI_SUCCESS&&!GuAsusReleased())s=EFI_NOT_READY;return s;
}
BOOLEAN GuAsusReleased(VOID) {
  UINT32 c[64];UINT64 attrs=0;const char *failed=NULL;
  if(!Plan.ready||Plan.stage!=4||!Plan.claimed||!Gu.owned||Gu.fault)failed="state";
  else if(Alive()!=EFI_SUCCESS)failed="console";else if(Gops(FALSE)!=EFI_SUCCESS)failed="GOP";
  else if(ReadConfig(c)!=EFI_SUCCESS)failed="config-read";else if(!ConfigStable(c))failed="config-drift";
  else if(Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attrs)!=EFI_SUCCESS||attrs!=Plan.attributes)failed="attributes";
  else if(Snapshot(&Current)!=EFI_SUCCESS)failed="graph-snapshot";
  else if(!Expected(4,TRUE)){failed="graph";GraphDiff(TRUE);}
  if(failed)NOTE("ASUS release check failed: %s attributes=%#llx expected=%#llx graph=%zu original=%zu fault=%u flrs=%u\n",
    failed,(unsigned long long)attrs,(unsigned long long)Plan.attributes,(size_t)Current.count,
    (size_t)Original.count,Gu.fault,Gu.flrs);return !failed;
}
EFI_STATUS GuAsusFinish(VOID) {
  if(!GuSafeToReturn()||!Gu.quiesced||Gu.flrs!=1){NOTE("ASUS finish refused before reconcile\n");return EFI_NOT_READY;}
  UINT32 c[64];UINT64 attrs=0,supported=0,missing=0;EFI_PCI_IO_PROTOCOL *io=Gu.pci;
  EFI_STATUS s=ReadConfig(c);if(s==EFI_SUCCESS&&!ConfigStable(c))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&attrs);
  if(s==EFI_SUCCESS&&(attrs&~Plan.attributes))s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)missing=Plan.attributes&~attrs;
  if(s==EFI_SUCCESS&&missing)s=io->Attributes(io,EfiPciIoAttributeOperationSupported,0,&supported);
  if(s==EFI_SUCCESS&&missing&&(supported&missing)!=missing)s=EFI_UNSUPPORTED;
  if(s==EFI_SUCCESS&&missing)s=io->Attributes(io,EfiPciIoAttributeOperationEnable,missing,NULL);
  if(s==EFI_SUCCESS)s=io->Attributes(io,EfiPciIoAttributeOperationGet,0,&attrs);
  if(s==EFI_SUCCESS&&attrs!=Plan.attributes)s=EFI_NOT_READY;
  if(s==EFI_SUCCESS)s=ReadConfig(c);if(s==EFI_SUCCESS&&!ConfigStable(c))s=EFI_NOT_READY;
  NOTE("ASUS post-FLR attribute reconcile: status=%#llx missing=%#llx attributes=%#llx expected=%#llx\n",
    (unsigned long long)s,(unsigned long long)missing,(unsigned long long)attrs,
    (unsigned long long)Plan.attributes);
  if(s!=EFI_SUCCESS||!GuAsusReleased()){NOTE("ASUS finish refused before close\n");return s==EFI_SUCCESS?EFI_NOT_READY:s;}
  s=Plan.table->BootServices->CloseProtocol(Plan.pci,&PciGuid,Plan.self,NULL);
  if(s!=EFI_SUCCESS){NOTE("ASUS finish close status=%#llx\n",(unsigned long long)s);return s;}
  Plan.claimed=FALSE;Gu.owned=FALSE;s=Snapshot(&Current);
  if(s==EFI_SUCCESS&&!Expected(4,FALSE)){s=EFI_NOT_READY;GraphDiff(FALSE);}return s;
}
BOOLEAN GuAsusMutated(VOID){return Plan.mutated;}
BOOLEAN GuAsusReadyForWindows(VOID) {
  if(!Plan.ready||Plan.stage!=4||Plan.claimed||Gu.owned||Gu.fault||!Gu.quiesced||Gu.flrs!=1||!GuSafeToReturn())return FALSE;
  UINT32 c[64];if(Alive()!=EFI_SUCCESS||Gops(FALSE)!=EFI_SUCCESS||ReadConfig(c)!=EFI_SUCCESS||
     !ConfigStable(c)||Snapshot(&Current)!=EFI_SUCCESS||!Expected(4,FALSE))return FALSE;
  UINT32 chip=GuRead32(0),cpu=GuRead32(0x110100),hi=GuRead32(0x1fa828);
  UINT32 ss0=GuRead32(0x82381c),ss1=GuRead32(0x823820),readout=GuRead32(0x823818),plm=GuRead32(0x823804);
  return !Gu.fault&&chip==0x193000a1&&(cpu&0x10)&&cpu!=0xffffffff&&
    (cpu&0xffff0000)!=0xbadf0000&&!(hi>>4)&&ss0==0x88888888&&ss1==8&&
    !(readout&0x07ffffff)&&readout!=0xffffffff&&plm==0xffffffff;
}
