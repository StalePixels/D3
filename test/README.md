D3 tests
=

`test/run.sh` builds the commands with `make all`, then runs each scenario in MAME's Next driver through the pinesy harness. It prints PASS or FAIL for each scenario and exits with 1 when any fails. A failing scenario shows the harness output under its name.

```
test/run.sh                    # build, then every scenario
test/run.sh ini untar          # only the named scenarios
```

Every scenario boots NextZXOS from a clone of the harness's SD image, as a real Next does, and runs its commands from an autoexec. The checks read the machine, not the picture. A text check looks for the printed text in the ULA screen. A file check compares a file on the SD image with a file in `data/` by SHA-256.

## Environment

`run.sh` stops with a message when a needed variable is missing.

| Variable | What it is |
|---|---|
| `PINESY_HARNESS` | The pinesy-harness folder, the one that holds `run.py`. |
| `D3_TEST_IMAGES` | A folder for the second SD card images, the harness's disk images and the run logs. Use a folder that backups and indexing skip: the images are tens of megabytes. |
| `NBN_SERVER` | The NBN PersonalServer folder (`NBNtools/server`), built with `npm install` and `npm run build`. Needed only by the scenarios that use the server. |
| `MAME` | Optional. The Pinesy binary. Without it the harness uses the Pinesy binary of the host. |
| `D3_DOT_DIR` | Optional. A folder with `INI.DOT`, `EVERY.DOT` and the other commands to test, in place of `DEV-D3`. With it set, `run.sh` does not run `make all`. The telnet scenarios also need the build's map file in it, as `tmp/TELNET.map`. |

The tools it calls: `make` and Docker (for `make all`), `python3`, `gzip`, `txt2bas`, `timeout`, `mtools` and `mkfs.vfat` for the scenarios with a second SD card, and `node`, `npm` and `lsof` for the scenarios that use the server.

Run logs of the last run are in `$D3_TEST_IMAGES/runs/<scenario>/`. A scenario that fails keeps its disk image until the scenario runs again.

## What each scenario checks

Each scenario is a template in `scenarios/`. `run.sh` fills in the paths, writes the scenario to a temporary folder and runs it. Each one has an `ON ERROR` line that prints `ERR` and the error code, the command, and a line that prints `AFTER`. `AFTER` on the screen shows that the command returned to BASIC. The `POKE 23692,255` before each `PRINT` stops the ROM from waiting at `scroll?` when a command fills the screen.

Each command runs with the ROM's scroll count (SCR_CT, 23692) set to 7, and the line after it prints `SCR` and the value it reads back. `SCR 7` on the screen shows that the command put SCR_CT back as it was. `xcopy-err` prints it from its error trap.

