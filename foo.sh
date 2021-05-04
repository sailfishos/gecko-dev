#!/bin/bash
export BUILD_DIR=`pwd`/../obj-build-mer-qt-xr
export BASE_CONFIG=`pwd`/../embedding/embedlite/config/mozconfig.merqtxulrunner
export CARGO_HOME=`pwd`/.cargo

export SB2_RUST_TARGET_TRIPLE=armv7-unknown-linux-gnueabihf
export RUST_HOST_TARGET=armv7-unknown-linux-gnueabihf
export RUST_TARGET=armv7-unknown-linux-gnueabihf
export TARGET=armv7-unknown-linux-gnueabihf
export HOST=armv7-unknown-linux-gnueabihf
export CARGO_BUILD_TARGET=armv7-unknown-linux-gnueabihf
export CARGO_CFG_TARGET_ARCH=armv7
# This should be define...
export CROSS_COMPILE=armv7-unknown-linux-gnueabihf
export CXXFLAGS="$CXXFLAGS -DCROSS_COMPILE=armv7-unknown-linux-gnueabihf"

export CC=gcc
export CXX=g++
export AR="gcc-ar"
export NM="gcc-nm"
export RANLIB="gcc-ranlib"

# This should be added very likely to the 
# export CARGOFLAGS=' -Z avoid-dev-deps'
# export CARGOFLAGS=' -j1'
mkdir -p $BUILD_DIR
cp -rf $BASE_CONFIG $BUILD_DIR/mozconfig
echo "export MOZCONFIG=$BUILD_DIR/mozconfig" >> $BUILD_DIR/rpm-shared.env
echo "export QT_QPA_PLATFORM=minimal" >> $BUILD_DIR/rpm-shared.env
echo "export MOZ_OBJDIR=$BUILD_DIR" >> $BUILD_DIR/rpm-shared.env
echo "export CARGO_HOME=$CARGO_HOME" >> $BUILD_DIR/rpm-shared.env
# echo "export CARGOFLAGS=' -Z avoid-dev-deps'" >> $BUILD_DIR/rpm-shared.env
source $BUILD_DIR/rpm-shared.env
#$PWD/mach build -j1
