# How the Ada unlock was applied before Windows

## Starting point

The core transaction comes from JustAResearcher's
[gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada) at commit
`d28c485418c3d9d034cd4bac19200bc33dd3453b`. That project demonstrated the Ada
speed-selector override in Linux before the stock NVIDIA driver attached.

Windows cannot run that Linux kernel module before `nvlddmkm.sys`, so this port
moves the same bounded transaction into a UEFI application that executes before
Windows Boot Manager. The UEFI application then chainloads the untouched
Microsoft loader.

The UEFI control-flow and containment scaffold also draws from
[impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK)
at commit `9352abd27480203ea146f766e7c96957912d148a`.

## Machine profile captured for this port

- ASUS ROG Strix SCAR 18 G834JYR
- RTX 4090 Laptop GPU `10de:2757`
- subsystem `1043:34d8`
- PCI function `0000:01:00.0`
- VBIOS `95.03.3E.00.0F`
- 16 MiB BAR0 at the captured address
- Intel integrated GPU owns the boot display/GOP

The complete option-ROM bytes and local GPT partition identities remain private.
The public source contains hashes and geometry needed to reject the wrong input,
not the copyrighted binary inputs themselves.

## Porting work

### 1. Linux compatibility layer

The upstream transaction sources are compiled against a small compatibility
layer implementing the Linux types, allocation, delays, firmware loading, PCI,
MMIO and DMA calls needed by this code. Each hardware operation is routed to
checked UEFI services rather than a Windows kernel driver.

### 2. Exact fail-closed admission

Before a transaction is armed, the application validates:

- exact GPU, subsystem, class/revision and PCI location;
- exact 256-byte PCI configuration and capability layout;
- exact BAR base and size;
- exact firmware-exposed option-ROM hash;
- exact private complete-ROM length/hash and parsed FWSEC fields;
- hashes and lengths of all five external payload assets;
- cold MMIO register profile and entry selector values;
- UEFI ownership graph and an exclusive claim on the discrete GPU;
- pinned Windows boot-manager size and SHA-256.

Static asset and loader mismatches occur before the operator prompt. Ultimate
topology and loaded-driver mismatches occur after `U` but before the first
detach, and still return to normal Windows. Any uncertainty after the first
detach halts and requires a full power-off. Admission checks are intentionally
board-specific.

### 3. Ultimate-mode display ownership

The ASUS MUX is set to Ultimate/dGPU-only, so the NVIDIA GOP is the active boot
display. Before any detach, the application verifies the exact GPU and GOP device
paths, framebuffer geometry, console relationships, and four driver-binding
images by loaded-image path, version, image size, stop RVA, and executable-code
SHA-256. It snapshots the complete active protocol graph and fails closed on
protected graph drift.

Only after all read-only preflight checks pass does it stop the physical GOP text
owner, graphics owner, and NVIDIA GOP owner in that order. It then opens the GPU
PCI I/O protocol exclusively, restores only the captured I/O and memory-decode
attributes, keeps bus mastering off, and validates the released cold state. A
failure after the first detach halts and requires a full power-off.

### 4. Manual, bounded transaction

The application stages the verified Windows loader first and waits for a manual
`U` keypress. It then allocates/maps bounded DMA buffers, enables bus mastering,
runs the pinned FWSEC and Booter payloads, verifies permission changes and the
payload marker, and writes:

- `SS0 = 0x88888888`
- `SS1 = 0x00000008`

There are no automatic retries in the same powered session.

### 5. Reset, containment and Windows handoff

After selector readback, the application performs exactly one PCIe FLR. It waits
for the GPU to return, verifies the selectors and effective readout survived,
quiesces the firmware engines, releases every DMA mapping, clears bus mastering,
restores the original PCI command state, and releases the exclusive claim.

Only after those checks pass does it start the already-loaded, hash-pinned
Microsoft Windows Boot Manager. A failure after firmware execution halts instead
of risking a handoff with retained DMA.

## Deployment model

The experiment used a dedicated FAT32 EFI System Partition and an existing native
firmware application entry. The experimental EFI and its locally generated assets
were placed only on that partition. The normal Windows ESP and Microsoft files
were not modified.

Windows Boot Manager remained first in the persistent firmware display order. A
one-shot `BootNext` selection launched the experimental entry once. After a full
shutdown and cold power-on, the operator pressed `U`. Firmware consumed the
one-shot selection, so subsequent boots defaulted to ordinary Windows.

This repository does not publish the machine-specific staging scripts because
they contain disk boundaries and partition identities that must never be copied
to another system.

## Transaction evidence and Windows proof

The hardware log recorded:

```text
Operator U: exact profile accepted; no retries
selector result: SS0=0x88888888 expected=0x88888888 matched=1
selector result: SS1=0x8 expected=0x8 matched=1
PCIe FLR returned: result=0
Transaction result=0 safe=1 retained=0 started=1 quiesced=1 flrs=1 DMA_live=0
TRANSACTION APPLIED: volatile readbacks passed and DMA released
```

These lines establish only the pre-OS transaction and recovery path. They do not
prove that Windows retained the unlocked state. Require a matched Windows
benchmark under identical power, temperature, model, batch, context, and software
conditions before reporting an unlocked result. The repository targets one
captured laptop and is not evidence that another model is safe.

## What is deliberately not changed

- No VBIOS flash or persistent GPU firmware write.
- No Windows NVIDIA driver patch.
- No replacement of `bootmgfw.efi`.
- No permanent firmware-order change.
- No embedded passwords, recovery keys or credentials.
- No redistribution of private ROM captures or NVIDIA payload binaries.
