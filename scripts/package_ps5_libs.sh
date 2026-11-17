#!/bin/bash

# usage package_ps5_libs.sh builddir packagename

BuildDir=$1
PackageName=$2

rm -rf $BuildDir/$PackageName

mkdir -p $BuildDir/$PackageName/lib

cd $BuildDir
for lib in `find -name '*.a'`;
    do ar -t $lib | xargs ar rvs $lib.new && mv -v $lib.new $lib;
done
cd ../

# Copy libs
cp $BuildDir/libps5_app_crt.a $BuildDir/$PackageName/lib/

