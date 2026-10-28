/* SPDX-License-Identifier: GPL-2.0-only */
/* Real application entry; dependency effects simulated and order asserted. */
#include "../app.c"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
static unsigned Checks,Case,Calls,Releases,Transactions,Finishes,WindowsCalls,Closes,Frees;
static BOOLEAN Mutated,Silenced;static jmp_buf Escape;static UINT8 Rom[16];
static EFI_PCI_IO_PROTOCOL Io;static EFI_BOOT_SERVICES Bs;static EFI_RUNTIME_SERVICES Rs;static EFI_SYSTEM_TABLE St;
GU_PLATFORM Gu;
#define CHECK(x) do{Checks++;if(!(x)){fprintf(stderr,"app FAIL case=%u line=%d %s\n",Case,__LINE__,#x);exit(1);}}while(0)
static EFI_STATUS Tick(unsigned n){CHECK(++Calls==n);return Case==n?EFI_DEVICE_ERROR:EFI_SUCCESS;}
VOID GuInit(EFI_BOOT_SERVICES *b,EFI_PCI_IO_PROTOCOL *p){memset(&Gu,0,sizeof(Gu));Gu.bs=b;Gu.pci=p;}
VOID GuLog(const char *f,...){(void)f;}VOID GuLogText(const char *f){(void)f;}
BOOLEAN GuCheckConfig(const UINT32 c[64],UINTN s,UINTN b,UINTN d,UINTN f,GU_CAPS *caps){
  CHECK(c[0]==GU_BOARD_ID&&!s&&b==1&&!d&&!f);*caps=(GU_CAPS){0x60,0x78,0x68,0,2};return TRUE;}
