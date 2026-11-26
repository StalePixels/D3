D's Developer Dotcommands
=

Some simple commands I use to help me develop.
Best taken with https://github.com/StalePixels/SPUI

.INI - Command-line INI editor for simple, programmable edits to INI or INI style config files.

.EVERY - Job scheduler.  Manages named jobs and will set NextRex 0x7F if the schedule is due.

.UNTAR - POSIX compliant ESXDOS version of UNTAR, super easy to embed in other apps.

.CH8SHOW - View 768b/1k 1bit font files in ROM/UDG format inspired by Damien Guard's zx-origins project.

.INKEY - View keycodes as returned for keys & combos from z88dk built for lazy developers who need a handy guide.

.LESS - Less is not more. A plain text pager with extra features, a compliment, not replacement, for MORE  - thanks Marco.

.XCOPY - Copy files and whole directory trees, keeping long file names. Spiritual sibling of DOS XCOPY.

.GUNZIP - Decompress a gzip file and keep the original. FILE.gz gives FILE, and FILE.tgz gives FILE.tar for .UNTAR.

.TELNET - Telnet client on the ESP WiFi module, with an 80x24 ANSI terminal on the tilemap. It takes over the screen and puts everything back when you leave.
`.TELNET host [port]` connects, on port 23 when there is no port. `.TELNET -pi` is an ANSI terminal on the Pi UART instead.
EDIT opens the menu, and Q in the menu leaves. BREAK sends ESC.

Notable Changes
-
* V 0.1.0         First stable public, with INI
* V 0.2.0         Second stable public, with EVERY
* V 0.3.0         Third stable public, with UNTAR
* V 0.4.0         Fourth stable public, with CH8SHOW
* V 0.4.2         Minor stable public, with INKEY
* V 0.5.0         Fifth stable public, with LESS
* V 0.6.0         Sixth stable public, with XCOPY and GUNZIP




