#!/bin/sh
# Two-process LAN test: one authoritative host, one client, both headless.
# Verifies that zombies/players/barricades replicate over real UDP sockets.
set -e
BIN="${1:-./THE_LAST_NIGHT}"
DIR="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$DIR/shots/net"
cd "$DIR"
"$BIN" --nettest host --shots shots/net > /tmp/nethost.log 2>&1 &
HOST=$!
sleep 1
"$BIN" --nettest client 127.0.0.1 --shots shots/net > /tmp/netclient.log 2>&1 &
CLIENT=$!
wait $CLIENT
wait $HOST
echo "--- host log ---"; tail -n 25 /tmp/nethost.log
echo "--- client log ---"; tail -n 25 /tmp/netclient.log
