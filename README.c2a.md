##### License

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

##### Build Instructions for Linux
- See the README.md for dependencies

- Compiling has to be done like this now (from vsomeip_c2a directory):

mkdir build -p && cd build && rm CMakeCache.txt CMakeFiles -rf && cmake -DCMAKE_INSTALL_PREFIX:PATH="${PWD}" -DENABLE_SIGNAL_HANDLING=1 -DCMAKE_BUILD_TYPE=Debug .. && make -j4

cd examples

make clean all && cd -

- now you have examples in bin/ and libs in lib/ of your build/ directory. 
- note that the ../config dir will be copied over in setup.sh scripts 

***NOTE:* This is mainly used as submodule of modular_network_simulator.**

**So setup/build scripts and module .json would now take care of the below**

- Change the unicast at the top and 'routing' of vsomeip-local.<your test>.json to your device's IP and the application name you're running on it.

- Add the multicast routes from this config with 'sudo route add -nv XXX.XXX.XXX.XXX <nic>'

- export LD_LIBRARY_PATH=<path to local above>/lib

- env VSOMEIP_APPLICATION_NAME=<app name> VSOMEIP_CONFIGURATION=<path to vsomeip-local.json>


-- Quick notes on setting up two client to use 'capturable' sockets (not "native" sockets) when run on the same host.
1) add two IP addresses on your loopback interface:
   sudo ip addr add 127.0.0.2/8 dev lo
   sudo ip addr add 127.0.0.3/8 dev lo
2) add your multicast routes to lo:
   sudo route add 224.244.224.245 lo
4) you can use reduced config for the client, or same config for both.
   service has to be defined for the sake of its port -
   maybe one day I'll overcome this.
5) you can add "network" param on top level, with distinct names,
   like "network":"0" and "network":"1"
   in the two .jsons, and distinct
   "unicast" : "127.0.0.2" / "unicast" : "127.0.0.3"
   if you use separate ones.
6) don't specify network and unicast if you're using the same .json.
   instead, prepend VSOMEIP_NETWORK=vsomeip0 VSOMEIP_UNICAST_ADDRESS=127.0.0.2 and
   VSOMEIP_NETWORK=vsomeip1 VSOMEIP_UNICAST_ADDRESS=127.0.0.3 to env for your respective apps