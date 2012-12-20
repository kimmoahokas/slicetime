#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Destroy devices created by tunnnel-up.sh"
    echo "usage: $0 device_number"
    echo "eg. $0 1"
    echo "deletes devices br1, tap1 and tunnel1."
    exit
fi

BRIDGE="br$1"
TUNNEL="tunnel$1"
TAP="tap$1"

TUNCMD=$(pgrep -l -f "tap-udptunnel [0-9. ]* $TUNNEL")
echo "killing tunnel: $TUNCMD"
sudo pkill -f "tap-udptunnel [0-9. ]* $TUNNEL"

sudo ifconfig $BRIDGE down
sudo ifconfig $TAP down
sudo brctl delif $BRIDGE $TAP
sudo tunctl -d $TAP
sudo brctl delbr $BRIDGE
