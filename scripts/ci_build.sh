#!/bin/bash

./scripts/download_dependencies.sh
./setup_build_meson.sh
meson compile -C build
./scripts/package_ps5_libs.sh build ps5-app-crt_release
