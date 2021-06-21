#/bin/bash

cd ..
mkdir build
cd build
clear
cmake ..
make -j
cd ../LaserBombon
cp ../build/LaserBombon .
