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
| `NBN_SERVER` | The NBN PersonalServer folder (`NBNtools/server`), built with `npm install` and `npm run build`. Needed only when `xcopy-up` or `xcopy-down` runs. |
| `MAME` | Optional. The Pinesy binary. Without it the harness uses the Pinesy binary of the host. |
| `D3_DOT_DIR` | Optional. A folder with `INI.DOT`, `EVERY.DOT` and the other commands to test, in place of `DEV-D3`. With it set, `run.sh` does not run `make all`. |

The tools it calls: `make` and Docker (for `make all`), `python3`, `gzip`, `txt2bas`, `timeout`, and for the XCOPY scenarios `mtools`, `mkfs.vfat`, `node`, `npm` and `lsof`.

Run logs of the last run are in `$D3_TEST_IMAGES/runs/<scenario>/`. A scenario that fails keeps its disk image until the scenario runs again.

## What each scenario checks

Each scenario is a template in `scenarios/`. `run.sh` fills in the paths, writes the scenario to a temporary folder and runs it. Each one has an `ON ERROR` line that prints `ERR` and the error code, the command, and a line that prints `AFTER`. `AFTER` on the screen shows that the command returned to BASIC. The `POKE 23692,255` before each `PRINT` stops the ROM from waiting at `scroll?` when a command fills the screen.

| Scenario | What it checks |
|---|---|
| `ini` | `.INI` reads a key, sets a new key and reads it back. The file holds the new key. |
| `ini-lastline` | `.INI` finds the key on a last line that has no newline, and keeps that line when it sets a new key. |
| `every` | `.EVERY 3` sets NextReg 0x7F on every third run only, and the count in `/SYS/EVERY.CFG` starts again after it. |
| `untar` | `.UNTAR` extracts a tar file with a folder, a file, an empty file and a symlink. The file contents match. |
| `untar-existing` | `.UNTAR` on the same tar file twice returns no error when the folders already exist. |
| `ch8show` | `.CH8SHOW` shows its heading for a font file and returns to BASIC. |
| `inkey` | `.INKEY` shows the code of a held key, and returns to BASIC on code 3 (EXTEND with C). |
| `less` | `.LESS` opens a text file, and `q` returns to BASIC. |
| `gunzip` | `.GUNZIP` on a file that `gzip` made on the Mac gives the original, byte for byte. |
| `xcopy-local` | `.XCOPY` copies the second SD card (D:) to a folder on the boot card. Every file without a space in its path matches the fixture, and D: is unchanged. |
| `xcopy-err` | `.XCOPY` with an option it does not have returns error 255, and the line after it does not run. |
| `crash-detect` | A program that resets the machine fails its scenario. The scenario passes when the run fails. |
| `xcopy-up` | `.XCOPY` copies the whole second SD card to a PersonalServer folder. The folder equals the fixture, and D: is unchanged. |
| `xcopy-down` | `.XCOPY` copies a PersonalServer folder to the empty second SD card. The card equals the fixture. |

`xcopy-up` and `xcopy-down` use one PersonalServer that `run.sh` starts on a free port of `127.0.0.1`, and run at the same time. They run at real speed, because Next network software and its server are built around the Next's clock. The other scenarios have no network and run with `-nothrottle`.

## Test data

`data/` holds what the scenarios put on the SD image or compare against.

- `arkward.tar` and `arkward/` are a tar file and what it holds. `examples.tar` and `examples/` are the same for three NextBASIC programs. `d3.tar` is a larger tar file, used as the original for `gunzip`.
- `ini/` holds the INI files and the results that `.INI` must leave. `every.cfg` is the `EVERY.CFG` that `every` must leave.
- `test.ch8` is a 768-byte font. `logo.txt` is a short text file.
- `fixture/` is the file tree for the XCOPY scenarios: folders five levels deep, long names with spaces, names that start with a dot, hidden, system and read-only entries, files of 0, 4096, 4097, 16385 and 70001 bytes. Git keeps no empty folders, so `tools/cards.py` makes the two empty ones that it names in `EMPTY_DIRS`. Names that fit 8.3 are upper case in the tree, because NextZXOS reports such names in upper case whatever case flag the card holds.

## The second SD card

The XCOPY scenarios put a second card on `-hard2`. `tools/cards.py` builds two 64 MiB cards in `$D3_TEST_IMAGES/xcopy-cards/`: one with the fixture on it and one empty. It builds them when they are missing, and again when the fixture changes. Each run clones the card with `cp -c`, and deletes the clone when the scenario passes. The card is an MBR with one FAT32 partition, as the boot card is, and `cards.py` checks that the full card reads back as the fixture before it uses it.

`cards.py` also lists the differences between two folders. The XCOPY scenarios use it to compare a server folder or a card with the fixture.
