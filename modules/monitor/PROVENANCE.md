# Woz monitor provenance

Status as investigated on 2026-09-07: ancestry identified; applicable upstream
licensing permission for this adaptation remains unresolved. This document does
not grant a license or assert that the original code is public domain.

The root MIT license explicitly excludes the entire `modules/monitor/` directory.

`src/wozmon.s` is an ARM Thumb adaptation closely following Steve Wozniak's
Apple-1 monitor (1976). The parser structure, examine/store state, and labels
including NOTCR, BLSKIP, NEXTITEM, and NEXTHEX correspond to the original listing.
This is an assessment from comparing the code, not proof of the exact source
listing used to create the port. No original source URL or license notice was
present in the local file, and the repository had no commits to trace its import.

References:

- [SB-Projects annotated original listing](https://www.sbprojects.net/projects/apple1/wozmon.php)
  credits Steve Wozniak, 1976. It establishes a comparison reference, not the
  immediate provenance or licensing of this ARM port.
- [WIRED, 2003-09-08](https://www.wired.com/2003/09/woz-oks-apple-i-resurrection/)
  reports Wozniak's permission for Vince Briel to use the Apple-1 ROM and his
  earlier free distribution of the code. The reported permission was addressed
  to that project; it does not establish a general MIT grant for this port.

Local adaptations include 32-bit addresses, ARM calling conventions, callback I/O,
and explicit workspace offsets. Their existence alone does not settle the rights
in the underlying adapted code. No MIT, Apache, or public-domain designation has
been assigned to this module. Its C++ adapter and configuration are maintained
alongside it, without claiming they originated in the historical monitor.

Remaining provenance work: identify the listing used for the initial port and
any applicable permission or license covering adaptation and redistribution.
Module separation and Kconfig exclusion do not resolve source licensing.
