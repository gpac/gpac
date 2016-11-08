#!/bin/sh

cd "`dirname $0`"

echo "*** Set version within Info.plist application file ***"
version=`grep '#define GPAC_VERSION ' ../../include/gpac/version.h | cut -d '"' -f 2`
TAG=$(git describe --tags --abbrev=0 2> /dev/null)
REVISION=$(echo `git describe --tags --long 2> /dev/null || echo "UNKNOWN"` | sed "s/^$TAG-//")
BRANCH=$(git rev-parse --abbrev-ref HEAD 2> /dev/null || echo "UNKNOWN")
rev="$REVISION-$BRANCH"
if [ "$rev" != "" ]
then
	sed 's/<string>.*<\/string><!-- VERSION_REV_REPLACE -->/<string>'"$version"'<\/string>/' ../../applications/osmo4_ios/osmo4ios-Info.plist > ../../applications/osmo4_ios/osmo4ios-Info.plist.new
	sed 's/<string>.*<\/string><!-- BUILD_REV_REPLACE -->/<string>'"$rev"'<\/string>/' ../../applications/osmo4_ios/osmo4ios-Info.plist.new > ../../applications/osmo4_ios/osmo4ios-Info.plist
	rm ../../applications/osmo4_ios/osmo4ios-Info.plist.new
fi

if [ "$rev" != "" ]
then
	full_version="$version-rev$rev"
else
	#if no revision can be extracted, use date
	full_version="$version-$(date +%Y%m%d)"
fi

echo "*** Compile and archive osmo4ios ***"
xcodebuild archive -project gpac4ios.xcodeproj -scheme osmo4ios -archivePath osmo4ios.xcarchive
if [ $? != 0 ] ; then
	exit 1
fi

echo "*** Generate IPA ***"
mkdir -p Payload
mv osmo4ios.xcarchive/Products/Applications/osmo4ios.app Payload/
zip -r "../../bin/iOS/osmo4-$full_version-ios.ipa" Payload
rm -rf Payload
rm -rf osmo4ios.xcarchive
#git pull

echo "*** GPAC generation for iOS completed ($full_version)! ***"
