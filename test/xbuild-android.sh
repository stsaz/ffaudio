#!/bin/bash

# ffaudio test: cross-build on Linux for Android

set -xe

if ! test -d "../ffaudio" ; then
	exit 1
fi

if ! podman container exists ffaudio_android_build ; then
	if ! podman image exists ffaudio-android-builder ; then
		# Create builder image
		cat <<EOF | podman build -t ffaudio-android-builder -f - .
FROM debian:bookworm-slim
RUN apt update && \
 apt install -y \
  make
EOF
	fi

	# Create builder container
	podman create --attach --tty \
	 -v `pwd`/..:/src \
	 -v $SDK_DIR:/Android \
	 --name ffaudio_android_build \
	 ffaudio-android-builder \
	 bash -c 'cd /src/ffaudio && source ./build_android.sh'
fi

# Prepare build script
cat >build_android.sh <<EOF
set -xe

mkdir -p _android-amd64
make \
 -C _android-amd64 \
 -f ../test/Makefile \
 SYS=android \
 ROOT_DIR=../.. \
 SDK_DIR=/Android \
 NDK_VER=$NDK_VER \
 FFAUDIO_API=aaudio \
 $@
EOF

# Build inside the container
podman start --attach ffaudio_android_build