| Scenario | What it checks |
|---|---|
| `ini` | `.INI` reads a key, sets a new key and reads it back. The file holds the new key. |
| `ini-lastline` | `.INI` finds the key on a last line that has no newline, and keeps that line when it sets a new key. |
| `every` | `.EVERY 3` sets NextReg 0x7F on every third run only, and the count in `/SYS/EVERY.CFG` starts again after it. |
| `untar` | `.UNTAR` extracts a tar file with a folder, a file, an empty file and a symlink. The file contents match. |
| `untar-existing` | `.UNTAR` on the same tar file twice returns no error when the folders already exist. |
| `untar-examples` | `.UNTAR` extracts a tar file that starts with a `./` entry for the current folder, and returns no error. The file contents match. |
| `untar-filefirst` | `.UNTAR` extracts a tar file that lists a file before its folders. It makes the folders for the file, returns no error, and the file contents match. |
| `untar-scroll` | `.UNTAR` on `d3.tar`, with the screen full and SCR_CT at 1, as after a typed command. Its lines never stop at `scroll?`, and it returns to BASIC with SCR_CT at 1. |
| `untar-nospace` | `.UNTAR` on `d3.tar` to the no-space second SD card (D:). The write fails when the card fills. `Failed write` is the last line UNTAR prints, it returns an error, and the line after it does not run. The error trap reads that last line back from the screen with `SCREEN$` and prints it after `LAST`. |
| `ch8show` | `.CH8SHOW` shows its heading for a font file and returns to BASIC. |
| `inkey` | `.INKEY` shows the code of a held key, and returns to BASIC on code 3 (EXTEND with C). |
| `less` | `.LESS` opens a text file, and `q` returns to BASIC. |
| `less-break` | `.LESS` opens a text file, and BREAK returns to BASIC. LESS waits for the key to go up, so the next line runs, and NextRegs 0x56 and 0x57 (MMU6 and MMU7) read the same as before the command. |
| `gunzip` | `.GUNZIP` on a file that `gzip` made on the Mac gives the original, byte for byte. |
| `telnet` | `.TELNET` opens a telnet link through the ESP to an echo server. Typed keys reach the server as the right bytes (BREAK as ESC, EXTEND + B as code 2), EDIT opens the menu in the status bar and another key closes it, a block of data comes back whole, and EDIT, then Q closes the link and puts the machine back. See below. |
| `telnet-banner` | `.TELNET` to a server that sends telnet commands and text as the link opens. The text shows and the commands do not, the server gets the refusals, and a cursor key sends ESC O A once the server has set DECCKM. See below. |
| `telnet-badhost` | `.TELNET` to a host name that does not resolve puts the machine back and returns error 255 with the ESP's message, `DNS Fail`. See below. |
| `telnet-pi` | `.TELNET -pi`, with Layer 2 and sprites on before it starts, takes over the machine with the Pi's UART and puts it back. Typed keys go out as their bytes, with no AT commands. See below. |
| `telnet-err` | `.TELNET` with a port number out of range shows its help and returns error 255, and the line after it does not run. |
| `telnet-term` | The terminal: ANSI streams give the right characters, attributes, cursor positions, scrolls and replies. See below. |
| `telnet-term-speed` | The terminal takes bytes faster than 115,200 baud brings them, and a long stream at that rate leaves the receive ring empty. See below. |
| `xcopy-local` | `.XCOPY` copies the second SD card (D:) to a folder on the boot card. Every file without a space in its path matches the fixture, and D: is unchanged. |
| `xcopy-scroll` | `.XCOPY -ehc` copies D: to the boot card, with the screen full and SCR_CT at 1. Its lines never stop at `scroll?`, and it returns to BASIC with SCR_CT at 1. |
| `xcopy-progress` | `.XCOPY` copies a file of 70001 bytes from D: to the boot card. The count after the file's name redraws in place and ends as `68K/68K`, and the copy matches the fixture. |
| `xcopy-progress-mb` | `.XCOPY` copies a file of 1500000 bytes from D: to the boot card. The count goes from K to M, where `1022K/1.4M` becomes the shorter `1.0M/1.4M`, and ends as `1.4M/1.4M`. The line after the command reads the count's row back with `SCREEN$` and prints it in brackets, so a character left over from a longer count shows before the `]`. |
| `xcopy-err` | `.XCOPY` with an option it does not have returns error 255, and the line after it does not run. |
| `crash-detect` | A program that resets the machine fails its scenario. The scenario passes when the run fails. |
| `xcopy-up` | `.XCOPY` copies the whole second SD card to a PersonalServer folder. The folder equals the fixture, and D: is unchanged. |
| `xcopy-down` | `.XCOPY` copies a PersonalServer folder to the empty second SD card. The card equals the fixture. |
| `xcopy-progress-up` | `.XCOPY` copies a file of 70001 bytes from D: to a PersonalServer folder. The count ends as `68K/68K`. |
| `xcopy-progress-down` | `.XCOPY` copies a file of 70001 bytes from a PersonalServer folder to the empty second SD card. The count ends as `68K/68K`. |

