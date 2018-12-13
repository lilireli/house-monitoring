#!/bin/bash

MYDIR=`dirname $0`

cd $MYDIR
git pull

cd build
make

./house_transmitter --url tcp://192.168.178.69:8001