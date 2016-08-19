#!/bin/bash

#
# Download and build dependencies as static libs
#
set -e

ZLIB_VERSION=1.2.8
LIBARCHIVE_VERSION=3.2.1
LIBSODIUM_VERSION=1.0.10
CONFUSE_VERSION=3.0

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
DEPS_DIR=$BASE_DIR/deps
DOWNLOAD_DIR=$DEPS_DIR/dl
DEPS_INSTALL_DIR=$DEPS_DIR/usr
PKG_CONFIG_PATH=$DEPS_INSTALL_DIR/lib/pkgconfig

MAKE_FLAGS=-j8

UNAME_S=$(uname -s)
if [[ $UNAME_S = "CYGWIN_NT-10.0" ]]; then
    # Cygwin is required to buid, but we want fwup to run with only
    # the mingw-w64 runtime library.
    CONFIGURE_ARGS=--host=x86_64-w64-mingw32
fi

# Initialize some directories
mkdir -p $DOWNLOAD_DIR
mkdir -p $DEPS_INSTALL_DIR

pushd $DEPS_DIR

pushd $DOWNLOAD_DIR
[[ -e zlib-$ZLIB_VERSION.tar.xz ]] || wget http://zlib.net/zlib-$ZLIB_VERSION.tar.xz
[[ -e confuse-$CONFUSE_VERSION.tar.xz ]] || wget https://github.com/martinh/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.xz
[[ -e libarchive-$LIBARCHIVE_VERSION.tar.gz ]] || wget http://libarchive.org/downloads/libarchive-$LIBARCHIVE_VERSION.tar.gz
[[ -e libsodium-$LIBSODIUM_VERSION.tar.gz ]] || wget https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz
popd

if [[ ! -e $DEPS_INSTALL_DIR/lib/libz.a ]]; then
    rm -fr $DEPS_DIR/zlib-*
    tar xf $DOWNLOAD_DIR/zlib-$ZLIB_VERSION.tar.xz
    pushd zlib-$ZLIB_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$DEPS_INSTALL_DIR --static
    make $MAKE_FLAGS
    make install
    popd
fi


if [[ ! -e $DEPS_INSTALL_DIR/lib/libconfuse.a ]]; then
    rm -fr $DEPS_DIR/confuse-*
    tar xf $DOWNLOAD_DIR/confuse-$CONFUSE_VERSION.tar.xz
    pushd confuse-$CONFUSE_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$DEPS_INSTALL_DIR --disable-examples --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $DEPS_INSTALL_DIR/lib/libarchive.a ]]; then
    rm -fr libarchive-*
    tar xf $DOWNLOAD_DIR/libarchive-$LIBARCHIVE_VERSION.tar.gz
    pushd libarchive-$LIBARCHIVE_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH LDFLAGS=-L$DEPS_INSTALL_DIR/lib CPPFLAGS=-I$DEPS_INSTALL_DIR/include ./configure \
        $CONFIGURE_ARGS \
        --prefix=$DEPS_INSTALL_DIR --without-xml2 --without-openssl \
        --without-nettle --without-expat --without-lzo2 --without-lzma \
        --without-bz2lib --without-iconv --enable-shared=no --disable-bsdtar \
        --disable-bsdcpio --disable-bsdcat --disable-xattr \
        --without-libiconv-prefix --without-lz4
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $DEPS_INSTALL_DIR/lib/libsodium.a ]]; then
    rm -fr libsodium-*
    tar xf $DOWNLOAD_DIR/libsodium-$LIBSODIUM_VERSION.tar.gz
    pushd libsodium-$LIBSODIUM_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$DEPS_INSTALL_DIR --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

# Return to the base directory
popd

echo Dependencies built successfully!
echo
echo To compile fwup statically with these libraries, run:
echo
echo ./autogen.sh    # if you're compiling from source
echo LDFLAGS=-L$DEPS_INSTALL_DIR/lib CPPFLAGS=-I$DEPS_INSTALL_DIR/include ./configure $CONFIGURE_ARGS --enable-shared=no
echo make
echo make check
echo make install
