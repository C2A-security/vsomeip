#!/bin/sh

mkdir -p build && cp config -R build/
sudo ip link add lo1 type dummy
sudo ifconfig lo1 up
sudo ifconfig lo1 127.0.0.2/8
sudo ifconfig lo 127.0.0.1/8

# todo clean up previous routes if exist
sudo route add 224.244.224.245 lo # todo make this take the mcast from json?
sudo route add 224.244.224.245 lo1 # todo make this take the mcast from json?

