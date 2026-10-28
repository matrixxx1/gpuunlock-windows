/* SPDX-License-Identifier: GPL-2.0-only */
/* Real ASUS ownership adapter and captured driver images; all EFI effects simulated. */
#include "../asus_release.c"
#include "../format.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

enum MODE {OK,GOP_SMALL_MODE,GOP_UNDERSIZED,MULTI_OPEN,DRIVER_VERSION,DRIVER_HASH,DRIVER_PATH,DRIVER_STOP,EXTRA_GPU_OWNER,
  GRAPH_DRIFT,UNRELATED_GRAPH_DRIFT,STOP2_FAIL,CONFIG_DRIFT,VENDOR_PAYLOAD_DRIFT,ATTR_FAIL,CLOSE_FAIL,MODE_COUNT};
static enum MODE Mode;static unsigned Checks;
#define CHECK(x) do{Checks++;if(!(x)){fprintf(stderr,"ASUS FAIL mode=%d line=%d: %s\n",Mode,__LINE__,#x);exit(1);}}while(0)
GU_PLATFORM Gu;
static EFI_BOOT_SERVICES Bs;static EFI_SYSTEM_TABLE St;static EFI_PCI_IO_PROTOCOL Io;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Keys;static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Text;
static EFI_GRAPHICS_OUTPUT_PROTOCOL Gop;static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GopMode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION GopInfo;
static EFI_DRIVER_BINDING_PROTOCOL Bind[4];static EFI_LOADED_IMAGE_PROTOCOL Images[4];
static UINT8 *Code[4];static UINT32 Cfg[64];static UINT64 Attrs;
static UINTN Stage,Stops,Enables,Closes;static BOOLEAN Exclusive,Drift,StaticOkay;
static char Notes[32768];static size_t NotesUsed;
static EFI_HANDLE H(UINTN id){return (EFI_HANDLE)(id+1);}
static EFI_HANDLE Agent(UINTN i){return H(20+i);}
static const EDGE Baseline[]={
  {(EFI_HANDLE)11,(EFI_HANDLE)24,(EFI_HANDLE)11,EFI_PCI_IO_PROTOCOL_GUID,0x10,1},
  {(EFI_HANDLE)11,(EFI_HANDLE)24,(EFI_HANDLE)12,EFI_DEVICE_PATH_PROTOCOL_GUID,0x08,1},
  {(EFI_HANDLE)12,(EFI_HANDLE)21,(EFI_HANDLE)12,EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,0x10,1},
  {(EFI_HANDLE)12,(EFI_HANDLE)21,(EFI_HANDLE)13,EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID,0x08,1},
  {(EFI_HANDLE)12,(EFI_HANDLE)22,(EFI_HANDLE)12,EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,0x10,1},
  {(EFI_HANDLE)12,(EFI_HANDLE)23,(EFI_HANDLE)12,{0x1c0c34f6,0xd380,0x41fa,{0xa0,0x49,0x8a,0xd0,0x6c,0x1a,0x66,0xaa}},0x10,1},
  {(EFI_HANDLE)41,(EFI_HANDLE)42,(EFI_HANDLE)41,{0x12345678,0x1111,0x2222,{1,2,3,4,5,6,7,8}},0x10,1}
};
static EFI_STATUS EFIAPI KeyRead(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *key){(void)p;(void)key;return EFI_NOT_READY;}
static EFI_STATUS EFIAPI KeyReset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,BOOLEAN verify){(void)p;(void)verify;return EFI_SUCCESS;}
VOID GuLog(const char *f,...){char line[2048];va_list a;va_start(a,f);size_t n=GuFormatV(line,sizeof(line),f,a);va_end(a);
  CHECK(n!=(size_t)-1&&n<sizeof(line)&&NotesUsed+n<sizeof(Notes));memcpy(Notes+NotesUsed,line,n);NotesUsed+=n;Notes[NotesUsed]=0;}
