#!/bin/sh

/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo -create -output build/$1-iphoneos/libgpac_dynamic.dylib build/gpac4ios.build/$1-iphoneos/libgpac_dynamic.build/Objects-normal/armv6/libgpac_dynamic.dylib build/gpac4ios.build/$1-iphoneos/libgpac_dynamic.build/Objects-normal/armv7/libgpac_dynamic.dylib

cp build/$1-iphoneos/libgpac_dynamic.dylib ../../bin/iOS/gpac4ios.app/
