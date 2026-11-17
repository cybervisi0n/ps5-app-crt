#!/bin/bash

# Run me from the root of the repo!

PAYLOAD_SDK_URL=https://github.com/ps5-payload-dev/sdk/releases/latest/download/ps5-payload-sdk.zip
PACBREW_RELEASE_URL=https://github.com/ps5-payload-dev/pacbrew-repo/releases/latest/download/ps5-payload-dev.tar.gz

mkdir -p deps/

wget $PAYLOAD_SDK_URL
unzip -o ./ps5-payload-sdk.zip -d deps
rm -f ps5-payload-sdk.zip

mkdir -p deps/include
mkdir -p deps/lib