`telnet`, `telnet-banner`, `telnet-badhost` and `telnet-pi` check from inside MAME, with `tools/telnet-probe.lua` (`PINESY_HARNESS_PROBE`, mode in `TELNET_PROBE_MODE`). The probe finds the addresses it needs in the map file of the build. It takes the state of the machine at the moment `.TELNET` starts its takeover and again when it has put everything back: NextRegs (Layer 2 and sprites among them), the tilemap clip window, the UART ports, the ULA and tilemap palettes, all of bank 5, and the I register and interrupt mode. The two must match. While the session runs it checks the registers, the interrupt vector table, the font and the palette, and it logs every byte the program writes to the UART and reads from it. Then it types keys with the harness and reads the screen; the top of `tools/telnet-probe.lua` says what each mode does. In `telnet` a block of data comes back from the echo server with the frame interrupt off, so only the UART's "Rx near full" interrupt can move it. In every mode that opens a session it checks the status bar on display row 28: the connection and `EDIT menu`. Each mode ends with EDIT (CAPS SHIFT and 1), then Q. Its results are in `telnet-probe.txt` in the run folder, and a scenario passes only when every line there passed. For `telnet` and `telnet-banner` the probe also writes `telnet-server.hex`, the bytes the server must have got, and `run.sh` checks the server's log against it with `tools/telnet-serverlog.py`: one connection, closed by the client, and exactly those bytes. The scenarios that use the ESP run at real speed.

`telnet-term` and `telnet-term-speed` run `.TELNET -pi` with `tools/telnet-term-probe.lua`, which also reads the map file. Nothing arrives on the Pi's UART, so the probe writes ANSI streams straight into the receive ring, as the interrupts would, and the main loop passes them to the terminal. In `telnet-term` each test starts with ESC c, and the probe checks the tilemap cells and the terminal's variables: colours, cursor moves, erase, tab stops, deferred wrap, the full-screen scroll (the map ring and NextReg $31) with the status bar still on display row 28, scroll regions, origin mode, the DSR and DA replies in the key queue, the character sets, DECSC and DECRC, the modes, the cursor blink, and unknown sequences. `telnet-term-speed` writes to `telnet-probe.txt` how many bytes a frame the terminal takes for plain text, for scrolling lines, and for a colour change on every character. Then it feeds scrolling lines at the rate of 115,200 baud for several seconds, and checks that the ring stays near empty and the status bar is still on display row 28. Both press EDIT, then Q at the end, and pass only when every line in `telnet-probe.txt` passed. They have no network and run with `-nothrottle`.

`xcopy-up`, `xcopy-down`, `xcopy-progress-up` and `xcopy-progress-down` use one PersonalServer that `run.sh` starts on a free port of `127.0.0.1`, and run at the same time. They run at real speed, because Next network software and its server are built around the Next's clock. The other scenarios have no network and run with `-nothrottle`.

## Test data

`data/` holds what the scenarios put on the SD image or compare against.

- `arkward.tar` and `arkward/` are a tar file and what it holds. `examples.tar` and `examples/` are the same for three NextBASIC programs. `filefirst.tar` and `filefirst/` are the same for one file, listed in the tar before its folders. `d3.tar` is a larger tar file, used as the original for `gunzip` and by `untar-scroll`.
- `ini/` holds the INI files and the results that `.INI` must leave. `every.cfg` is the `EVERY.CFG` that `every` must leave.
- `test.ch8` is a 768-byte font. `logo.txt` is a short text file.
- `fixture/` is the file tree for the XCOPY scenarios: folders five levels deep, long names with spaces, names that start with a dot, hidden, system and read-only entries, files of 0, 4096, 4097, 16385 and 70001 bytes. Git keeps no empty folders, so `tools/cards.py` makes the two empty ones that it names in `EMPTY_DIRS`. Names that fit 8.3 are upper case in the tree, because NextZXOS reports such names in upper case whatever case flag the card holds.

## The second SD card

The XCOPY scenarios and `untar-nospace` put a second card on `-hard2`. `tools/cards.py` builds 64 MiB cards in `$D3_TEST_IMAGES/xcopy-cards/`: one with the fixture on it, one empty, one with a single file, `FILLER.BIN`, that leaves 512 KiB free, and one with a single file, `BIG.BIN`, of 1500000 bytes. It builds them when they are missing, and again when the fixture changes. Each run clones the card with `cp -c`, and deletes the clone when the scenario passes. The card is an MBR with one FAT32 partition, as the boot card is, and `cards.py` checks that the full card reads back as the fixture before it uses it.

`cards.py` also lists the differences between two folders. The XCOPY scenarios use it to compare a server folder or a card with the fixture.
