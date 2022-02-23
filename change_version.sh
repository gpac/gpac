#!/bin/sh


version="`grep '#define GPAC_VERSION ' \"./include/gpac/version.h\" | cut -d '"' -f 2`"
#version_major=`grep '#define GPAC_VERSION_MAJOR ' ./include/gpac/version.h | sed 's/[^0-9]*//g'`
#version_minor=`grep '#define GPAC_VERSION_MINOR ' ./include/gpac/version.h | sed 's/[^0-9]*//g'`
#version_micro=`grep '#define GPAC_VERSION_MICRO ' ./include/gpac/version.h | sed 's/[^0-9]*//g'`

echo "Version is $version"

#patch README.md
source="README.md"
sed -e "s/README for GPAC version.*/README for GPAC version $version/;" $source > ftmp
rm $source
mv ftmp $source


#patch  applications/osmo4_ios/osmo4ios-Info.plist 

source="applications/osmo4_ios/osmo4ios-Info.plist"
sed -e "/CFBundleShortVersionString/{n;s/.*/	<string>$version<\/string>/;}" $source > ftmp
rm $source
mv ftmp $source

# patch file gpac.pc
source="gpac.pc"
sed -e "s/Version:.*/Version:$version/;" $source > ftmp
rm $source
mv ftmp $source

# patch file gpac.spec
source="gpac.spec"
sed -e "s/Version:.*/Version: $version/;" $source | sed -e "s/Release:.*/Release: $version/;" | sed -e "s/Source0:.*/Source0: gpac-$version.tar.gz/;" > ftmp
rm $source
mv ftmp $source

# patch file debian/changelog
source="debian/changelog"
sed -e "s/gpac (.*/gpac ($version) stable; urgency=low/;" $source | sed -e "s/Check https.*/Check https:\/\/github.com\/gpac\/gpac\/releases\/tag\/v$version/;" > ftmp
rm $source
mv ftmp $source

# patch packagers/win32_64/nsis/gpac_installer.nsi
source="packagers/win32_64/nsis/gpac_installer.nsi"
sed -e "s/\!define GPAC_VERSION.*/\!define GPAC_VERSION $version/;" $source > ftmp
rm $source
mv ftmp $source

# patch file share/doc/configuration.html
source="share/doc/configuration.html"
sed -e "s/GPAC Version.*</GPAC Version $version</;" $source > ftmp
rm $source
mv ftmp $source
