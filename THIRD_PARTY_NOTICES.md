# Third-party notices

## Original Ada unlock project

The transaction core is derived from
[JustAResearcher/gpu-unlock-ada](https://github.com/JustAResearcher/gpu-unlock-ada),
commit `d28c485418c3d9d034cd4bac19200bc33dd3453b`, GPL-2.0-only. The original
project contains the RTX 4090 research, Linux implementation, reports and
evidence that made this Windows/UEFI port possible. Its authorship and SPDX
headers are preserved. The GPLv2 text is reproduced in `LICENSE`.

That upstream project itself preserves notices for earlier GPU-unlock work,
including Zagitta/gpu-unlock and related NVIDIA-derived source. Those file-level
notices remain in this source tree and the corresponding license texts remain in
`LICENSES/`.

## Windows UEFI reference

The UEFI control-flow and containment scaffold is derived from
[impacifist/Windows-GPU-UNLOCK](https://github.com/impacifist/Windows-GPU-UNLOCK),
commit `9352abd27480203ea146f766e7c96957912d148a`, GPL-2.0-only.

## TianoCore EDK II

TianoCore EDK II headers are from commit
`c1415771ac592ad8509351c46cd3cda4f38d7d52`, under
BSD-2-Clause-Patent. See `vendor/edk2/License.txt` and
`vendor/edk2/headers.json`. Original file notices are preserved.

## External NVIDIA inputs

NVIDIA firmware files, generated payload binaries and option-ROM captures are
external inputs with their own terms. They are not included in this repository.
Hashes and format descriptions do not grant redistribution rights.
