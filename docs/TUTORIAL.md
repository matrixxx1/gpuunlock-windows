# Windows setup and reproduction tutorial

This tutorial explains how to reproduce the pre-Windows unlock without a USB
drive. Read it completely before changing any disk or firmware setting.

The included write profile is only for the captured ASUS G834JYR with RTX 4090
Laptop GPU `10de:2757`, subsystem `1043:34d8`, and VBIOS `95.03.3E.00.0F`.
Even another G834JYR must pass every ROM, PCI, firmware and cold-state check.
Other GPUs and laptop models require a new port; do not weaken a check to make
them pass.

## 1. Understand the boot design

The safe layout has two independent EFI System Partitions:

- the original Windows ESP, containing Microsoft's untouched boot files;
- a dedicated experimental ESP, containing this UEFI application, private
  firmware inputs and run logs.

For initial validation, Windows Boot Manager stays first and the unlock
application is launched only by a one-shot selection. After the exact machine
and both boot paths are validated, the experimental entry may be placed first
persistently only if Windows Boot Manager remains its immediate fallback. The
UEFI application itself always presents a 20-second `U`/`L` override window.

The application loads and hashes Windows Boot Manager before touching the GPU.
On any admission mismatch it releases the GPU claim and starts stock Windows.
After firmware execution, it hands off only if DMA and bus mastering have been
contained and all readbacks pass.

## 2. Back up and record the baseline

Before beginning:

1. Back up important files to another physical device.
2. Create Windows installation/recovery media.
3. If BitLocker/device encryption is enabled, save the recovery key somewhere
   off the PC and suspend protection for boot experiments. Do not publish it.
4. Record BIOS version, Windows build, NVIDIA driver and VBIOS version.
5. Confirm the machine can enter its UEFI setup and one-time boot menu.
6. Record the current firmware order:

```powershell
bcdedit /enum '{fwbootmgr}'
bcdedit /enum '{bootmgr}'
```

The first persistent `displayorder` entry should remain `{bootmgr}` throughout
this procedure.

## 3. Verify the target GPU from Windows

```powershell
nvidia-smi --query-gpu=name,pci.bus_id,pci.device_id,driver_version,vbios_version `
  --format=csv,noheader

Get-PnpDevice -Class Display |
  Select-Object Status,FriendlyName,InstanceId
```

Stop if the NVIDIA device, subsystem, VBIOS or PCI route differs from the profile
documented in this repository. A model name alone is not sufficient.

## 4. Obtain the source and build tools

Install Git, Python 3, LLVM/Clang, and Visual Studio Build Tools with the x64 C++
linker. Then clone:

```powershell
git clone https://github.com/matrixxx1/gpuunlock-windows.git
cd gpuunlock-windows
```

The final repository URL above is the intended public location.

## 5. Generate the external NVIDIA assets

This repository does not redistribute NVIDIA firmware. Generate the pinned
610.43.03 inputs locally using the original project:

```text
https://github.com/JustAResearcher/gpu-unlock-ada
```

On a compatible Linux environment or WSL checkout of that project, run its
documented `make assets` target. The Windows port expects these five files:

```text
booter_load_ad102.bin
v67-ad102-610.43.03-four-plm.bin
gsp_ga10x.bin
gsp_boot_ad102.bin
gsp_boot_desc_ad102.bin
```

The source validates their exact sizes and SHA-256 values in `profile.c`. Do not
substitute files from another driver version or download payloads from an
untrusted binary mirror.

## 6. Capture the board option ROM privately

Use a reputable, read-only VBIOS save function such as GPU-Z's **Save to file**.
Do not use any flash/write operation. Keep the result private; do not commit or
upload it.

The tested GPU-Z container held multiple slots. This port uses the complete
declared four-image PCI option-ROM chain, not merely the shorter firmware-exposed
UEFI copy. For the included profile the expected chain is 666,624 bytes and its
hash is pinned in `ad103m_rom_profile.c`.

If your saved file is a vendor container rather than a raw 666,624-byte chain,
inspect its declared PCI image boundaries and extract the matching complete
chain offline. Do not change the expected hash to an unreviewed ROM. First parse
and review its FWSEC descriptor, fields and signatures, then create a new profile
and negative tests.

Verify the exact supported fixture:

```powershell
.\Test-RomProfile.ps1 -Rom C:\private\complete-optionrom.bin -Clang clang
```

The test is CPU-only and intentionally mutates copies to prove that incorrect
ROMs and FWSEC fields are rejected.

## 7. Create the private boot profile

Create the ignored local file:

```powershell
New-Item -ItemType Directory -Force local | Out-Null
Copy-Item boot_profile.example.h local\boot_profile.h
```

From an elevated PowerShell window, inventory partitions:

```powershell
Get-Partition |
  Select-Object DiskNumber,PartitionNumber,DriveLetter,Guid,GptType,Size |
  Format-Table -AutoSize
