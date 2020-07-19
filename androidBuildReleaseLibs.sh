#!/usr/bin/env bash

# Cross-compile libmp4v2 for use in an Android app

# Builds both shared libraries (libmp4v2.so) and static libraries (libmp4v2.so) for each Android ABI
# Outputs are copied to lib folder:
#  lib
#   ├── aarch64-linux-android
#   │   ├── libmp4v2.a
#   │   └── libmp4v2.so
#   ├── armv7a-linux-androideabi
#   │   ├── libmp4v2.a
#   │   └── libmp4v2.so
#   ├── i686-linux-android
#   │   ├── libmp4v2.a
#   │   └── libmp4v2.so
#   └── x86_64-linux-android
#       ├── libmp4v2.a
#       └── libmp4v2.so

# This script uses autoconf which requires Linux/MacOS (or WSL on Linux)
# with Android NDK installed.

# You must set the variable NDK to the path to the Android NDK to use before
# running this script e.g.:
#    export NDK=~/Library/Android/sdk/ndk/21.3.6528147

# By default uses min SDK version 16. You can override this by setting the
# variable MIN_SDK e.g.
#    export MIN_SDK=30

if [[ -z "$NDK" ]]; then
    echo "Must set NDK to Android NDK location" 1>&2
    exit 1
fi

MIN_SDK=${MIN_SDK:=16}
echo "building for API version $MIN_SDK"

_host_tag="$(uname -s | tr '[:upper:]' '[:lower:]')"
export TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/$_host_tag-x86_64
echo "Using toolchain $TOOLCHAIN"

PATH="$TOOLCHAIN/bin:$PATH"

_setup_ndk() {
	
	if [ $MIN_SDK -lt 21  ] && [ $TARGET = "aarch64-linux-android" ] || [ $TARGET = "x86_64-linux-android" ]; then  
		# No support for 64 bit prior to NDK version 21 but ok to use API 21 for 64 bit builds since the 64 bit version is ignored on those system
		export API=21
	else
		export API=$MIN_SDK
	fi

	export CC=$TOOLCHAIN/bin/$TARGET$API-clang
	export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++

	# armv7a doesn't follow the usual pattern
	if [ $TARGET = "armv7a-linux-androideabi" ]; then
  		_tool_prefix="arm-linux-androideabi"
	else
  		_tool_prefix=$TARGET
	fi

	export AR=$TOOLCHAIN/bin/$_tool_prefix-ar
	export AS=$TOOLCHAIN/bin/$_tool_prefix-as
	export LD=$TOOLCHAIN/bin/$_tool_prefix-ld
	export RANLIB=$TOOLCHAIN/bin/$_tool_prefix-ranlib
	export STRIP=$TOOLCHAIN/bin/$_tool_prefix-strip
	export NM=$TOOLCHAIN/bin/$_tool_prefix-nm
	export CHOST=$TARGET
}

_build() {

	rm -rf build
	mkdir build
	pushd build
	# --with-pic is used for static lib since static lib in Android app is added to a shared lib which can't have relocations
	# -- disable-util builds the library only and skips the utility programs, --disable-debug builds release only
	../configure --host $TARGET --disable-util --disable-debug --with-pic
	make -j8
	mkdir ../lib/$TARGET
	cp .libs/*.so ../lib/$TARGET/
	cp .libs/*.a ../lib/$TARGET/
	popd
}

for TARGET in aarch64-linux-android armv7a-linux-androideabi i686-linux-android x86_64-linux-android
do
	_setup_ndk
	_build
done


