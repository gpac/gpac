#!/bin/sh

echo "*** Compile libgpac for Simulator (i386) ***"
xcodebuild -target libgpac_dynamic -sdk iphonesimulator -configuration Release -project gpac4ios.xcodeproj

echo "*** Compile libgpac for iOS (arm) ***"
xcodebuild -target libgpac_dynamic -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj
./script_libgpac.sh Release
#xcodebuild -target libgpac_dynamic -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj

echo "*** Compile modules and osmo4ios for Simulator (i386) ***"
xcodebuild -alltargets -sdk iphonesimulator -configuration Release -project gpac4ios.xcodeproj

echo "*** Compile modules for iOS (arm) ***"
xcodebuild -alltargets -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj
./script_modules.sh Release
#xcodebuild -alltargets -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj

echo "*** Compile osmo4ios for Simulator (arm) ***"
#link must occur at debug to avoid optimizing that leads to freezes
xcodebuild -target osmo4ios -sdk iphoneos -configuration Debug -project gpac4ios.xcodeproj

echo "*** Copy the generated libs (arm only) ***"
rm -f ../../bin/iOS/osmo4ios.app/*
cp build/Release-iphoneos/*.dylib ../../bin/iOS/osmo4ios.app/
cp build/Debug-iphoneos/osmo4ios.app/osmo4ios ../../bin/iOS/osmo4ios.app/
cp build/Debug-iphoneos/osmo4ios.app/PkgInfo ../../bin/iOS/osmo4ios.app/
cp build/Debug-iphoneos/osmo4ios.app/Info.plist ../../bin/iOS/osmo4ios.app/

echo "*** Test the presence of target files ***"
if [ `ls ../../bin/iOS/osmo4ios.app/ | wc -l` -ne 22 ]
then
	echo "Error: target files number not correct (expected 22, got `ls ../../bin/iOS/osmo4ios.app/ | wc -l`)"
else
	echo "*** Extra Libs generation for iOS completed! ***"
fi

