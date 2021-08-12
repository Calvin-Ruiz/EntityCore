#/bin/bash

cd ..
mkdir build
cd build
clear
cmake ..
make -j install
cd ../LaserBombon
cp ../build/LaserBombon .
