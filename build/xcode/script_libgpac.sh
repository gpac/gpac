#!/bin/sh

/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo -create -output build/Release-iphoneos/libgpac_dynamic.dylib build/gpac4ios.build/Release-iphoneos/libgpac_dynamic.build/Objects-normal/armv6/libgpac_dynamic.dylib build/gpac4ios.build/Release-iphoneos/libgpac_dynamic.build/Objects-normal/armv7/libgpac_dynamic.dylib

cp build/Release-iphoneos/libgpac_dynamic.dylib ../../bin/iOS/osmo4ios.app/
