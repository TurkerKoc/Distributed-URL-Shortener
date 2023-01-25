#!/usr/bin/env bash
set -euo pipefail

curl https://www.sqlite.org/2022/sqlite-autoconf-3400100.tar.gz --output sqlite-autoconf-3400100.tar.gz

tar xvfz sqlite-autoconf-3400100.tar.gz
cd sqlite-autoconf-3400100
./configure --prefix=/usr/local
make
make install

cd ..

export MY_INSTALL_DIR=$HOME/.local
mkdir -p $MY_INSTALL_DIR
export PATH="$MY_INSTALL_DIR/bin:$PATH"
apt install -y cmake
apt install -y build-essential autoconf libtool pkg-config git
git clone --recurse-submodules -b v1.50.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc

cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$MY_INSTALL_DIR" ../..
make -j 4
make install
popd

cd ..



mkdir -p cmake-build-debug
cd cmake-build-debug
cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ..
make