static EFI_STATUS EFIAPI GetVariable(CHAR16 *name,EFI_GUID *guid,UINT32 *attrs,UINTN *n,VOID *v){
  (void)name;(void)guid;*attrs=0;*n=1;*(UINT8 *)v=Case==99?1:0;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE kind,EFI_GUID *guid,VOID *key,UINTN *n,EFI_HANDLE **out){
  (void)kind;(void)guid;(void)key;*n=1;*out=malloc(sizeof(**out));CHECK(*out);(*out)[0]=(EFI_HANDLE)2;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *guid,VOID **p){(void)guid;CHECK(h==(EFI_HANDLE)2);*p=&Io;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI FreePool(VOID *p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 off,UINTN n,VOID *out){
  CHECK(p==&Io&&w==EfiPciIoWidthUint32&&!off&&(n==1||n==64));memset(out,0,n*4);UINT32 *c=out;c[0]=GU_BOARD_ID;
  if(n==64){c[1]=3;c[2]=0x030000a1;c[4]=GU_BAR0_BASE;c[5]=0xc;c[6]=0x60;c[7]=0xc;c[8]=0x64;c[9]=0x4001;c[11]=GU_BOARD_SUBSYSTEM;}return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){(void)p;*s=*d=*f=0;*b=1;return EFI_SUCCESS;}
EFI_STATUS GuBootOpen(EFI_HANDLE i,EFI_SYSTEM_TABLE *t){CHECK(i==(EFI_HANDLE)1&&t==&St);return Tick(1);}
VOID GuBootBanner(VOID){}UINT8 GuBootReadDefault(VOID){return GU_BOOT_DEFAULT_UNLOCK;}
EFI_STATUS GuBootLoadAssets(VOID){return Tick(2);}
EFI_STATUS GuBootLoadOptionRom(VOID **data,UINTN *bytes){EFI_STATUS s=Tick(3);*data=Rom;*bytes=sizeof(Rom);return s;}
EFI_STATUS GuAd103mPrepareStatic(const VOID *data,UINTN bytes){CHECK(data==Rom&&bytes==sizeof(Rom));EFI_STATUS s=Tick(4);if(s==EFI_SUCCESS)Gu.prom_verified=TRUE;return s;}
EFI_STATUS GuBootLoadWindows(VOID){return Tick(5);}
EFI_STATUS GuBootChoose(BOOLEAN *u,BOOLEAN def){CHECK(def);EFI_STATUS s=Tick(6);*u=Case!=0;return s;}
BOOLEAN GuAd103mAlreadyUnlocked(VOID){return Case==101;}
EFI_STATUS GuAd103mPrepareReleased(VOID){CHECK(!Mutated&&!Gu.owned);EFI_STATUS s=Tick(7);if(s==EFI_SUCCESS)Gu.profile_verified=TRUE;return s;}
EFI_STATUS GuAsusPreflight(EFI_SYSTEM_TABLE *t,EFI_HANDLE self,EFI_HANDLE pci){
  CHECK(t==&St&&self==(EFI_HANDLE)1&&pci==(EFI_HANDLE)2&&Gu.prom_verified&&Gu.profile_verified);return Tick(8);}
VOID GuBootSay(const CHAR16 *s){(void)s;CHECK(!Silenced);}VOID GuBootSilence(VOID){CHECK(Calls==8);Silenced=TRUE;}
EFI_STATUS GuAsusRelease(VOID){CHECK(Silenced);Releases++;Mutated=TRUE;Gu.owned=TRUE;return Tick(9);}
BOOLEAN GuAsusReleased(VOID){return Mutated&&Gu.owned;}BOOLEAN GuAd103mColdEntry(VOID){return Gu.profile_verified;}
int gpu_unlock_610_load_shared(struct pci_dev *d,struct gpu_unlock_610_shared *shared){
  CHECK(d->device==0x2757&&Gu.owned&&Gu.armed&&Gu.display_ok);(void)shared;return Tick(10)==EFI_SUCCESS?0:-EIO;}
int gpu_unlock_610_unlock(struct pci_dev *d,const struct gpu_unlock_610_shared *s){
  (void)d;(void)s;Transactions++;Gu.started=TRUE;Gu.quiesced=TRUE;Gu.flrs=1;return Tick(11)==EFI_SUCCESS?0:-EIO;}
void gpu_unlock_610_release_shared(struct gpu_unlock_610_shared *s){(void)s;}BOOLEAN GuSafeToReturn(VOID){return TRUE;}
EFI_STATUS GuAsusFinish(VOID){CHECK(Transactions&&Gu.quiesced);Finishes++;EFI_STATUS s=Tick(12);if(s==EFI_SUCCESS)Gu.owned=FALSE;return s;}
VOID GuFree(VOID *p){if(p){CHECK(p==Rom);Frees++;}}
EFI_STATUS GuClearMaster(VOID){CHECK(Mutated);return EFI_SUCCESS;}BOOLEAN GuAsusMutated(VOID){return Mutated;}
VOID GuBootResult(BOOLEAN success){if(success)CHECK((Transactions&&Finishes)||Case==101);}
BOOLEAN GuAsusReadyForWindows(VOID){CHECK(Closes==1&&!Gu.owned&&Gu.quiesced);return Case!=14;}
EFI_STATUS GuBootClose(VOID){Closes++;return Case==13?EFI_DEVICE_ERROR:EFI_SUCCESS;}
EFI_STATUS GuBootWindows(VOID){WindowsCalls++;return EFI_ABORTED;}
VOID GuBootHalt(VOID){CHECK(Mutated);longjmp(Escape,1);}
static VOID Reset(VOID){
  Calls=Releases=Transactions=Finishes=WindowsCalls=Closes=Frees=0;Mutated=Silenced=FALSE;
  memset(&Gu,0,sizeof(Gu));memset(&Bs,0,sizeof(Bs));memset(&Rs,0,sizeof(Rs));memset(&St,0,sizeof(St));memset(&Io,0,sizeof(Io));
  Bs.LocateHandleBuffer=Locate;Bs.HandleProtocol=Handle;Bs.FreePool=FreePool;Rs.GetVariable=GetVariable;St.BootServices=&Bs;St.RuntimeServices=&Rs;
  Io.Pci.Read=Read;Io.GetLocation=Location;
}
int main(void){
  for(volatile unsigned iteration=0;iteration<16;iteration++){
    Case=iteration==15?99:iteration;Reset();if(!setjmp(Escape))(VOID)UefiMain((EFI_HANDLE)1,&St);
    if(Case==99)CHECK(!Calls&&!WindowsCalls);else if(Case==0)CHECK(WindowsCalls==1&&!Mutated&&Frees==1);
    else if(Case==1)CHECK(!WindowsCalls&&!Mutated);else if(Case<=8)CHECK(WindowsCalls==1&&!Mutated);
    else CHECK(!WindowsCalls&&Mutated);
    CHECK(Releases<=1&&Transactions<=1&&Finishes<=1&&WindowsCalls<=1);printf("app flow case %u PASS\n",Case);
  }
  Case=100;Reset();if(!setjmp(Escape))(VOID)UefiMain((EFI_HANDLE)1,&St);
  CHECK(WindowsCalls==1&&Releases==1&&Transactions==1&&Finishes==1&&Closes==1&&Frees==1);
  Case=101;Reset();if(!setjmp(Escape))(VOID)UefiMain((EFI_HANDLE)1,&St);
  CHECK(WindowsCalls==1&&!Mutated&&!Releases&&!Transactions&&!Finishes&&Closes==1&&Frees==1);
  printf("app flow: %u checks; 18 scenarios, all effects simulated.\n",Checks);return 0;
}
