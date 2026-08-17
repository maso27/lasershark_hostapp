#!/usr/bin/env bash
# play_laser.sh — play an audio file through speakers + lasershark, then clean up

# ---- Config: adjust to your setup ----
CARD="hw:2,0"                              # USB dongle, from `aplay -l`
RATE="48000"                               # must be <= lasershark's max ILDA rate
LASERSHARK_BIN="$HOME/laser/lasershark_hostapp/lasershark_jack"
# ----------------------------------------

if [ $# -ne 1 ]; then
    echo "Usage: $0 <audio file>"
    exit 1
fi

AUDIO_FILE="$1"
if [ ! -f "$AUDIO_FILE" ]; then
    echo "File not found: $AUDIO_FILE"
    exit 1
fi

JACKD_PID=""
LSJACK_PID=""
MPLAYER_PID=""

cleanup() {
    echo "Shutting down..."
    [ -n "$MPLAYER_PID" ] && kill "$MPLAYER_PID" 2>/dev/null
    [ -n "$LSJACK_PID" ] && kill -INT "$LSJACK_PID" 2>/dev/null && sleep 1
    [ -n "$JACKD_PID" ] && kill "$JACKD_PID" 2>/dev/null
    wait 2>/dev/null
}
trap cleanup EXIT INT TERM

echo "Starting jackd..."
jackd -d alsa -d "$CARD" -r "$RATE" -p 1024 -n 2 &
JACKD_PID=$!
sleep 2

echo "Starting lasershark_jack..."
"$LASERSHARK_BIN" &
LSJACK_PID=$!
sleep 1

echo "Playing: $AUDIO_FILE"
mplayer -ao jack:name=filesrc:noconnect "$AUDIO_FILE" &
MPLAYER_PID=$!
sleep 1

jack_connect filesrc:out_0 system:playback_1
jack_connect filesrc:out_1 system:playback_2
jack_connect filesrc:out_0 lasershark:in_x
jack_connect filesrc:out_1 lasershark:in_y

wait "$MPLAYER_PID"
echo "Playback finished."
