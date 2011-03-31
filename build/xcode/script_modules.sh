#!/bin/sh
#set -x

for i in `ls ../../modules/`; do /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo -create -output build/$1-iphoneos/gm_$i.dylib build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv6/gm_$i.dylib build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv7/gm_$i.dylib ; done
