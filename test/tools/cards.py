#!/usr/bin/env python3
"""Second SD card and fixture tools for the XCOPY scenarios.

usage:
  cards.py build IMAGES_DIR          make the full, empty, no-space and big cards when
                                     missing, print their paths, one to a line, in
                                     that order
  cards.py materialise OUT           copy the fixture to OUT, with its empty folders
  cards.py extract IMAGE OUT         copy the files of a card to OUT
  cards.py compare EXPECTED ACTUAL   list every difference; exit 1 when there is one
  cards.py checks REMOTE_DIR         print a harness "check" line for each fixture
                                     file whose path has no space

The no-space card is an empty card with one file, FILLER.BIN, that leaves
NOSPACE_FREE bytes free. The big card holds one file, BIG.BIN, of BIG_SIZE bytes.

The fixture is the folder data/fixture next to this tool's folder. Git keeps no
empty folders, so EMPTY_DIRS names the ones to make. Needs mtools and mkfs.vfat.
"""
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(HERE, '..', 'data', 'fixture')
SECOND_MIB = 64
SECOND_LBA = 2048
NOSPACE_FREE = 512 * 1024
# Over 1 MiB, so a copy's count goes from K to M
BIG_SIZE = 1500000
ENV = dict(os.environ, MTOOLS_SKIP_CHECK='1')

# Attributes the card holds. A server folder cannot hold them, so no scenario compares them.
ATTRS = {
    'HIDDEN.TXT': 'h',
    'SYSTEM.DAT': 's',
    'hidden-system.dat': 'hs',
    'READONLY.TXT': 'r',
    'hiddendir': 'h',
}
EMPTY_DIRS = ['EMPTYDIR', 'A/B/C/D/E/empty-in-deep']


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, env=ENV)
    if r.returncode:
        sys.exit('failed: %s\n%s%s' % (' '.join(cmd), r.stdout, r.stderr))
    return r.stdout


def files(root):
    """Every path below root, with its kind and, for a file, size and SHA-256."""
    out = {}
    for d, dirs, names in os.walk(root):
        dirs[:] = [x for x in dirs if x != '.DS_Store']
        rel = os.path.relpath(d, root)
        if rel != '.':
            out[rel] = ('dir',)
        for n in names:
            if n == '.DS_Store':
                continue
            p = os.path.join(d, n)
            with open(p, 'rb') as f:
                data = f.read()
            out[os.path.relpath(p, root)] = ('file', len(data), hashlib.sha256(data).hexdigest())
    return out


def digest():
    h = hashlib.sha256(repr((sorted(files(FIXTURE).items()), sorted(ATTRS.items()),
                             EMPTY_DIRS, SECOND_MIB, NOSPACE_FREE, BIG_SIZE)).encode())
    return h.hexdigest()[:12]


def materialise(out):
    if os.path.isdir(out):
        shutil.rmtree(out)
    shutil.copytree(FIXTURE, out, ignore=shutil.ignore_patterns('.DS_Store'))
    for d in EMPTY_DIRS:
        os.makedirs(os.path.join(out, d), exist_ok=True)


def offset(image):
    with open(image, 'rb') as f:
        mbr = f.read(512)
    return struct.unpack('<I', mbr[446 + 8:446 + 12])[0] * 512


def spec(image):
    return '%s@@%d' % (image, offset(image))


