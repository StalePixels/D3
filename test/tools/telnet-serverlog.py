#!/usr/bin/env python3
"""Checks the log of a pinesy-harness server after a telnet scenario.

    telnet-serverlog.py LOG HEX

The server must have had one connection, which the client closed, and the
bytes it received, all chunks together, must be HEX exactly (bytes in hex,
separated by spaces). Prints what is wrong and exits with 1, or exits
with 0.
"""

import re
import sys


def main():
    log, want = sys.argv[1], bytes.fromhex(sys.argv[2])
    got, connects, closes = b'', 0, 0
    with open(log) as f:
        for line in f:
            m = re.search(r' recv \d+ bytes hex=([0-9a-f ]*) ', line)
            if m:
                got += bytes.fromhex(m.group(1))
            elif re.search(r' connect #\d', line):
                connects += 1
            elif re.search(r'(?<!server) close #\d', line):
                closes += 1
    bad = []
    if connects != 1:
        bad.append(f'{connects} connections, want 1')
    if closes != 1:
        bad.append(f'{closes} closes by the client, want 1')
    if got != want:
        bad.append(f'received {got.hex(" ")}, want {want.hex(" ")}')
    for b in bad:
        print(f'{log}: {b}')
    sys.exit(1 if bad else 0)


main()
