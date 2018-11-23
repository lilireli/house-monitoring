# Installation needs

## Install Pistache
git clone https://github.com/oktal/pistache.git 
cd pistache
git submodule update --init
mkdir build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install

## Boost ASIO
https://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/examples/cpp11_examples.html

libboost-dev

## ZMQ
sudo apt-get install libzmq3-dev

## Sqlite
sudo apt-get install libsqlite3 libsqlite3-dev

## HTTP server
libboost-all-dev