```

Put the exact Windows ESP and dedicated experimental ESP GPT GUIDs into
`local\boot_profile.h`. A GUID such as
`01234567-89ab-cdef-0123-456789abcdef` becomes:

```c
{0x01234567,0x89ab,0xcdef,{0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef}}
```

Never copy another person's partition GUIDs. `local/` is ignored by Git.

## 8. Pin the current Microsoft boot manager

The application refuses to hand off to an unexpected Windows loader. Temporarily
mount the Windows ESP from an elevated shell, record the file length and hash,
then unmount it:

```powershell
mountvol S: /S
Get-Item S:\EFI\Microsoft\Boot\bootmgfw.efi | Select-Object Length
Get-FileHash S:\EFI\Microsoft\Boot\bootmgfw.efi -Algorithm SHA256
mountvol S: /D
```

Review and update only the pinned length and SHA-256 in `boot_io.c`, then rerun
all tests. Windows updates can replace this file; a changed hash should stop the
experimental boot until the new Microsoft-signed loader is reviewed and repinned.

## 9. Run the complete host tests

```powershell
.\Build.ps1 -Gcc clang -Ar llvm-ar -RunTests `
  -Prom C:\private\complete-optionrom.bin `
  -Assets C:\private\generated-assets `
  -DriverCapture C:\private\ultimate-driver-capture
```

Expected summaries for the published profile are:

```text
17 admission cases PASS; all hardware operations simulated.
90 real core + compat + UEFI port scenarios passed; all hardware operations simulated.
AD103M unbound ownership, exact config, and partial-enable containment PASS
ASUS: 17840 checks across 16 cases; captured code and ownership callbacks simulated.
app flow: 363 checks; 18 scenarios, all effects simulated.
```

These tests do not access the GPU. A pass proves the modeled control flow, not
that untested hardware is safe.

## 10. Build the UEFI application

```powershell
.\Build-Experimental.ps1 -Gcc clang -Ar llvm-ar
Get-FileHash .\build\AD103M-EXPERIMENTAL.EFI -Algorithm SHA256
```

The build verifies that the result is an x64 EFI application, has no imports,
TLS or CLR metadata, and stays within the audited stack limits.

## 11. Prepare a dedicated internal ESP

Use Windows Disk Management or another trusted partition tool to create a small
FAT32 EFI System Partition in intentionally unallocated space. Do not resize or
format the Windows ESP. Double-check the disk number, partition boundaries and
GPT type before writing anything.

For the experiment, temporarily assign the dedicated ESP a drive letter such as
`U:`. Create this layout:

```text
U:\EFI\BOOT\BOOTX64.EFI
U:\EFI\GPUUnlock\AD103M-EXPERIMENTAL.EFI
U:\AD103M-ASSETS\booter_load_ad102.bin
U:\AD103M-ASSETS\v67-ad102-610.43.03-four-plm.bin
U:\AD103M-ASSETS\gsp_ga10x.bin
U:\AD103M-ASSETS\gsp_boot_ad102.bin
U:\AD103M-ASSETS\gsp_boot_desc_ad102.bin
U:\AD103M-ASSETS\G834JYR-RTX4090L-95.03.3E.00.0F-complete-optionrom.bin
```

Copy the locally built EFI to both EFI paths. Hash every staged file and compare
it with the reviewed local source. Do not place these files on the Windows ESP.

## 12. Register and validate with a one-shot boot

First use the laptop's UEFI one-time boot menu to select the dedicated ESP's
fallback `BOOTX64.EFI`. On the tested ASUS firmware this created a native firmware
application entry for later one-shot selection. Do not make it the default.

Back in Windows, inspect the entries:

```powershell
bcdedit /enum firmware
bcdedit /enum '{fwbootmgr}'
```

After identifying and reviewing the exact experimental entry, an elevated shell
can select it once without changing persistent order:

