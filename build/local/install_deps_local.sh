#!/bin/bash
cd "`dirname $0`"
set -ex
SOURCE_DIR="`pwd`/_source"
BUILD_DIR="`pwd`/_build"
INSTALL_DIR="`pwd`/_install"
CACHE_DIR="`pwd`/_cache"
mkdir -p $SOURCE_DIR
mkdir -p $BUILD_DIR
mkdir -p $INSTALL_DIR
source ../../VERSION
if [ -z "$JOBS" ]; then
  JOBS=`nproc`
  if [ -z "$JOBS" ]; then
    JOBS=1
  fi
fi
# CLI11
if [ ! -e $INSTALL_DIR/CLI11/include/CLI/Version.hpp ]; then
  pushd $INSTALL_DIR
    rm -rf CLI11
    git clone --branch v$CLI11_VERSION --depth 1 https://github.com/CLIUtils/CLI11.git
  popd
fi
# # nlohmann/json
# if [ ! -e $INSTALL_DIR/json/include/nlohmann/json.hpp ]; then
#   pushd $INSTALL_DIR
#     rm -rf json
#     git clone --branch v$JSON_VERSION --depth 1 https://github.com/nlohmann/json.git
#   popd
# fi
# WebRTC
if [ ! -e $INSTALL_DIR/webrtc/lib/libwebrtc.a ]; then
  rm -rf $INSTALL_DIR/webrtc
  ../../script/get_webrtc.sh $WEBRTC_BUILD_VERSION ubuntu-18.04_x86_64 $INSTALL_DIR $SOURCE_DIR
fi
# LLVM
if [ ! -e $INSTALL_DIR/llvm/clang/bin/clang++ ]; then
  rm -rf $INSTALL_DIR/llvm
  ../../script/get_llvm.sh $INSTALL_DIR/webrtc $INSTALL_DIR
fi
# Boost
if [ ! -e $INSTALL_DIR/boost/lib/libboost_filesystem.a ]; then
  rm -rf $SOURCE_DIR/boost
  rm -rf $BUILD_DIR/boost
  rm -rf $INSTALL_DIR/boost
  mkdir -p $SOURCE_DIR/boost
  ../../script/setup_boost.sh $BOOST_VERSION $SOURCE_DIR/boost $CACHE_DIR/boost
  pushd $SOURCE_DIR/boost/source
    echo "using clang : : $INSTALL_DIR/llvm/clang/bin/clang++ : ;" > project-config.jam
    ./b2 \
      cxxflags=" \
        -D_LIBCPP_ABI_UNSTABLE \
        -nostdinc++ \
        -isystem$INSTALL_DIR/llvm/libcxx/include \
      " \
      toolset=clang \
      visibility=global \
      target-os=linux \
      address-model=64 \
      link=static \
      variant=release \
      install \
      -j$JOBS \
      --build-dir=$BUILD_DIR/boost \
      --prefix=$INSTALL_DIR/boost \
      --ignore-site-config \
      --with-filesystem \
      --with-json
  popd
fi
# SDL2
if [ ! -e $INSTALL_DIR/SDL2/lib/libSDL2.a ]; then
  rm -rf $SOURCE_DIR/SDL2
  rm -rf $BUILD_DIR/SDL2
  rm -rf $INSTALL_DIR/SDL2
  mkdir -p $SOURCE_DIR/SDL2
  mkdir -p $BUILD_DIR/SDL2
  ../../script/setup_sdl2.sh $SDL2_VERSION $SOURCE_DIR/SDL2
  pushd $BUILD_DIR/SDL2
    CC=$INSTALL_DIR/llvm/clang/bin/clang \
    CXX=$INSTALL_DIR/llvm/clang/bin/clang++ \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR/SDL2 \
      $SOURCE_DIR/SDL2/source
    cmake --build . -j$JOBS
    cmake --build . --target install
  popd
fi

# nanomsg/nng
if [ ! -e $INSTALL_DIR/nng/lib/libnng.a ]; then
  rm -rf $SOURCE_DIR/nng
  rm -rf $BUILD_DIR/nng
  rm -rf $INSTALL_DIR/nng
  pushd $SOURCE_DIR
    rm -rf nng
    git clone --branch v$NNG_VERSION --depth 1 https://github.com/nanomsg/nng.git
  popd
  mkdir -p $BUILD_DIR/nng
  pushd $BUILD_DIR/nng
    CC=$INSTALL_DIR/llvm/clang/bin/clang \
    CXX=$INSTALL_DIR/llvm/clang/bin/clang++ \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR/nng \
      $SOURCE_DIR/nng

    cmake --build . -j$JOBS
    cmake --build . --target install
  popd
fi

# cwzx/nngpp header-only
if [ ! -e $INSTALL_DIR/nngpp/include/nngpp/nngpp.h ]; then
  pushd $INSTALL_DIR
    rm -rf nngpp
    git clone --branch nng-v$NNGPP_VERSION --depth 1 https://github.com/cwzx/nngpp.git
  popd
fi