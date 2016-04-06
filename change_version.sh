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

#patch doc/configuration.html
source="doc/configuration.html"
sed -e "s/GPAC Version.*/GPAC Version $version/;" $source > ftmp
rm $source
mv ftmp $source

#patch doc/man/gpac.1
source="doc/man/gpac.1"
sed -e "s/GPAC framework version.*/GPAC framework version $version./;" $source > ftmp
rm $source
mv ftmp $source


#patch  applications/osmo4_ios/osmo4ios-Info.plist 

source="applications/osmo4_ios/osmo4ios-Info.plist"
sed -e "/CFBundleShortVersionString/{n;s/.*/	<string>$version<\/string>/;}" $source > ftmp
rm $source
mv ftmp $source

# patch applications/osmo4_w32/Osmo4.rc

source="applications/osmo4_w32/Osmo4.rc"
sed -e "/\"FileDescription\", \"Osmo4-GPAC\"/{n;s/.*/		VALUE \"FileVersion\", \"$version\"/;}" $source | sed -e "/\"ProductName\", \"Osmo4-GPAC\"/{n;s/.*/		VALUE \"ProductVersion\", \"$version\"/;}" > ftmp
rm $source
mv ftmp $source

# patch bin/smartphone 2003 (armv4)/release/install/archive.bat
source="bin/smartphone 2003 (armv4)/release/install/archive.bat"
sed -e "s/set gpac_version=.*/set gpac_version=\"$version-r%gpac_revision%/;" "$source" > ftmp
rm "$source"
mv ftmp "$source"


# patch bin/smartphone 2003 (armv4)/release/install/build_installer.bat
source="bin/smartphone 2003 (armv4)/release/install/build_installer.bat"
sed -e "s/set gpac_version=.*/set gpac_version=\"$version-r%gpac_revision%/;" "$source" > ftmp
rm "$source"
mv ftmp "$source"

# patch file gpac.pc
source="gpac.pc"
sed -e "s/Version:.*/Version:$version/;" $source > ftmp
rm $source
mv ftmp $source

# patch file gpac.spec
source="gpac.spec"
sed -e "s/Version:.*/Version: $version/;" $source | sed -e "s/Release:.*/Release: $version/;" | sed -e "s/Source0:.*/Source0: gpac-$version.tar.gz%{?_with_amr:Source1:http:\/\/www.3gpp.org\/ftp\/Specs\/archive\/26_series\/26.073\/26073-700.zip}/;" > ftmp
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
