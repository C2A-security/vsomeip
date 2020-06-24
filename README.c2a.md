##### License

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

##### Build Instructions for Linux
- See th README.md for dependencies

- Compiling has to be done like this now:
mkdir local # if you don't intend to install vsomeip libs system-wise
mkdir build
cd build
rm CMakeCache.txt CMakeFiles -rf && cmake -DCMAKE_INSTALL_PREFIX:PATH=../local -DENABLE_SIGNAL_HANDLING=1 -DCMAKE_BUILD_TYPE=Debug ..
make -j4
cd examples
make clean all


***NOTE THIS HAS BEEN MADE A SUBMODULE of modular_network_simulator***

- Change the unicast at the top and 'routing' of vsomeip-local.json to your device's IP and the application name you're running on it.

- Add the muticast routes from this config with 'sudo route add -nv XXX.XXX.XXX.XXX <nic>'

- export LD_LIBRARY_PATH=<path to local above>/lib

- env VSOMEIP_APPLICATION_NAME=<app name> VSOMEIP_CONFIGURATION=<path to vsomeip-local.json>
