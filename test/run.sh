#!/bin/bash
# D3 test suite: builds the commands, runs each scenario through the pinesy
# harness, prints PASS or FAIL for each, and exits 1 when any fails.
#
#   test/run.sh                   build, then run every scenario
#   test/run.sh ini every         run only the named scenarios
#
# Environment: see test/README.md.

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(dirname "$HERE")

die() { echo "run.sh: $*" >&2; exit 2; }

ALL="ini ini-lastline every untar untar-existing untar-examples untar-filefirst untar-scroll untar-nospace ch8show inkey less less-break gunzip
     xcopy-local xcopy-scroll xcopy-err crash-detect xcopy-up xcopy-down"
NAMES=${*:-$ALL}
for n in $NAMES; do
    [ -f "$HERE/scenarios/$n.scn" ] || die "no scenario called $n"
done

# ---------------------------------------------------------------- environment

[ -n "$PINESY_HARNESS" ] || die "PINESY_HARNESS is not set. Set it to the pinesy-harness folder, the one that holds run.py."
[ -f "$PINESY_HARNESS/run.py" ] || die "PINESY_HARNESS is $PINESY_HARNESS, which has no run.py."
[ -n "$D3_TEST_IMAGES" ] || die "D3_TEST_IMAGES is not set. Set it to a folder for disk images and run logs, one that backups and indexing skip."
mkdir -p "$D3_TEST_IMAGES" || die "cannot make D3_TEST_IMAGES ($D3_TEST_IMAGES)."
if [ -n "$MAME" ]; then
    [ -x "$MAME" ] || die "MAME is $MAME, which is not an executable. Unset it to use the harness's own Pinesy binary."
fi

NEED_NET=
case " $NAMES " in *" xcopy-up "*|*" xcopy-down "*) NEED_NET=1 ;; esac
NEED_CARDS=
case " $NAMES " in *" xcopy-"*|*" untar-nospace "*) NEED_CARDS=1 ;; esac

if [ -n "$NEED_NET" ]; then
    [ -n "$NBN_SERVER" ] || die "NBN_SERVER is not set. Set it to the NBN PersonalServer folder (NBNtools/server), built with npm run build."
    [ -f "$NBN_SERVER/build/main.js" ] || die "NBN_SERVER is $NBN_SERVER, which has no build/main.js. Run npm install and npm run build there."
fi

TOOLS="python3 gzip txt2bas timeout"
[ -n "$NEED_CARDS" ] && TOOLS="$TOOLS mcopy mmd mattrib mdir mkfs.vfat"
[ -n "$NEED_NET" ] && TOOLS="$TOOLS npm node lsof"
[ -z "$D3_DOT_DIR" ] && TOOLS="$TOOLS make docker"
for t in $TOOLS; do
    command -v "$t" >/dev/null || die "the tool $t is not on the PATH."
done

# ---------------------------------------------------------------- work folder

WORK=$(mktemp -d "${TMPDIR:-/tmp}/d3test.XXXXXX") || die "cannot make a temporary folder."
SPID=
PORT=
cleanup() {
    if [ -n "$SPID" ]; then
        {
            kill "$SPID"
            if [ -n "$PORT" ]; then
                for p in $(lsof -ti "tcp:$PORT" -sTCP:LISTEN); do kill "$p"; done
            fi
            wait "$SPID"
        } 2>/dev/null
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT
mkdir -p "$WORK/scn" "$WORK/out"

export PINESY_HARNESS_RUNS="$D3_TEST_IMAGES/runs"
export PINESY_HARNESS_IMAGES="$D3_TEST_IMAGES/harness"
rm -rf "$PINESY_HARNESS_RUNS"
mkdir -p "$D3_TEST_IMAGES/cards"

# ---------------------------------------------------------------- build

if [ -n "$D3_DOT_DIR" ]; then
    DOTS=$D3_DOT_DIR
else
    DOTS=$REPO/DEV-D3
    echo "Building (make all)"
    if ! make -C "$REPO" all >"$WORK/make.log" 2>&1; then
        tail -30 "$WORK/make.log"
        die "make all failed."
    fi
fi

if [ -n "$NEED_CARDS" ]; then
    CARDS=$(python3 "$HERE/tools/cards.py" build "$D3_TEST_IMAGES") || die "cannot build the second SD card images."
    FULL=$(echo "$CARDS" | sed -n 1p)
    EMPTY=$(echo "$CARDS" | sed -n 2p)
    NOSPACE=$(echo "$CARDS" | sed -n 3p)
fi

# ---------------------------------------------------------------- server

if [ -n "$NEED_NET" ]; then
    SROOT=$WORK/server
    mkdir -p "$SROOT"
    python3 "$HERE/tools/cards.py" materialise "$SROOT/down" || die "cannot make the server folder."
    PORT=$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1])')
    (cd "$NBN_SERVER" && NBN_FILEPATH="$SROOT" NBN_IP=127.0.0.1 NBN_PORT="$PORT" NBN_SHOWDOTS=true exec npm start) \
        >"$WORK/server.log" 2>&1 &
    SPID=$!
    for i in $(seq 1 60); do
        grep -q 'Server listening' "$WORK/server.log" && break
        sleep 0.5
    done
    grep -q 'Server listening' "$WORK/server.log" || { cat "$WORK/server.log"; die "the NBN server did not start."; }
