#!/bin/bash

if [ $# -ne 5 ]; then
    echo "This script creates TAP-udp-tunnel to remote ns-3 host, a tap device"
    echo "for virtual machine and a bridge connecting the two devices."
    echo "usage: $0 server_ip local_port remote_port flow_id device_number"
    echo "eg. $0 192.168.1.6 7543 7544 17 1"
    echo "creates devices br1, tap1 and tunnel1, tunnel will be connected to"
    echo "remote ns-3."
    exit
fi

SERVER=$1
LOCAL=$2
REMOTE=$3
FLOWID=$4
BRIDGE=br$5
TUNNEL=tunnel$5
TAP=tap$5

echo "Bringing up bridge $BRIDGE with devices $TAP and $TUNNEL.."

# sudo doesn't like going to background when it needs to ask password
# so lets do sudo action and it caches the password for rest of the script.
sudo sleep 1

echo "tap-udptunnel $SERVER $LOCAL $REMOTE $FLOWID $TUNNEL &"
sudo /home/kimdu/bin/tap-udptunnel $SERVER $LOCAL $REMOTE $FLOWID $TUNNEL &
sleep 3
sudo ifconfig $TUNNEL 192.168.2.254 netmask 255.255.255.0
sudo ifconfig $TUNNEL -arp
sudo ifconfig $TUNNEL up

echo "brctl addbr $BRIDGE"
sudo brctl addbr $BRIDGE

echo "tunctl -u $(id -un) -t $TAP"
sudo tunctl -u $(id -un) -t $TAP

echo "ifconfig $TAP 0.0.0.0 promisc up"
sudo ifconfig $TAP 0.0.0.0 promisc up

echo "brctl addif $BRIDGE $TAP"
sudo brctl addif $BRIDGE $TAP

echo "brctl addif $BRIDGE $TUNNEL"
sudo brctl addif $BRIDGE $TUNNEL

echo "brctl show"
sudo brctl show

echo "ifconfig $BRIDGE up"
sudo ifconfig $BRIDGE up
