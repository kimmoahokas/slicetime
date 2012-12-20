#!/bin/sh

if [ $# -ne 1 ]; then
    echo "This script destroys devices created by bridge-up.sh"
    echo "usage: $0 device_base"
    echo "device_base is name for bridge to be destroyed and prefix for tap devices"
    echo "eg. "$0 br0" destroys devices br0, br0-tap0 and br0-tap1"
    exit
fi

BRIDGE=$1
TAP0=$BRIDGE-tap0
TAP1=$BRIDGE-tap1

echo "Destroying bridge $BRIDGE with devices $TAP0 and $TAP1.."


echo "sudo ifconfig $BRIDGE down"
sudo ifconfig $BRIDGE down

echo "sudo ifconfig $TAP0 down"
sudo ifconfig $TAP0 down

echo"sudo ifconfig $TAP1 down"
sudo ifconfig $TAP1 down

echo "sudo brctl delif $BRIDGE $TAP0"
sudo brctl delif $BRIDGE $TAP0

echo "sudo brctl delif $BRIDGE $TAP1"
sudo brctl delif $BRIDGE $TAP1

echo "sudo tunctl -d $TAP0"
sudo tunctl -d $TAP0

echo "sudo tunctl -d $TAP1"
sudo tunctl -d $TAP1

echo "sudo brctl delbr $BRIDGE"
sudo brctl delbr $BRIDGE

echo "brctl show"
brctl show