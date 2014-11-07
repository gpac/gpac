#!/bin/sh

cd "`dirname $0`"

echo "*** Set version within Info.plist application file ***"
version=`grep '#define GPAC_VERSION ' ../../include/gpac/version.h | cut -d '"' -f 2`
rev=`LANG=en_US svn info | grep Revision | tr -d 'Revision: '`
if [ "$rev" != "" ]
then
#	svn revert ../../applications/osmo4_ios/osmo4ios-Info.plist
	sed 's/<string>.*<\/string><!-- VERSION_REV_REPLACE -->/<string>'"$version"'<\/string>/' ../../applications/osmo4_ios/osmo4ios-Info.plist > ../../applications/osmo4_ios/osmo4ios-Info.plist.new
	sed 's/<string>.*<\/string><!-- BUILD_REV_REPLACE -->/<string>'"$rev"'<\/string>/' ../../applications/osmo4_ios/osmo4ios-Info.plist.new > ../../applications/osmo4_ios/osmo4ios-Info.plist
	rm ../../applications/osmo4_ios/osmo4ios-Info.plist.new
fi

echo "*** Clean previous build files ***"
#xcodebuild -alltargets -sdk iphoneos -configuration Debug -project gpac4ios.xcodeproj clean
xcodebuild -alltargets -sdk iphoneos -configuration Release -project gpac4ios.xcodeproj clean
if [ $? != 0 ] ; then
	exit 1
fi

echo "*** Compile osmo4ios for iOS (arm) ***"
#link must occur at debug to avoid optimizing that leads to freezes
#xcodebuild -target osmo4ios -sdk iphoneos -configuration Debug -project gpac4ios.xcodeproj
xcodebuild -target osmo4ios -configuration Release -project gpac4ios.xcodeproj
if [ $? != 0 ] ; then
	exit 1
fi

echo "*** Copy the generated libs (arm only) ***"
cp build/Release-iphoneos/osmo4ios.app/osmo4ios ../../bin/iOS/osmo4ios.app/
cp build/Release-iphoneos/osmo4ios.app/PkgInfo ../../bin/iOS/osmo4ios.app/
cp build/Release-iphoneos/osmo4ios.app/Info.plist ../../bin/iOS/osmo4ios.app/
cp build/Release-iphoneos/osmo4ios.app/ResourceRules.plist ../../bin/iOS/osmo4ios.app/

echo "*** Test the presence of target files ***"
#if [ `ls ../../bin/iOS/osmo4ios.app/ | wc -l` -ne 5 ]
#then
#	echo "Error: target files number not correct (expected 24, got `ls ../../bin/iOS/osmo4ios.app/ | wc -l`)"
#	exit 1
#fi

#echo "*** Build archive name ***"
cd ../..
if [ "$rev" != "" ]
then
	full_version="$version-r$rev"
else
	#if no revision can be extracted from SVN, use date
	full_version="$version-$(date +%Y%m%d)"
fi

echo "*** Generate an archive and clean ***"
cd bin/iOS
mkdir osmo4ios.app/gui
mkdir osmo4ios.app/gui/icons
mkdir osmo4ios.app/gui/extensions
cp ../../applications/osmo4_ios/Resources/icon*.png osmo4ios.app/
cp ../../gui/gui*.bt osmo4ios.app/gui/
cp ../../gui/gui*.js osmo4ios.app/gui/
cp ../../gui/gwlib.js osmo4ios.app/gui/
cp ../../gui/mpegu-core.js osmo4ios.app/gui/
cp -r ../../gui/icons osmo4ios.app/gui/
cp -r ../../gui/extensions osmo4ios.app/gui/
find osmo4ios.app | fgrep .svn | fgrep -v svn/ | xargs rm -rf 
tar -czf "osmo4ios-$full_version.tar.gz" osmo4ios.app/
rm -rf osmo4ios.app
svn up -r $rev
cd ../../build/xcode/

echo "*** GPAC generation for iOS completed ($full_version)! ***"
