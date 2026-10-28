/* SPDX-License-Identifier: GPL-2.0-only */
/* Dedicated internal ESP log/assets. Windows ESP is opened read-only. */
#include "boot_io.h"
#include "boot_profile.h"
#include "compat/compat.h"
#include "format.h"
#include "profile.h"
#include "sha256.h"
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

static EFI_SYSTEM_TABLE *Table;
static EFI_HANDLE Self,Windows,OwnVolume;
static EFI_FILE_PROTOCOL *Root,*Log;
static BOOLEAN Silent;
static EFI_GUID LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID,FsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,InfoGuid=EFI_FILE_INFO_ID;
static const EFI_GUID WindowsPartition=GU_WINDOWS_PARTITION_INITIALIZER;
static const EFI_GUID InternalPartition=GU_INTERNAL_PARTITION_INITIALIZER;
static const CHAR16 WindowsName[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";

static BOOLEAN Inspect(const EFI_DEVICE_PATH_PROTOCOL *p,UINTN *size,BOOLEAN *internal,BOOLEAN *windows) {
  UINTN at=0,hd=0;*internal=FALSE;*windows=FALSE;
  if(!p)return FALSE;
  for(UINTN nodes=0;nodes<64&&at+4<=4096;nodes++) {
    UINTN n=p->Length[0]+((UINTN)p->Length[1]<<8);
    if(n<4||n>4096-at)return FALSE;
    if(p->Type==END_DEVICE_PATH_TYPE) {
      if(n!=4||p->SubType!=END_ENTIRE_DEVICE_PATH_SUBTYPE)return FALSE;
      *size=at+4;return TRUE;
    }
    if(p->Type==MEDIA_DEVICE_PATH&&p->SubType==MEDIA_HARDDRIVE_DP) {
      if(n!=sizeof(HARDDRIVE_DEVICE_PATH)||++hd!=1)return FALSE;
      const HARDDRIVE_DEVICE_PATH *d=(const VOID *)p;
      if(d->MBRType==2&&d->SignatureType==2&&d->PartitionSize) {
        if(d->PartitionNumber==1&&!memcmp(d->Signature,&WindowsPartition,16))*windows=TRUE;
        if(d->PartitionNumber==4&&!memcmp(d->Signature,&InternalPartition,16))*internal=TRUE;
      }
    }
    at+=n;p=(const VOID *)((const UINT8 *)p+n);
  }
  return FALSE;
}
VOID GuBootSay(const CHAR16 *text) {
  if(!Silent&&Table&&Table->ConOut&&Table->ConOut->OutputString)
    Table->ConOut->OutputString(Table->ConOut,(CHAR16 *)text);
}
static VOID GuBootPause(UINTN seconds) {
  if(!Table||!Table->BootServices||!Table->BootServices->Stall)return;
  for(UINTN i=0;i<seconds;i++)(VOID)Table->BootServices->Stall(1000000);
}
VOID GuBootBanner(VOID) {
  if(!Table||!Table->ConOut)return;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out=Table->ConOut;
  if(out->Reset)(VOID)out->Reset(out,FALSE);
  if(out->ClearScreen)(VOID)out->ClearScreen(out);
  if(out->SetAttribute)(VOID)out->SetAttribute(out,EFI_LIGHTCYAN|EFI_BACKGROUND_BLUE);
  GuBootSay(L"\r\n===============================================================\r\n"
            L"       GPU UNLOCK WINDOWS - PRE-BOOT LOADER IS RUNNING          \r\n"
            L"===============================================================\r\n\r\n"
            L"Preparing exact-machine safety checks. Please wait.\r\n"
            L"You will be asked to attempt UNLOCK or run NORMAL Windows.\r\n\r\n");
  if(out->SetAttribute)(VOID)out->SetAttribute(out,EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
}
VOID GuBootResult(BOOLEAN unlocked) {
  if(!Table||!Table->ConOut)return;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out=Table->ConOut;
  if(out->ClearScreen)(VOID)out->ClearScreen(out);
  if(out->SetAttribute)(VOID)out->SetAttribute(out,
      (unlocked?EFI_LIGHTGREEN:EFI_LIGHTRED)|EFI_BACKGROUND_BLACK);
  GuBootSay(unlocked?
    L"\r\n*** GPU TRANSACTION APPLIED - WINDOWS VERIFICATION PENDING ***\r\n":
    L"\r\n*** NORMAL BOOT SELECTED - GPU TRANSACTION NOT APPLIED ***\r\n");
  if(out->SetAttribute)(VOID)out->SetAttribute(out,EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
  GuBootSay(L"\r\nThe Windows status notifier will verify performance after sign-in.\r\n");
  GuBootPause(3);
}
VOID GuBootSilence(VOID){Silent=TRUE;}
VOID GuLog(const char *format,...) {
  CHAR8 buffer[2048];va_list args;va_start(args,format);
  size_t n=GuFormatV(buffer,sizeof(buffer),format,args);va_end(args);
  if(n==(size_t)-1||n>=sizeof(buffer)||!Log){Gu.fault=TRUE;return;}
  UINTN bytes=n;EFI_STATUS s=Log->Write(Log,&bytes,buffer);
  if(s!=EFI_SUCCESS||bytes!=n||Log->Flush(Log)!=EFI_SUCCESS)Gu.fault=TRUE;
}
VOID GuLogText(const char *text){GuLog("%s",text);}
BOOLEAN GuBootLogOpen(VOID){return Log!=NULL;}
EFI_STATUS GuBootOpen(EFI_HANDLE image,EFI_SYSTEM_TABLE *t) {
  if(Table)return EFI_ALREADY_STARTED;
  if(!image||!t||!t->BootServices)return EFI_INVALID_PARAMETER;
  Table=t;Self=image;
  EFI_BOOT_SERVICES *b=t->BootServices;EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_DEVICE_PATH_PROTOCOL *path=NULL;
  EFI_STATUS s=b->HandleProtocol(image,&LoadedGuid,(VOID **)&loaded);
  if(s!=EFI_SUCCESS||!loaded||!loaded->DeviceHandle)return EFI_UNSUPPORTED;
  OwnVolume=loaded->DeviceHandle;s=b->HandleProtocol(OwnVolume,&PathGuid,(VOID **)&path);
  UINTN length=0;BOOLEAN internal=FALSE,windows=FALSE;
  if(s!=EFI_SUCCESS||!Inspect(path,&length,&internal,&windows)||!internal||windows)return EFI_ACCESS_DENIED;
  s=b->HandleProtocol(OwnVolume,&FsGuid,(VOID **)&fs);if(s!=EFI_SUCCESS||!fs)return EFI_UNSUPPORTED;
  s=fs->OpenVolume(fs,&Root);if(s!=EFI_SUCCESS||!Root)return EFI_DEVICE_ERROR;
  EFI_FILE_PROTOCOL *dir=NULL;
  s=Root->Open(Root,&dir,L"\\AD103M-UNLOCK",EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,EFI_FILE_DIRECTORY);
  if(s!=EFI_SUCCESS||!dir)return EFI_DEVICE_ERROR;
  for(UINTN slot=1;slot<=9999;slot++) {
    CHAR16 name[]=L"RUN0000.TXT";UINTN number=slot;
    for(UINTN at=7;at>3;){at--;name[at]=(CHAR16)('0'+number%10);number/=10;}
    EFI_FILE_PROTOCOL *existing=NULL;s=dir->Open(dir,&existing,name,EFI_FILE_MODE_READ,0);
    if(s==EFI_SUCCESS&&existing){s=existing->Close(existing);if(s!=EFI_SUCCESS)break;continue;}
    if(s!=EFI_NOT_FOUND)break;
    s=dir->Open(dir,&Log,name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);break;
  }
  EFI_STATUS closed=dir->Close(dir);if(s!=EFI_SUCCESS||!Log)return s==EFI_SUCCESS?EFI_OUT_OF_RESOURCES:s;
  if(closed!=EFI_SUCCESS)return closed;
  if(!b->SetWatchdogTimer)return EFI_UNSUPPORTED;
  return b->SetWatchdogTimer(0,0,0,NULL);
}
UINT8 GuBootReadDefault(VOID) {
  static const CHAR16 name[]=L"\\AD103M-UNLOCK\\BOOT-DEFAULT";
  static const CHAR8 unlock_token[]="GPUUNLOCK-WINDOWS-DEFAULT-UNLOCK-V1";
  static const CHAR8 normal_token[]="GPUUNLOCK-WINDOWS-DEFAULT-NORMAL-V1";
  if(!Root||Gu.started)return GU_BOOT_DEFAULT_NORMAL;
  EFI_FILE_PROTOCOL *f=NULL;
  EFI_STATUS s=Root->Open(Root,&f,(CHAR16 *)name,EFI_FILE_MODE_READ,0);
  if(s==EFI_NOT_FOUND)return GU_BOOT_DEFAULT_NORMAL;
  if(s!=EFI_SUCCESS||!f){GuLog("Persistent boot default open failed: status=%#llx\n",(unsigned long long)s);return GU_BOOT_DEFAULT_NORMAL;}
  CHAR8 data[sizeof(unlock_token)]={0};UINTN bytes=sizeof(data);
  s=f->Read(f,&bytes,data);
  EFI_STATUS closed=f->Close(f);
  UINT8 request=GU_BOOT_DEFAULT_NORMAL;
  if(s==EFI_SUCCESS&&bytes==sizeof(unlock_token)-1&&!memcmp(data,unlock_token,sizeof(unlock_token)-1))
    request=GU_BOOT_DEFAULT_UNLOCK;
  else if(!(s==EFI_SUCCESS&&bytes==sizeof(normal_token)-1&&!memcmp(data,normal_token,sizeof(normal_token)-1)))
    GuLog("Persistent boot default invalid; using normal boot\n");
  GuLog("Persistent boot default: unlock=%u read_status=%#llx close_status=%#llx bytes=%zu\n",
    request,(unsigned long long)s,(unsigned long long)closed,(size_t)bytes);
  if(closed!=EFI_SUCCESS)Gu.fault=TRUE;
  return request;
}
static EFI_STATUS ReadFile(EFI_FILE_PROTOCOL *root,const CHAR16 *name,UINTN expected,VOID **data) {
  *data=NULL;EFI_FILE_PROTOCOL *f=NULL;UINT64 info_words[128];UINTN info_bytes=sizeof(info_words);
  EFI_STATUS s=root->Open(root,&f,(CHAR16 *)name,EFI_FILE_MODE_READ,0);
  if(s!=EFI_SUCCESS||!f)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
  s=f->GetInfo(f,&InfoGuid,&info_bytes,info_words);
  EFI_FILE_INFO *info=(VOID *)info_words;
  if(s==EFI_SUCCESS&&(info_bytes<sizeof(EFI_FILE_INFO)||info_bytes>sizeof(info_words)||
     info->FileSize!=expected||(info->Attribute&EFI_FILE_DIRECTORY)))s=EFI_COMPROMISED_DATA;
  VOID *p=NULL;
  if(s==EFI_SUCCESS){p=GuAlloc(expected);if(!p)s=EFI_OUT_OF_RESOURCES;}
  for(UINTN at=0;s==EFI_SUCCESS&&at<expected;) {
    UINTN n=expected-at;if(n>1048576)n=1048576;UINTN wanted=n;
    s=f->Read(f,&n,(UINT8 *)p+at);if(s==EFI_SUCCESS&&n!=wanted)s=EFI_DEVICE_ERROR;at+=n;
  }
  if(s==EFI_SUCCESS){UINT8 tail;UINTN n=1;s=f->Read(f,&n,&tail);if(s==EFI_SUCCESS&&n)s=EFI_COMPROMISED_DATA;}
  EFI_STATUS closed=f->Close(f);if(s==EFI_SUCCESS)s=closed;
  if(s!=EFI_SUCCESS){GuFree(p);return s;}*data=p;return EFI_SUCCESS;
}
EFI_STATUS GuBootLoadAssets(VOID) {
  static const CHAR16 *names[]={L"\\AD103M-ASSETS\\booter_load_ad102.bin",L"\\AD103M-ASSETS\\v67-ad102-610.43.03-four-plm.bin",
    L"\\AD103M-ASSETS\\gsp_ga10x.bin",L"\\AD103M-ASSETS\\gsp_boot_ad102.bin",L"\\AD103M-ASSETS\\gsp_boot_desc_ad102.bin"};
  static const char *full[]={"gpu-unlock/610.43.03/booter_load_ad102.bin","gpu-unlock/610.43.03/v67-ad102-610.43.03-four-plm.bin",
    "gpu-unlock/610.43.03/gsp_ga10x.bin","gpu-unlock/610.43.03/gsp_boot_ad102.bin","gpu-unlock/610.43.03/gsp_boot_desc_ad102.bin"};
  static const UINTN sizes[]={57672,64000,84277400,36864,84};
  if(!Root||Gu.started)return EFI_NOT_READY;
  for(UINTN i=0;i<5;i++) {
    VOID *p=NULL;EFI_STATUS s=ReadFile(Root,names[i],sizes[i],&p);
    if(s!=EFI_SUCCESS)return s;
    GuAssets[i].name=full[i];GuAssets[i].firmware=(struct firmware){sizes[i],p};
  }
  return GuAd103mAssetsValid()?EFI_SUCCESS:EFI_CRC_ERROR;
}
EFI_STATUS GuBootLoadOptionRom(VOID **data,UINTN *bytes) {
  if(!data||!bytes||!Root||Gu.started)return EFI_INVALID_PARAMETER;
  *bytes=666624;
  EFI_STATUS s=ReadFile(Root,L"\\AD103M-ASSETS\\G834JYR-RTX4090L-95.03.3E.00.0F-complete-optionrom.bin",*bytes,data);
  if(EFI_ERROR(s))*bytes=0;
  return s;
}
EFI_STATUS GuBootLoadWindows(VOID) {
  if(Windows||!Table||Gu.started)return EFI_NOT_READY;
  EFI_BOOT_SERVICES *b=Table->BootServices;EFI_HANDLE *h=NULL,volume=NULL;UINTN count=0,matches=0,path_bytes=0;
  EFI_DEVICE_PATH_PROTOCOL *base=NULL;
  EFI_STATUS s=b->LocateHandleBuffer(ByProtocol,&FsGuid,NULL,&count,&h);
  if(s==EFI_SUCCESS&&(!h||!count||count>128))s=EFI_UNSUPPORTED;
  for(UINTN i=0;s==EFI_SUCCESS&&i<count;i++) {
    EFI_DEVICE_PATH_PROTOCOL *p=NULL;UINTN bytes=0;BOOLEAN internal=FALSE,windows=FALSE;
    if(b->HandleProtocol(h[i],&PathGuid,(VOID **)&p)==EFI_SUCCESS&&Inspect(p,&bytes,&internal,&windows)&&windows&&!internal) {
      matches++;volume=h[i];base=p;path_bytes=bytes;
    }
  }
  if(h)b->FreePool(h);
  if(s!=EFI_SUCCESS||matches!=1||volume==OwnVolume)return EFI_NOT_FOUND;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_FILE_PROTOCOL *root=NULL;
  s=b->HandleProtocol(volume,&FsGuid,(VOID **)&fs);
  if(s==EFI_SUCCESS&&fs)s=fs->OpenVolume(fs,&root);else s=EFI_UNSUPPORTED;
  if(s!=EFI_SUCCESS||!root)return EFI_DEVICE_ERROR;
  VOID *bytes=NULL;s=ReadFile(root,WindowsName,3086680,&bytes);
  EFI_STATUS closed=root->Close(root);if(s==EFI_SUCCESS)s=closed;
  if(s==EFI_SUCCESS&&!GuHashMatches(bytes,3086680,"2d60a2203865122dad22772136b3d905167d9dcf926d602bb5f48b3b6668abec"))s=EFI_CRC_ERROR;
  UINTN node_bytes=4+sizeof(WindowsName),total=path_bytes-4+node_bytes+4;
  UINT8 *path=NULL;
  if(s==EFI_SUCCESS){path=GuAlloc(total);if(!path)s=EFI_OUT_OF_RESOURCES;}
  if(s==EFI_SUCCESS) {
    memcpy(path,base,path_bytes-4);UINT8 *node=path+path_bytes-4;
    node[0]=MEDIA_DEVICE_PATH;node[1]=MEDIA_FILEPATH_DP;node[2]=(UINT8)node_bytes;node[3]=(UINT8)(node_bytes>>8);
    memcpy(node+4,WindowsName,sizeof(WindowsName));node+=node_bytes;
    node[0]=END_DEVICE_PATH_TYPE;node[1]=END_ENTIRE_DEVICE_PATH_SUBTYPE;node[2]=4;node[3]=0;
    s=b->LoadImage(FALSE,Self,(VOID *)path,bytes,3086680,&Windows);
  }
  GuFree(bytes);GuFree(path);
  if(s==EFI_SUCCESS) {
    EFI_LOADED_IMAGE_PROTOCOL *im=NULL;s=b->HandleProtocol(Windows,&LoadedGuid,(VOID **)&im);
    if(s==EFI_SUCCESS&&(!im||im->DeviceHandle!=volume))s=EFI_UNSUPPORTED;
  }
  GuLog("Windows image staged: status=%#llx matches=%zu SHA256 pinned\n",(unsigned long long)s,(size_t)matches);
  return s;
}
EFI_STATUS GuBootChoose(BOOLEAN *unlock,BOOLEAN default_unlock) {
  if(!unlock||!Table||!Table->ConIn||!Table->ConIn->ReadKeyStroke)return EFI_UNSUPPORTED;
  *unlock=FALSE;
  if(Table->ConOut&&Table->ConOut->ClearScreen)(VOID)Table->ConOut->ClearScreen(Table->ConOut);
  if(Table->ConOut&&Table->ConOut->SetAttribute)
    (VOID)Table->ConOut->SetAttribute(Table->ConOut,EFI_YELLOW|EFI_BACKGROUND_BLUE);
  GuBootSay(L"\r\n===============================================================\r\n"
            L"              CHOOSE THIS WINDOWS BOOT MODE                    \r\n"
            L"===============================================================\r\n\r\n"
            L"  Press U once:  UNLOCK GPU, then start Windows\r\n"
            L"  Press L:       keep GPU LOCKED, then start normal Windows\r\n\r\n"
            L"  The saved default is used after 20 seconds.\r\n"
            L"  Press U or L to override the saved default for this boot.\r\n"
            L"  Do not repeatedly press U; one keypress is sufficient.\r\n\r\n");
  if(Table->ConOut&&Table->ConOut->SetAttribute)
    (VOID)Table->ConOut->SetAttribute(Table->ConOut,EFI_LIGHTGRAY|EFI_BACKGROUND_BLACK);
  GuBootSay(default_unlock?L"  SAVED DEFAULT: UNLOCK DURING BOOT\r\n\r\n":
                           L"  SAVED DEFAULT: RUN NORMAL\r\n\r\n");
  for(UINTN seconds=20;seconds>0;seconds--) {
    CHAR16 line[]=L"\rTime remaining: 20 seconds  ";
    line[17]=(CHAR16)('0'+seconds/10);line[18]=(CHAR16)('0'+seconds%10);
    GuBootSay(line);
    for(UINTN tick=0;tick<10;tick++) {
      EFI_INPUT_KEY key;EFI_STATUS s=Table->ConIn->ReadKeyStroke(Table->ConIn,&key);
      if(s==EFI_SUCCESS) {
        GuLog("Boot selector key: scan=%#x unicode=%#x\n",key.ScanCode,key.UnicodeChar);
        if(key.UnicodeChar=='u'||key.UnicodeChar=='U'){
          *unlock=TRUE;GuBootSay(L"\r\nUNLOCK attempt selected. Running the guarded transaction...\r\n");return EFI_SUCCESS;
        }
        if(key.ScanCode==0x17||key.UnicodeChar==27||key.UnicodeChar=='l'||key.UnicodeChar=='L'){
          GuBootSay(L"\r\nLOCKED boot selected. Starting normal Windows...\r\n");return EFI_SUCCESS;
        }
      } else if(s!=EFI_NOT_READY)return s;
      s=Table->BootServices->Stall(100000);if(s!=EFI_SUCCESS)return s;
    }
  }
  *unlock=default_unlock;
  GuBootSay(default_unlock?
    L"\r\nTimed out: saved UNLOCK default selected. Running the transaction...\r\n":
    L"\r\nTimed out: saved NORMAL default selected. Starting normal Windows...\r\n");
  return EFI_SUCCESS;
}
EFI_STATUS GuBootClose(VOID) {
  EFI_STATUS s=EFI_SUCCESS;
  if(Log){s=Log->Flush(Log);EFI_STATUS closed=Log->Close(Log);Log=NULL;if(s==EFI_SUCCESS)s=closed;}
  if(Root){EFI_STATUS closed=Root->Close(Root);Root=NULL;if(s==EFI_SUCCESS)s=closed;}
  return s;
}
EFI_STATUS GuBootWindows(VOID) {
  if(!Windows)return EFI_NOT_READY;
  return Table->BootServices->StartImage(Windows,NULL,NULL);
}
VOID GuBootHalt(VOID) {
  GuBootSay(L"\r\nSTOPPED. Power OFF fully; next boot use Windows Boot Manager.\r\n");
  for(;;)__asm__ volatile("pause");
}
