#!/bin/bash
# Runs LoveDOS test games headless in DOSBox and reports their results.
#
# usage: test/run.sh [test ...]       (default: all tests)
#
# Needs DOSBox and a DPMI host: set DPMI to the path of CWSDPMI.EXE or
# HDPMI32.EXE. Each test writes LOG.TXT containing "ok"/"FAIL" lines and a
# final "fails <n>" line.
set -u
cd "$(dirname "$0")"
TESTDIR=$(pwd)
EXE=${EXE:-$TESTDIR/../bin/love.exe}
DPMI=${DPMI:?set DPMI to the path of CWSDPMI.EXE or HDPMI32.EXE}
DOSBOX=${DOSBOX:-dosbox}
TIMEOUT=${TIMEOUT:-120}
TMP=$(mktemp -d)
trap '[ -z "${KEEP:-}" ] && rm -rf "$TMP"' EXIT

tests=("$@")
if [ ${#tests[@]} -eq 0 ]; then
  for d in */; do [ -f "$d/main.lua" ] && tests+=("${d%/}"); done
fi

dpmiexe=$(basename "$DPMI" | tr a-z A-Z)
resident=""
[ "$dpmiexe" = "HDPMI32.EXE" ] && resident="HDPMI32 -r > NUL"

failed=0
for t in "${tests[@]}"; do
  rm -rf "$TMP/c" && mkdir -p "$TMP/c"
  cp "$EXE" "$TMP/c/LOVE.EXE"
  cp "$DPMI" "$TMP/c/$dpmiexe"
  cp -r "$t" "$TMP/c/GAME"
  cp testlib.lua "$TMP/c/GAME/"
  cat > "$TMP/dosbox.conf" <<CONF
[dosbox]
machine=svga_s3
memsize=32
[cpu]
core=dynamic
cycles=max
[mixer]
rate=44100
[sblaster]
sbtype=sb16
[speaker]
pcspeaker=false
[autoexec]
mount c "$TMP/c"
c:
set BLASTER=A220 I7 D1 H5 T6
$resident
cd GAME
..\\LOVE.EXE . > ..\\OUT.TXT
exit
CONF
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$TMP/audio.raw" \
    timeout "$TIMEOUT" "$DOSBOX" -conf "$TMP/dosbox.conf" -noconsole -exit \
    > "$TMP/dosbox.log" 2>&1
  log=$(find "$TMP/c/GAME" -maxdepth 1 -iname log.txt | head -1)
  out=$(find "$TMP/c" -maxdepth 1 -iname out.txt | head -1)
  result=$(tr -d "\r" < "$log" 2>/dev/null | grep -E "^fails " | tail -1)
  if [ "$result" = "fails 0" ] && [ ! -s "$out" ]; then
    echo "PASS $t ($(grep -c '^ok' "$log") checks)"
  else
    failed=$((failed + 1))
    echo "FAIL $t: ${result:-no result (crashed or timed out)}"
    [ -n "$log" ] && tr -d '\r' < "$log" | grep -v '^ok' | sed 's/^/    /'
    [ -s "$out" ] && sed "s/^/    out: /" "$out"
  fi
done
exit $failed
