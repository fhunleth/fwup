#!/bin/bash

#
# Build script for Travis
#
# Inputs:
#    MODE           - "static", "dynamic", "windows", "raspberrypi", or "minimal"
#
# Static builds use scripts to download libarchive and libconfuse, so those are
# only installed on shared library builds.
#

set -e
set -v

FWUP_VERSION=$(cat VERSION)
OS_NAME="$(uname -s)"

# Create ./configure
./autogen.sh

case "${OS_NAME}-${MODE}" in
    *-static)
        # If this is a static build, run 'build_pkg.sh'
        bash -v scripts/build_pkg.sh
        exit 0
        ;;
    Linux-minimal)
        bash -v scripts/build_and_test_minimal.sh
        exit 0
        ;;
    Linux-dynamic)
        ./configure --enable-gcov
        ;;
    Linux-singlethread)
        # The verify-syscalls script can't follow threads, so
        # single thread builds are the only way to verify that
        # the issued read and write calls follow the expected
        # alignment, size and order
        ./configure --enable-gcov --without-pthreads
        ;;
    Darwin-dynamic)
        PKG_CONFIG_PATH="/usr/local/opt/libarchive/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure;
        ;;
    Linux-windows)
        CC=x86_64-w64-mingw32-gcc \
            CROSS_COMPILE=x86_64-w64-mingw32 \
            bash -v scripts/build_pkg.sh
        exit 0
        ;;
    Linux-raspberrypi)
        CC=arm-linux-gnueabihf-gcc \
            CROSS_COMPILE=arm-linux-gnueabihf \
            PATH=~/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin:$PATH \
            QEMU_LD_PREFIX=~/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/libc/lib/arm-linux-gnueabihf \
            bash -v scripts/build_pkg.sh
        exit 0
        ;;
    *)
        echo "Unexpected build option: ${OS_NAME}-${MODE}"
        exit 1
esac

# Normal build
make -j4
if ! make -j4 check; then
    cat tests/test-suite.log
    echo "git source 'make check' failed. See log above"
    exit 1
fi
make dist

# Check that the distribution version works by building it again
tar xf fwup-$FWUP_VERSION.tar.gz
cd fwup-$FWUP_VERSION
if [ "$OS_NAME" = "Linux" ]; then
    ./configure;
else
    PKG_CONFIG_PATH="/usr/local/opt/libarchive/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure;
fi
make -j4
if ! make -j4 check; then
    cat tests/test-suite.log
    echo "Distribution 'make check' failed. See log above"
    exit 1
fi