```powershell
bcdedit /set '{fwbootmgr}' bootsequence '{REVIEWED-EXPERIMENTAL-ENTRY-GUID}'
bcdedit /enum '{fwbootmgr}'
```

Verify that `bootsequence` names only the reviewed entry and persistent
`displayorder` still begins with `{bootmgr}`. Never copy the example placeholder.

Only after successful locked and unlocked trials, `Enable-PersistentSelector.ps1`
can place the reviewed experimental entry first while preserving Windows Boot
Manager immediately behind it. Pass the exact entry GUID and drive letter after
reviewing the output above; do not reuse an example identity.

## 13. Perform the cold/manual run

1. Close applications and save work.
2. Perform a complete shutdown, not Restart:

   ```powershell
   shutdown /s /t 0
   ```

3. Wait at least 15 seconds after power-off.
4. Power on.
5. Read the UEFI application's status screen.
6. During the visible 20-second countdown, press `U` to unlock or `L` for a
   locked normal boot. Either key overrides the saved default for that boot.
   Timeout uses the persistent default. Press `U` only when the exact machine
   profile is expected to pass.
7. Do not retry in the same powered session after any post-firmware failure.

On a mismatch, the expected behavior is refusal followed by normal Windows.

### Optional persistent-default tray control

Run `Install-GpuUnlockControl.ps1` once to create the desktop **GPU Unlock
Control** tray app. Its left-click and right-click context menu can enable or
disable starting the app at Windows sign-in, and set the persistent timeout
default to **Unlock during boot** or **Run Normal**. It remains available from
the system tray near the clock without a full application window. Changing the
default requires elevation because it writes the dedicated experimental ESP.
The UEFI screen still waits 20 seconds on every boot and accepts `U` or `L` as
an override. After changing the default, fully power down, wait at least 15
seconds, then power on for the lock status to change.

Pass the CUDA-enabled Python executable when installing the tray app so its
context menu can report the current measured state without blocking the UI:

```powershell
.\Install-GpuUnlockControl.ps1 -Python C:\path\to\cuda-venv\Scripts\python.exe
```

## 14. Verify the result in Windows

Confirm driver/device health:

```powershell
nvidia-smi
Get-PnpDevice -Class Display |
  Select-Object Status,FriendlyName,InstanceId
```

Copy the newest `AD103M-UNLOCK\RUNxxxx.TXT` from the dedicated ESP and require:

```text
selector result: SS0=0x88888888 ... matched=1
selector result: SS1=0x8 ... matched=1
PCIe FLR returned: result=0
Transaction result=0 safe=1 retained=0 ... DMA_live=0
TRANSACTION APPLIED
```

This line proves the pre-OS transaction, not that Windows retained the unlock.
Require a matched Windows benchmark before reporting the boot as unlocked.

Also confirm the one-shot `bootsequence` is gone. If persistent selection was
enabled, require the reviewed experimental entry first and Windows Boot Manager
immediately second; otherwise require Windows Boot Manager first.

## 15. Measure, do not assume

Run `benchmark.py` before and after under identical power, temperature, model,
batch, context and software conditions. Compare with `compare.py`. A successful
register transaction does not guarantee the same gain for every workload.

## 16. Rollback

The GPU change itself is volatile: a normal restart or power cycle removes it.
To stop using the experiment, restore Windows Boot Manager to first place and do
not arm the experimental entry again. You may later remove the experimental
firmware entry and dedicated files after recording their exact identities; never
delete or format a partition based only on a drive letter.

## Porting to another system

For a different GPU, VBIOS, laptop or desktop board, this tutorial stops before
the write-capable build. A responsible port requires a read-only observer first:

1. capture exact PCI configuration, BAR resources, ROM identity, cold MMIO,
   selectors, WPR geometry, display ownership and UEFI protocol graph;
2. parse and pin the complete board ROM and FWSEC descriptor;
3. derive memory layout and firmware selection from evidence, not a nearby SKU;
4. add negative tests for every captured identity and state;
5. simulate allocation, DMA, firmware, FLR, cleanup and every injected failure;
6. prove Windows handoff and boot rollback independently;
7. use a manual one-shot trial with no automatic retry.

If you cannot explain every changed constant and rejection test, do not execute
the transaction on that system. Open an issue with sanitized inventory instead;
never upload ROMs, firmware binaries, partition GUIDs, recovery keys or passwords.
