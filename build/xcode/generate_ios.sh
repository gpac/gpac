#!/bin/sh

echo "*** Clean previous build files ***"
xcodebuild -alltargets -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj clean

#echo "*** Compile libgpac for Simulator (i386) ***"
xcodebuild -target libgpac_dynamic -sdk iphonesimulator -configuration Release -project gpac4ios.xcodeproj

echo "*** Compile libgpac for iOS (arm) ***"
xcodebuild -target libgpac_dynamic -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj
./script_libgpac.sh Release
#xcodebuild -target libgpac_dynamic -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj

#echo "*** Compile modules and osmo4ios for Simulator (i386) ***"
#xcodebuild -alltargets -parallelizeTargets -sdk iphonesimulator -configuration Release -project gpac4ios.xcodeproj

echo "*** Compile modules for iOS (arm) ***"
xcodebuild -alltargets -parallelizeTargets -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj
./script_modules.sh Release

echo "*** Compile osmo4ios for iOS (arm) ***"
#link must occur at debug to avoid optimizing that leads to freezes
xcodebuild -target osmo4ios -sdk iphoneos -configuration Debug -project gpac4ios.xcodeproj

echo "*** Copy the generated libs (arm only) ***"
cp build/Release-iphoneos/*.dylib ../../bin/iOS/osmo4ios.app/
cp build/Debug-iphoneos/osmo4ios.app/osmo4ios ../../bin/iOS/osmo4ios.app/
cp build/Debug-iphoneos/osmo4ios.app/PkgInfo ../../bin/iOS/osmo4ios.app/
cp build/Debug-iphoneos/osmo4ios.app/Info.plist ../../bin/iOS/osmo4ios.app/

echo "*** Test the presence of target files ***"
if [ `ls ../../bin/iOS/osmo4ios.app/ | wc -l` -ne 22 ]
then
	echo "Error: target files number not correct (expected 22, got `ls ../../bin/iOS/osmo4ios.app/ | wc -l`)"
	exit 1
fi

echo "*** Generate an archive and clean ***"
cd ../..
version=`grep '#define GPAC_VERSION ' include/gpac/tools.h | cut -d '"' -f 2`
rev=`LANG=en_US svn info | grep Revision | tr -d 'Revision: '`
if [ "$rev" != "" ]
then
	full_version="$version-r$rev"
else
        #if no revision can be extracted from SVN, use date
	full_version="$version-$(date +%Y%m%d)"
fi
cd bin/iOS
rm -rf osmo4ios.app/.svn
tar -czf "osmo4ios-$full_version.tar.gz" osmo4ios.app/
rm -rf osmo4ios.app
cd ../../build/xcode/

echo "*** Extra Libs generation for iOS completed (full_version)! ***"
