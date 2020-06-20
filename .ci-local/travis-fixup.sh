#!/bin/sh
set -e -u -x

env|grep TRAVIS

[ "$TRAVIS_OS_NAME" = "linux" ] || exit 0

# Ensure there is an interface with a (correct) broadcast address
# eg. 'trusty' VMs have interface broadcast address mis-configured
# (why oh why do people insist on setting this explicitly?)

sudo ip tuntap add dev tap42 mode tap

sudo ip addr add 192.168.240.1/24 broadcast + dev tap42

sudo ip link set dev tap42 up

# note that this device will be UP but not RUNNING
# so java will see it as not UP since java confuses UP and RUNNING
