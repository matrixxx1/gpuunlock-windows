# gpuunlock-windows

Experimental Windows/UEFI port of the Ada GPU speed-selector unlock for one
ASUS ROG Strix SCAR 18 G834JYR with an RTX 4090 Laptop GPU (`10de:2757`,
subsystem `1043:34d8`). It attempts the volatile unlock before Windows starts and then
hands control to the original Microsoft Windows Boot Manager.

This work exists because of **JustAResearcher's original
[gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada)** project.
The transaction core in this repository is derived from upstream commit
[`d28c485`](https://github.com/JustAResearcher/gpu-unlock-ada/commit/d28c485418c3d9d034cd4bac19200bc33dd3453b).
Please credit and consult that project for the original RTX 4090 research,
Linux implementation, technical explanation, and evidence.

## What this Windows port does

The Linux transaction was adapted to a freestanding x64 UEFI application:

1. Load and hash a pinned copy of the current Microsoft Windows boot manager.
2. Locate exactly one matching NVIDIA GPU and verify its PCI identity, BAR,
   option ROM, FWSEC geometry, and five payload assets.
3. Always show a 20-second selector: `U` unlocks, `L` starts normal Windows,
   and timeout uses the saved persistent default.
4. In ASUS Ultimate/dGPU-only mode, pin the active physical GOP, console graph,
   and four loaded driver images by path, version, size, stop RVA, and executable
   code hash; then release only those exact owners in a fixed order.
5. Claim the GPU exclusively, restore only its captured I/O and memory-decode
   attributes with bus mastering off, and verify the released cold register state.
6. Enable bus mastering only for bounded firmware DMA, run the pinned FWSEC and
   Booter transaction, and write the volatile speed selectors.
7. Perform one PCIe Function Level Reset, verify the unlocked readback, release
   all DMA, disable bus mastering, restore the PCI command state, and start the
   pinned Microsoft boot manager.

The implementation does **not** flash the VBIOS, update GPU firmware, patch the
Windows NVIDIA driver, or replace Windows Boot Manager. The unlock is volatile
and is lost after the next restart or power cycle unless the UEFI application is
run again.

The current build displays a high-contrast banner as soon as the UEFI loader
starts, shows an explicit `UNLOCK attempt` or `NORMAL` selection, and holds the
pre-OS transaction result on screen before handing off to Windows. `Status-GpuUnlock.ps1` can run a
short matched CUDA measurement after sign-in and report the current boot as
unlocked, locked, or indeterminate. This is a performance-based verification;
Windows does not directly read the protected selector registers.

Install the optional per-user logon notification with the CUDA-enabled Python
environment that already runs `benchmark.py`:

```powershell
.\Install-StatusNotifier.ps1 -Python C:\path\to\cuda-venv\Scripts\python.exe
```

On the validated machine, `Install-GpuUnlockControl.ps1` creates a desktop
system-tray control near the clock, with one-click actions to start or stop the
app at Windows sign-in and set the persistent boot default to **Unlock during
boot** or **Run Normal**. Both left-click and right-click open the compact context menu;
there is no full application window. The menu asynchronously measures and shows
the current boot as `LOCKED`, `UNLOCKED`, `INDETERMINATE`, or unable to verify,
and includes a manual refresh action.
The setting writes a fixed persistent default to the dedicated experimental ESP.
Every boot still provides the full 20-second `U`/`L` override window. After
changing the default, fully power down, wait at least 15 seconds, and power on
again for the lock status to change. A missing or malformed setting defaults to
normal Windows.

See [docs/WINDOWS-PORT.md](docs/WINDOWS-PORT.md) for the complete porting and
hardware-validation record.

For a practical walkthrough, see the
[Windows setup and reproduction tutorial](docs/TUTORIAL.md).

## Validation boundaries

The host suite exercises admission, firmware/DMA sequencing, FLR recovery,
captured ASUS driver ownership, PCI-state restoration, cleanup, and application
handoff with all hardware effects simulated. The EFI verifier separately checks
the produced image format and imports.

A pre-OS log can establish that the volatile transaction ran, the selector
readbacks matched, FLR completed, PCI state was restored, DMA was released, and
Windows handoff began. It does **not** establish that Windows retained the
unlocked state. Require a matched Windows benchmark under identical conditions
before reporting an unlocked result.

This repository targets one captured machine and is not broad hardware
qualification. Do not remove or widen an admission check to make another card
run.

## Safety and support scope

This repository is research software with real pre-OS GPU write capability.
An incorrect port can hang the machine, prevent that boot from reaching Windows,
or require a full power-off. It is not a universal RTX 4090 installer.

- The write-capable build is restricted to the exact captured G834JYR profile.
- Secure Boot must be disabled for an unsigned local EFI build.
- Use one-shot firmware selection until the exact machine has been validated.
- If enabling the selector persistently afterward, keep Windows Boot Manager as
  the immediate fallback entry and verify both paths before relying on it.
- Keep Windows recovery media and any required BitLocker recovery key available.
- Never stage this as the only boot path or overwrite Microsoft's EFI files.
- Never reuse the included constants for another board without new captures,
  negative tests, and a fresh fail-closed profile.

## Source-only publication

The repository intentionally excludes:

- NVIDIA firmware and generated payload binaries;
- VBIOS/option-ROM captures;
- EFI executables and other build output;
- partition GUIDs, raw boot logs, and machine-specific staging receipts;
- passwords, recovery keys, API tokens, and other credentials.

Those exclusions are enforced by `.gitignore`. Required firmware inputs must be
obtained and generated separately under their applicable licenses.

## Build outline

The tested Windows toolchain used Clang/LLVM for compilation and the Microsoft
x64 linker for the EFI application.

```powershell
# Create a private, ignored machine configuration.
New-Item -ItemType Directory -Force local | Out-Null
Copy-Item boot_profile.example.h local\boot_profile.h
# Edit local\boot_profile.h with the two reviewed ESP partition GUIDs.

# Verify the privately captured complete option-ROM chain.
.\Test-RomProfile.ps1 -Rom C:\private\complete-optionrom.bin

# Run the simulated admission, transaction, ownership and containment tests.
.\Build.ps1 -Gcc clang -Ar llvm-ar -RunTests `
  -Prom C:\private\complete-optionrom.bin `
  -Assets C:\private\generated-assets `
  -DriverCapture C:\private\ultimate-driver-capture

# Build the write-capable UEFI application only after all checks pass.
.\Build-Experimental.ps1 -Gcc clang -Ar llvm-ar
```

The resulting local artifact is `build/AD103M-EXPERIMENTAL.EFI`. This project
does not provide an automatic installer. Review the source and the deployment
rules in [docs/WINDOWS-PORT.md](docs/WINDOWS-PORT.md) before any hardware use.

## Benchmarking

`benchmark.py` is an ordinary Windows CUDA/cuBLAS workload. It does not access
PCI configuration, boot settings, or GPU firmware. Capture a stock baseline,
perform a cold/manual unlock, repeat the identical workload, and compare with
`compare.py`. Treat application-level results separately from instruction-level
throughput claims.

## License and attribution

GPL-2.0-only. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Copyright and license notices
from JustAResearcher, earlier GPU-unlock work, the Windows UEFI reference, NVIDIA
source files, and TianoCore EDK II are preserved.