def blank_card(path):
    """A 64 MiB card: MBR with one FAT32 LBA partition, as the boot card has.
    That size is 2^17 blocks, which the SD card model accepts."""
    total = SECOND_MIB * 1024 * 1024 // 512
    mbr = bytearray(512)
    mbr[446:462] = struct.pack('<B3sB3sII', 0, b'\0\0\0', 0x0C, b'\xfe\xff\xff',
                               SECOND_LBA, total - SECOND_LBA)
    mbr[510:512] = b'\x55\xaa'
    with open(path, 'wb') as f:
        f.truncate(total * 512)
        f.write(mbr)
    run(['mkfs.vfat', '-F', '32', '-s', '1', '-n', 'D3TEST', '--offset', str(SECOND_LBA),
         path, str((total - SECOND_LBA) // 2)])


def fill_card(path, tree):
    s = spec(path)
    for name in sorted(os.listdir(tree)):
        run(['mcopy', '-i', s, '-s', '-n', '-Q', os.path.join(tree, name), '::/'])
    for d in EMPTY_DIRS:
        # mcopy -s copies an empty folder or not, depending on the mtools version
        if subprocess.run(['mdir', '-i', s, '-b', '::/' + d], capture_output=True, env=ENV).returncode:
            run(['mmd', '-i', s, '::/' + d])
    for p, flags in ATTRS.items():
        run(['mattrib', '-i', s] + ['+' + f for f in flags] + ['::/' + p])


def free_bytes(path):
    last = run(['mdir', '-i', spec(path), '::/']).strip().splitlines()[-1]
    if 'bytes free' not in last:
        sys.exit('no free space count from mdir: %s' % last)
    return int(re.sub(r'[^0-9]', '', last))


def nospace_card(path, folder):
    blank_card(path)
    filler = os.path.join(folder, 'FILLER.BIN')
    with open(filler, 'wb') as f:
        f.truncate(free_bytes(path) - NOSPACE_FREE)
    try:
        run(['mcopy', '-i', spec(path), '-n', '-Q', filler, '::/'])
    finally:
        os.remove(filler)
    left = free_bytes(path)
    if left > NOSPACE_FREE:
        sys.exit('the no-space card has %d bytes free, more than %d' % (left, NOSPACE_FREE))


def big_card(path, folder):
    blank_card(path)
    big = os.path.join(folder, 'BIG.BIN')
    with open(big, 'wb') as f:
        f.write((bytes(range(256)) * (BIG_SIZE // 256 + 1))[:BIG_SIZE])
    try:
        run(['mcopy', '-i', spec(path), '-n', '-Q', big, '::/'])
    finally:
        os.remove(big)


def extract(image, out):
    """Copy the files of a card to OUT. Entries are copied one by one, because
    mcopy of "::/" alone leaves out hidden ones."""
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    s = spec(image)
    for line in run(['mdir', '-i', s, '-a', '-b', '::/']).splitlines():
        line = line.strip()
        if line and not line.endswith('/.') and not line.endswith('/..'):
            run(['mcopy', '-i', s, '-s', '-n', '-Q', line, out])


def compare(expected, actual):
    e, a = files(expected), files(actual)
    bad = []
    for p in sorted(set(e) - set(a)):
        bad.append('MISSING %s' % p)
    for p in sorted(set(a) - set(e)):
        bad.append('EXTRA   %s' % p)
    for p in sorted(set(e) & set(a)):
        if e[p] != a[p]:
            bad.append('DIFFERENT %s: expected %s, got %s' % (p, e[p][:2], a[p][:2]))
    return bad


def build(images):
    folder = os.path.join(images, 'xcopy-cards')
    os.makedirs(folder, exist_ok=True)
    d = digest()
    full = os.path.join(folder, 'second-full-%s.img' % d)
    empty = os.path.join(folder, 'second-empty-%s.img' % d)
    nospace = os.path.join(folder, 'second-nospace-%s.img' % d)
    big = os.path.join(folder, 'second-big-%s.img' % d)
    for old in os.listdir(folder):
        if old.endswith('.img') and d not in old:
            os.remove(os.path.join(folder, old))
    if not os.path.exists(empty):
        blank_card(empty)
    if not os.path.exists(full):
        tree = os.path.join(folder, 'tree-%s' % d)
        materialise(tree)
        blank_card(full + '.part')
        fill_card(full + '.part', tree)
        back = os.path.join(folder, 'readback-%s' % d)
        extract(full + '.part', back)
        bad = compare(tree, back)
        shutil.rmtree(tree)
        shutil.rmtree(back)
        if bad:
            os.remove(full + '.part')
            sys.exit('the full card does not read back as the fixture:\n' + '\n'.join(bad))
        os.rename(full + '.part', full)
    if not os.path.exists(nospace):
        nospace_card(nospace + '.part', folder)
        os.rename(nospace + '.part', nospace)
    if not os.path.exists(big):
        big_card(big + '.part', folder)
        os.rename(big + '.part', big)
    print(full)
    print(empty)
    print(nospace)
    print(big)


def main(argv):
    if len(argv) >= 3 and argv[1] == 'build':
        build(argv[2])
    elif len(argv) >= 3 and argv[1] == 'materialise':
        materialise(argv[2])
    elif len(argv) >= 4 and argv[1] == 'extract':
        extract(argv[2], argv[3])
    elif len(argv) >= 4 and argv[1] == 'compare':
        bad = compare(argv[2], argv[3])
        print('\n'.join(bad))
        return 1 if bad else 0
    elif len(argv) >= 3 and argv[1] == 'checks':
        for p, v in sorted(files(FIXTURE).items()):
            if v[0] == 'file' and ' ' not in p:
                print('check %s/%s %s' % (argv[2].rstrip('/'), p, os.path.join(os.path.abspath(FIXTURE), p)))
    else:
        sys.exit(__doc__)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