fi

# ---------------------------------------------------------------- one scenario

# Makes $WORK/scn/NAME.scn from the template, runs it, and writes
# $WORK/out/NAME.txt (what to show on a failure) and $WORK/out/NAME.res (PASS or FAIL).
run_scn() {
    local n=$1 card= src= post=
    local scn=$WORK/scn/$n.scn out=$WORK/out/$n.txt
    case $n in
        gunzip)      gzip -n -c "$HERE/data/d3.tar" >"$WORK/D3.TAR.GZ" ;;
        xcopy-local) src=$FULL;  post=card ;;
        xcopy-scroll) src=$FULL ;;
        xcopy-up)    src=$FULL;  post=up ;;
        xcopy-err)   src=$EMPTY ;;
        xcopy-down)  src=$EMPTY; post=down ;;
        untar-nospace) src=$NOSPACE ;;
    esac
    if [ -n "$src" ]; then
        card=$D3_TEST_IMAGES/cards/$n.img
        cp -c "$src" "$card" 2>/dev/null || cp "$src" "$card"
        chmod 644 "$card"
    fi
    sed -e "s|@DOTS@|$DOTS|g" -e "s|@DATA@|$HERE/data|g" -e "s|@WORK@|$WORK|g" \
        -e "s|@CARD@|$card|g" -e "s|@PORT@|$PORT|g" "$HERE/scenarios/$n.scn" >"$scn"
    [ "$n" = xcopy-local ] && python3 "$HERE/tools/cards.py" checks /xl >>"$scn"

    timeout -k 10 900 "$PINESY_HARNESS/run.py" "$scn" >"$out" 2>&1
    local rc=$?
    local ok=1
    if grep -q '^# expect-fail' "$scn"; then
        [ $rc -eq 1 ] || ok=
        [ $rc -eq 1 ] && echo "(the run failed, as this scenario expects)" >>"$out"
    else
        [ $rc -eq 0 ] || ok=
    fi

    # What the copy left behind, read from the card or the server folder.
    local expect=$WORK/expect-$n
    if [ -n "$post" ]; then
        python3 "$HERE/tools/cards.py" materialise "$expect"
        local diff=
        case $post in
            up)
                diff=$(python3 "$HERE/tools/cards.py" compare "$expect" "$SROOT/up" 2>&1)
                ;;
            down)
                python3 "$HERE/tools/cards.py" extract "$card" "$WORK/card-$n" >>"$out" 2>&1
                diff=$(python3 "$HERE/tools/cards.py" compare "$expect" "$WORK/card-$n" 2>&1)
                ;;
            card)
                ;;
        esac
        if [ -n "$diff" ]; then
            ok=
            { echo "folder compare, expected against actual:"; echo "$diff"; } >>"$out"
        fi
        # a copy from D: must leave D: as it was
        if [ "$post" = up ] || [ "$post" = card ]; then
            python3 "$HERE/tools/cards.py" extract "$card" "$WORK/card-$n" >>"$out" 2>&1
            diff=$(python3 "$HERE/tools/cards.py" compare "$expect" "$WORK/card-$n" 2>&1)
            if [ -n "$diff" ]; then
                ok=
                { echo "D: changed:"; echo "$diff"; } >>"$out"
            fi
        fi
    fi

    if [ -n "$ok" ]; then
        echo PASS >"$WORK/out/$n.res"
        [ -n "$card" ] && rm -f "$card"
    else
        echo FAIL >"$WORK/out/$n.res"
    fi
}

report() {
    local n=$1
    local res
    res=$(cat "$WORK/out/$n.res")
    printf '%s %s\n' "$res" "$n"
    if [ "$res" = FAIL ]; then
        grep -v '^command' "$WORK/out/$n.txt" | head -40 | sed 's/^/    /'
        FAILED=1
    fi
}

# ---------------------------------------------------------------- run

FAILED=
PAIR=
case " $NAMES " in *" xcopy-up "*) case " $NAMES " in *" xcopy-down "*) PAIR=1 ;; esac ;; esac
for n in $NAMES; do
    case $n in
        xcopy-up|xcopy-down) [ -n "$PAIR" ] && continue ;;
    esac
    run_scn "$n"
    report "$n"
done
if [ -n "$PAIR" ]; then
    # Both read and write their own folders of the one server, and run at real speed.
    run_scn xcopy-up &
    UP_PID=$!
    run_scn xcopy-down &
    wait "$UP_PID" $!
    report xcopy-up
    report xcopy-down
fi

echo "Total wall time: ${SECONDS} s"
[ -z "$FAILED" ] || exit 1
exit 0
