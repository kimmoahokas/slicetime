#!/bin/bash

#Get directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
####################################################
# Set following variables before using this script #
####################################################

# TODO: Set here path to directory where your qemu is
QEMUDIR=${HOME}/Work/Android/out/host/linux-x86

# TODO: Set here directory where your android sdk is extracted
export ANDROID_SDK_ROOT=${HOME}/android-sdk-linux

# TODO: Set emulator binary directory
BIN=$QEMUDIR/bin
#BIN=${HOME}/Work/Android/external/qemu/objs
# If you are building emulator from source $BIN should be something like:
# BIN=$QEMUDIR/objs
# if you are building whole android system $BIN should be something like:
# BIN=


#############################################################
# In theory you don't need to edit anything after this line #
#############################################################

PARAMS="$@"

if [ $# -lt 1 ]; then
    echo "Usage $0 <avd> [-t <tap_device>] [-i <ip_address>] [-e <emulator_args>]"
    echo "	[-q <qemu_args>], [-m <mac_address>] [-c <icount>] [-v]"
    echo "<avd> is android virtual device name"
    echo "<tap_device> is the tap device to use"
    echo "<ip_adress> is the internal ip address to use"
    echo "<emulator_args> are additional arguments to emulator"
    echo "<qemu_args> are additional arguments to qemu"
    echo "<mac_address> is ethernet address used inside emulator"
    echo "<icount> is emulator icount. default 3"
    echo "if -v is defined print additional debug info"
    echo "eg. $0 Jelly1"
    echo "or $0 Jelly1 -t br0-tap0 -i 192.168.0.5 -e \"-no-snapshot-load\""
    exit
fi

#default values
TAP=br0-tap0
IP=192.168.0.1
ICOUNT=3
VERBOSE=""

#parse command line arguments
AVD=$1
shift
while getopts "t:i:e:q:m:c:v" opt; do
  echo "opt: $opt, OPTARG: $OPTARG"
  case $opt in
    t)
      TAP=$OPTARG
      ;;
    i)
      IP=$OPTARG
      ;;
    e)
      EMU=$OPTARG
      ;;
    q)
      QEMU=$OPTARG
      ;;
    m)
      MAC=$OPTARG
      ;;
    c)
      ICOUNT=$OPTARG
      ;;
    v)
      VERBOSE="-verbose"
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done

# get the default mac address of tap interface
MAC_DEF="00:00:00:00:00:0${IP##*.}"
if [ -z "$MAC" ]; then
    MAC=$MAC_DEF
fi


# This line brings up the eth1 interface in android with given ip and mac
QEMUNET="-net nic,vlan=0 -net user,vlan=0 -net nic,vlan=1,macaddr=$MAC -net tap,vlan=1,ifname=$TAP,script=no,downscript=no -boot-property net.shared_net_ip=$IP"

# general qemu arguments
QEMUARGS="-monitor stdio -icount $ICOUNT $QEMU"

#android emulator arguments
ARGS="-avd $AVD $VERBOSE $EMU"

echo "Starting android emulator.."

echo "Command: emulator $ARGS -qemu $QEMUNET $QEMUARGS"
$BIN/emulator $ARGS -qemu $QEMUNET $QEMUARGS

echo "Emulator terminated. To run again type:"
echo "	$0 $PARAMS"
