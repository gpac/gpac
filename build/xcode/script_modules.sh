#i could not understand how apple handles armv6 and armv7...

#for projects building armv6 ok
for i in `ls ../../modules/`; do /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo -create -output build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv7/gm_$i.dylib build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv7/gm_$i.dylib.libtool.armv7 build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv6/gm_$i.dylib.libtool.armv6 ; done

#for projects completely ko:
#1. add armv6
for i in `ls ../../modules/`; do /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo -create -output build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv6/gm_$i.dylib build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv6/gm_$i.dylib.libtool.armv6 build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv7/gm_$i.dylib.libtool.armv7  ; done
#2. hack armv6 build
for i in `ls ../../modules/`; do /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/lipo -create -output build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv7/gm_$i.dylib build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv7/gm_$i.dylib.libtool.armv7 build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv6/gm_$i.dylib ; done
#3. copy armv7 build in the bin dir
for i in `ls ../../modules/`; do cp build/gpac4ios.build/$1-iphoneos/$i.build/Objects-normal/armv7/gm_$i.dylib build/$1-iphoneos/gm_$i.dylib ; done
