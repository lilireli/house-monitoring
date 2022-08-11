#!/bin/bash

MYDIR=`dirname $0`

cd $MYDIR
git pull

cd build
make

./house_transmitter --url https://localhost:5000/temp/add --auth-key 12345