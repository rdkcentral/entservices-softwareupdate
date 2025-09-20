#!/usr/bin/env bash
set -euo pipefail

# Directory setup
WORKSPACE="$(pwd)"   # or set explicitly to the repo root
INSTALL_DIR="$WORKSPACE/install"
BUILD_DIR="$WORKSPACE/build"

echo "=== Update & install required packages ==="
sudo apt update
sudo apt-get install -y \
  pkg-config libglib2.0-dev libnm-dev libcurl4-openssl-dev \
  lcov ninja-build libgupnp-1.2-1 libgupnp-1.2-dev \
  libgssdp-1.2-0 libsoup2.4-1 python3-pip

echo "=== Install Python module jsonref ==="
pip install --upgrade pip
pip install jsonref

echo "=== Install CMake 3.16.x if needed ==="
# If your distro CMake is already >=3.16 you can skip this
# Example using pip:
pip install "cmake>=3.16,<3.17"

echo "=== Clone Thunder repositories ==="
mkdir -p "$WORKSPACE"
cd "$WORKSPACE"

# Only clone if the directories don’t exist (imitates cache behavior)
if [ ! -d Thunder ]; then
  git clone --branch "$THUNDER_REF" https://github.com/rdkcentral/Thunder
fi
if [ ! -d ThunderTools ]; then
  git clone --branch "$THUNDER_REF" https://github.com/rdkcentral/ThunderTools
fi
if [ ! -d ThunderInterfaces ]; then
  git clone --branch "$THUNDER_REF" https://github.com/rdkcentral/ThunderInterfaces
fi

# Checkout networkmanager (assuming this repo already contains it)
if [ ! -d networkmanager ]; then
  git clone "$GITHUB_SERVER_URL/$GITHUB_REPOSITORY" networkmanager
fi

echo "=== Apply Thunder patch ==="
cd "$WORKSPACE/Thunder"
git apply "$WORKSPACE/networkmanager/tests/patches/thunder/SubscribeStub.patch" || true

echo "=== Build ThunderTools ==="
cmake -S "$WORKSPACE/ThunderTools" -B "$BUILD_DIR/ThunderTools" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/usr" \
  -DCMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake" \
  -DGENERIC_CMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake"
cmake --build "$BUILD_DIR/ThunderTools" --target install -j"$(nproc)"

echo "=== Build Thunder ==="
cmake -S "$WORKSPACE/Thunder" -B "$BUILD_DIR/Thunder" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/usr" \
  -DCMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake" \
  -DBUILD_TYPE=Debug \
  -DBINDING=127.0.0.1 \
  -DPORT=9998
cmake --build "$BUILD_DIR/Thunder" --target install -j"$(nproc)"

echo "=== Build ThunderInterfaces ==="
cmake -S "$WORKSPACE/ThunderInterfaces" -B "$BUILD_DIR/ThunderInterfaces" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/usr" \
  -DCMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake"
cmake --build "$BUILD_DIR/ThunderInterfaces" --target install -j"$(nproc)"

echo "=== Generate IARM headers ==="
mkdir -p "$INSTALL_DIR/usr/lib"
touch "$INSTALL_DIR/usr/lib/libIARMBus.so"

mkdir -p "$INSTALL_DIR/usr/include/rdk/iarmbus"
touch "$INSTALL_DIR/usr/include/rdk/iarmbus/libIARM.h"

mkdir -p "$WORKSPACE/networkmanager/tests/headers/rdk/iarmbus"
touch "$WORKSPACE/networkmanager/tests/headers/rdk/iarmbus/libIARM.h"
touch "$WORKSPACE/networkmanager/tests/headers/rdk/iarmbus/libIBus.h"

echo "=== Build networkmanager with RDK Proxy ==="
cmake -S "$WORKSPACE/networkmanager" -B "$BUILD_DIR/networkmanager_rdk" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/usr" \
  -DCMAKE_MODULE_PATH="$INSTALL_DIR/tools/cmake" \
  -DCMAKE_CXX_FLAGS="\
    -I $WORKSPACE/networkmanager/tests/headers \
    -I $WORKSPACE/networkmanager/tests/headers/rdk/iarmbus \
    --include $WORKSPACE/networkmanager/tests/mocks/Iarm.h \
    --include $WORKSPACE/networkmanager/tests/mocks/secure_wrappermock.h \
    -Wall --coverage" \
  -DENABLE_UNIT_TESTING=ON \
  -DENABLE_ROUTER_DISCOVERY_TOOL=OFF
cmake --build "$BUILD_DIR/networkmanager_rdk" --target install -j"$(nproc)"

echo "=== Run unit tests for Legacy_Network ==="
export PATH="$INSTALL_DIR/usr/bin:$PATH"
export LD_LIBRARY_PATH="$INSTALL_DIR/usr/lib:$INSTALL_DIR/usr/lib/wpeframework/plugins:${LD_LIBRARY_PATH:-}"
cd "$WORKSPACE"
legacynetwork_tests