VOID GuLogText(const char *s){(void)s;}
BOOLEAN GuBootLogOpen(VOID){return TRUE;}
BOOLEAN GuTargetConfig(const UINT32 *c,UINTN s,UINTN b,UINTN d,UINTN f){return c[0]==GU_BOARD_ID&&c[11]==GU_BOARD_SUBSYSTEM&&!s&&b==1&&!d&&!f;}
BOOLEAN GuAd103mStaticPrepared(VOID){return StaticOkay;}
BOOLEAN GuSafeToReturn(VOID){return TRUE;}
UINT32 GuRead32(UINT32 off){switch(off){case 0:return 0x193000a1;case 0x110100:return 0x10;case 0x82381c:return 0x88888888;
  case 0x823820:return 8;case 0x823804:return 0xffffffff;default:return 0;}}
static BOOLEAN RemovedEdge(const EDGE *e){
  if(Stage>=1&&e->handle==H(11)&&e->agent==Agent(0)&&Guid(&e->protocol,&TextGuid))return TRUE;
  if(Stage>=2&&e->handle==H(11)&&e->agent==Agent(1)&&Guid(&e->protocol,&GopGuid))return TRUE;
  if(Stage>=3&&e->handle==H(11)&&e->agent==Agent(2)&&Guid(&e->protocol,&EdidGuid))return TRUE;
  return Stage>=4&&e->handle==H(10)&&e->agent==Agent(3);
}
static EFI_STATUS EFIAPI Free(VOID *p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE kind,EFI_GUID *g,VOID *key,UINTN *count,EFI_HANDLE **h){
  (void)key;*h=NULL;if(kind==AllHandles){*count=64;*h=malloc(*count*sizeof(**h));CHECK(*h);for(UINTN i=0;i<*count;i++)(*h)[i]=H(i);return EFI_SUCCESS;}
  CHECK(kind==ByProtocol&&Guid(g,&GopGuid));if(Stage>=4){*count=0;return EFI_NOT_FOUND;}
  *count=1;*h=malloc(sizeof(**h));CHECK(*h);(*h)[0]=H(11);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *g,VOID **p){
  *p=NULL;if(Guid(g,&PathGuid)){if(h==H(10))*p=(VOID *)PciPath;else if(h==H(11)&&Stage<4)*p=(VOID *)GopPath;}
  if(Guid(g,&PciGuid)&&h==H(10))*p=&Io;if(Guid(g,&GopGuid)&&h==H(11)&&Stage<4)*p=&Gop;
  if(Guid(g,&InputGuid)&&h==H(12))*p=&Keys;if(Guid(g,&TextGuid)&&h==H(12))*p=&Text;
  for(UINTN i=0;i<4;i++){if(Guid(g,&BindingGuid)&&h==Agent(i))*p=&Bind[i];if(Guid(g,&ImageGuid)&&h==Bind[i].ImageHandle)*p=&Images[i];}
  return *p?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static BOOLEAN EdgePresent(const EDGE *e){return !RemovedEdge(e);}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *n){
  EFI_GUID *unique[16];*n=0;for(UINTN i=0;i<ARRAY_SIZE(Baseline);i++)if(Baseline[i].handle==h&&EdgePresent(&Baseline[i])){
    BOOLEAN seen=FALSE;for(UINTN j=0;j<*n;j++)if(Guid(unique[j],&Baseline[i].protocol))seen=TRUE;if(!seen)unique[(*n)++]=(EFI_GUID *)&Baseline[i].protocol;}
  if(Exclusive&&h==H(10)){BOOLEAN seen=FALSE;for(UINTN j=0;j<*n;j++)if(Guid(unique[j],&PciGuid))seen=TRUE;if(!seen)unique[(*n)++]=&PciGuid;}
  if(!*n)unique[(*n)++]=&PathGuid;*out=malloc(*n*sizeof(**out));CHECK(*out);memcpy(*out,unique,*n*sizeof(**out));return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Opens(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n){
  *out=calloc(16,sizeof(**out));CHECK(*out);*n=0;
  for(UINTN i=0;i<ARRAY_SIZE(Baseline);i++){const EDGE *e=&Baseline[i];if(e->handle==h&&Guid(g,&e->protocol)&&EdgePresent(e))
    (*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){e->agent,e->controller,e->attributes,e->count};}
  if(Mode==MULTI_OPEN)for(UINTN i=0;i<*n;i++)(*out)[i].OpenCount=3;
  if(Exclusive&&h==H(10)&&Guid(g,&PciGuid))(*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(13),NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE,1};
  if((Mode==EXTRA_GPU_OWNER||(Mode==GRAPH_DRIFT&&Drift))&&h==H(10)&&Guid(g,&PciGuid))
    (*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(50),H(10),EFI_OPEN_PROTOCOL_BY_DRIVER,1};
  if(Mode==UNRELATED_GRAPH_DRIFT&&Drift&&h==H(40)&&g->Data1==0x12345678)
    (*out)[(*n)++]=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){H(50),H(40),EFI_OPEN_PROTOCOL_BY_DRIVER,1};
  return EFI_SUCCESS;
}
static EFI_TPL EFIAPI Raise(EFI_TPL t){CHECK(t==TPL_NOTIFY);return TPL_APPLICATION;}
static VOID EFIAPI Restore(EFI_TPL t){CHECK(t==TPL_APPLICATION);}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){CHECK(p==&Io);*s=*d=*f=0;*b=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI ConfigRead(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN count,VOID *out){
  CHECK(p==&Io&&w==EfiPciIoWidthUint32&&!off&&count==64);memcpy(out,Cfg,sizeof(Cfg));return EFI_SUCCESS;}
static EFI_STATUS EFIAPI GetBar(EFI_PCI_IO_PROTOCOL *p,UINT8 index,UINT64 *support,VOID **out){
  CHECK(p==&Io&&!index);*support=0;EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=calloc(1,sizeof(*d));CHECK(d);
  d->Desc=0x8a;d->Len=43;d->AddrSpaceGranularity=32;d->AddrRangeMin=GU_BAR0_BASE;d->AddrRangeMax=0x86ffffff;d->AddrLen=0x1000000;*out=d;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Attributes(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out){
  CHECK(p==&Io);if(op==EfiPciIoAttributeOperationGet){CHECK(out&&!mask);*out=Attrs;return EFI_SUCCESS;}
  if(op==EfiPciIoAttributeOperationSupported){CHECK(out&&!mask);*out=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY;return EFI_SUCCESS;}
  CHECK(op==EfiPciIoAttributeOperationEnable&&mask==(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY)&&!out&&Exclusive);
  Enables++;if(Mode==ATTR_FAIL)return EFI_DEVICE_ERROR;Attrs|=mask;Cfg[1]|=3;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Open(EFI_HANDLE h,EFI_GUID *g,VOID **out,EFI_HANDLE a,EFI_HANDLE controller,UINT32 attrs){
  CHECK(h==H(10)&&Guid(g,&PciGuid)&&a==H(13)&&!controller&&attrs==EFI_OPEN_PROTOCOL_EXCLUSIVE&&Stage==4&&!Exclusive);
  Exclusive=TRUE;*out=&Io;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Close(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE a,EFI_HANDLE controller){
  CHECK(h==H(10)&&Guid(g,&PciGuid)&&a==H(13)&&!controller&&Exclusive);Closes++;
  if(Mode==CLOSE_FAIL)return EFI_DEVICE_ERROR;Exclusive=FALSE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Disconnect(EFI_HANDLE controller,EFI_HANDLE driver,EFI_HANDLE child){
  CHECK(!child&&Stops<4);CHECK(controller==(Stops==3?H(10):H(11))&&driver==Agent(Stops));Stops++;Stage=Stops;
  if(Stage==4){Cfg[1]&=~3u;Attrs=0;}if(Mode==STOP2_FAIL&&Stage==2)return EFI_DEVICE_ERROR;return EFI_SUCCESS;
}
static const UINT32 UltimateConfig[64]={
  0x275710de,0x00100003,0x030000a1,0x00800010,0x86000000,0x0000000c,0x00000060,0x0000000c,
  0x00000064,0x00004001,0,0x34d81043,0x87000000,0x00000060,0,0x000001ff,
  0x34d81043,0,0,0,0,1,0x0023d6ce,0,0x48036801,8,0x00807805,0,0,0,0x0012b410,0x112c8de1,
  0x00092930,0x00456904,0x10810140,0,0,0,0,0x00070813,0x00000400,0x0180001e,0x001e0004,0,0,
  0x01140009,0x06000001,0x00015e00,0,0,0x00050011,0x00b90000,0x00ba0000,0,0,0,0,0,0,0,0,0,0,0};
static VOID Setup(VOID){
  memset(&Gu,0,sizeof(Gu));memset(&Plan,0,sizeof(Plan));memset(&Bs,0,sizeof(Bs));memset(&St,0,sizeof(St));memset(&Io,0,sizeof(Io));
  memcpy(Cfg,UltimateConfig,sizeof(Cfg));Stage=Stops=Enables=Closes=0;Exclusive=Drift=FALSE;StaticOkay=TRUE;NotesUsed=0;Notes[0]=0;
  Bs.LocateHandleBuffer=Locate;Bs.HandleProtocol=Handle;Bs.ProtocolsPerHandle=Protocols;Bs.OpenProtocolInformation=Opens;Bs.FreePool=Free;
  Bs.RaiseTPL=Raise;Bs.RestoreTPL=Restore;Bs.OpenProtocol=Open;Bs.CloseProtocol=Close;Bs.DisconnectController=Disconnect;
  St.BootServices=&Bs;St.ConsoleInHandle=St.ConsoleOutHandle=St.StandardErrorHandle=H(12);St.ConIn=&Keys;St.ConOut=St.StdErr=&Text;
  Keys.ReadKeyStroke=KeyRead;Keys.Reset=KeyReset;Keys.WaitForKey=(EFI_EVENT)1;
  Gop.Mode=&GopMode;GopMode.Info=&GopInfo;GopMode.SizeOfInfo=sizeof(GopInfo);GopMode.FrameBufferBase=0x6000000000ULL;GopMode.FrameBufferSize=0x1900000;
  GopInfo.Version=0;GopInfo.HorizontalResolution=2560;GopInfo.VerticalResolution=1600;
  GopInfo.PixelsPerScanLine=2560;GopInfo.PixelFormat=PixelBlueGreenRedReserved8BitPerColor;
  if(Mode==GOP_SMALL_MODE||Mode==GOP_UNDERSIZED){
    GopInfo.HorizontalResolution=1024;GopInfo.VerticalResolution=600;GopInfo.PixelsPerScanLine=1024;
    GopMode.FrameBufferSize=Mode==GOP_SMALL_MODE?0x258000:0x257000;
  }
  Io.Pci.Read=ConfigRead;Io.GetLocation=Location;Io.GetBarAttributes=GetBar;Io.Attributes=Attributes;Attrs=0x300;
  Gu.bs=&Bs;Gu.pci=&Io;Gu.image=H(13);Gu.handle=H(10);Gu.prom_verified=TRUE;Gu.bar_base=GU_BAR0_BASE;Gu.bar_size=0x1000000;
  for(UINTN i=0;i<4;i++){
    memset(&Bind[i],0,sizeof(Bind[i]));memset(&Images[i],0,sizeof(Images[i]));Bind[i].DriverBindingHandle=Agent(i);
    Bind[i].ImageHandle=i==0?H(30):i==1?H(31):Agent(i);Bind[i].Version=i==3?0x70011:i==2?1:0x10;
    Bind[i].Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(Code[i]+(i==0?0x1008:i==1?0xb5c:i==2?0x14d4:0x6ec));
    Images[i].ImageBase=Code[i];Images[i].ImageSize=i==0?0x5ec0:i==1?0x3be0:i==2?0x9000:0x27ac0;
    Images[i].ImageCodeType=i==2?EfiRuntimeServicesCode:EfiBootServicesCode;
    Images[i].DeviceHandle=i==3?H(10):H(5);Images[i].FilePath=(VOID *)(i==0?OutFile:i==1?GfxFile:i==2?EdidFile:NvFile);
  }
  if(Mode==DRIVER_VERSION)Bind[3].Version++;
  if(Mode==DRIVER_HASH)Code[1][0x2a0]^=1;
  if(Mode==DRIVER_PATH)Images[0].FilePath=(VOID *)GfxFile;
  if(Mode==DRIVER_STOP)Bind[1].Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)(Code[1]+0xb5d);
}
static UINT8 *ReadFile(const char *path,UINTN size){FILE *f=fopen(path,"rb");CHECK(f);UINT8 *p=malloc(size);CHECK(p);
  CHECK(fread(p,1,size,f)==size&&fgetc(f)==EOF);fclose(f);return p;}
int main(int argc,char **argv){
  CHECK(argc==2);static const char *names[]={"IMG10.BIN","IMG13.BIN","IMG05.BIN","IMG62.BIN"};
  static const UINTN sizes[]={0x5ec0,0x3be0,0x9000,0x27ac0};
  for(UINTN i=0;i<4;i++){char path[4096];CHECK(snprintf(path,sizeof(path),"%s/%s",argv[1],names[i])>0);Code[i]=ReadFile(path,sizes[i]);}
  for(Mode=OK;Mode<MODE_COUNT;Mode++){
    Setup();EFI_STATUS s=GuAsusPreflight(&St,H(13),H(10));
    if(Mode==GOP_UNDERSIZED||(Mode>=DRIVER_VERSION&&Mode<=EXTRA_GPU_OWNER)){
      CHECK(s!=EFI_SUCCESS&&!Stops&&!GuAsusMutated());goto done;
    }
    CHECK(s==EFI_SUCCESS&&!Stops&&!GuAsusMutated());
    if(Mode==GRAPH_DRIFT){Drift=TRUE;s=GuAsusRelease();CHECK(s!=EFI_SUCCESS&&!Stops&&!GuAsusMutated());goto done;}
    if(Mode==UNRELATED_GRAPH_DRIFT)Drift=TRUE;
    s=GuAsusRelease();
    if(Mode==STOP2_FAIL){CHECK(s!=EFI_SUCCESS&&Stops==2&&GuAsusMutated());goto done;}
    if(Mode==ATTR_FAIL){CHECK(s!=EFI_SUCCESS&&Stops==4&&GuAsusMutated()&&Exclusive);goto done;}
    CHECK(s==EFI_SUCCESS&&Stops==4&&Enables==1&&Exclusive&&GuAsusReleased());
    if(Mode==CONFIG_DRIFT){Cfg[0x88/4]^=0x10000;CHECK(!GuAsusReleased());Gu.quiesced=TRUE;Gu.flrs=1;CHECK(GuAsusFinish()!=EFI_SUCCESS&&Exclusive);goto done;}
    if(Mode==VENDOR_PAYLOAD_DRIFT){Cfg[0xb8/4]^=2;CHECK(!GuAsusReleased());Gu.quiesced=TRUE;Gu.flrs=1;CHECK(GuAsusFinish()!=EFI_SUCCESS&&Exclusive);goto done;}
    Gu.quiesced=TRUE;Gu.flrs=1;Cfg[0x80/4]&=~0x00090000u;Cfg[0xb8/4]&=~1u;Attrs=0;s=GuAsusFinish();
    if(Mode==CLOSE_FAIL)CHECK(s!=EFI_SUCCESS&&Exclusive&&Closes==1);
    else {
      CHECK(s==EFI_SUCCESS&&!Exclusive&&!Gu.owned&&Closes==1&&Enables==2&&GuAsusReadyForWindows());
      CHECK(strstr(Notes,"NVIDIA vendor status drift tolerated")!=NULL);
    }
done:
    if(Mode==DRIVER_HASH)Code[1][0x2a0]^=1;
    printf("ASUS release case %d PASS stops=%zu mutated=%u\n",Mode,(size_t)Stops,GuAsusMutated());
  }
  for(UINTN i=0;i<4;i++)free(Code[i]);printf("ASUS: %u checks across %u cases; captured code and ownership callbacks simulated.\n",Checks,MODE_COUNT);return 0;
}
