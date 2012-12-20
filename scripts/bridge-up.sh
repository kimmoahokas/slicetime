#!/bin/sh

if [ $# -ne 1 ]; then
    echo "This script creates two tap devices and bridge between them. It is"
    echo "useful for running android emulator and ns-3 in same machine."
    echo "usage: $0 device_base"
    echo "device_base is name for bridge to be created and prefix for tap devices"
    echo "eg. "$0 br0" creates devices br0, br0-tap0 and br0-tap1"
    exit
fi

BRIDGE=$1
TAP0=$BRIDGE-tap0
TAP1=$BRIDGE-tap1

echo "Bringing up bridge $BRIDGE with devices $TAP0 and $TAP1.."

echo "sudo brctl addbr $BRIDGE"
sudo brctl addbr $BRIDGE

echo "sudo tunctl -u $(id -un) -t $TAP0"
sudo tunctl -u $(id -un) -t $TAP0

echo "sudo tunctl -u $(id -un) -t $TAP1"
sudo tunctl -u $(id -un) -t $TAP1

echo "sudo ifconfig $TAP0 0.0.0.0 promisc up"
sudo ifconfig $TAP0 0.0.0.0 promisc up

echo "sudo ifconfig tap1 0.0.0.0 promisc up"
sudo ifconfig $TAP1 0.0.0.0 promisc up

echo "sudo brctl addif $BRIDGE $TAP0"
sudo brctl addif $BRIDGE $TAP0

echo "sudo brctl addif $BRIDGE $TAP1"
sudo brctl addif $BRIDGE $TAP1

echo "brctl show"
brctl show

echo "sudo ifconfig $BRIDGE up"
sudo ifconfig $BRIDGE up

#echo "sudo ifconfig br0 up"
#sudo ifconfig br0 up