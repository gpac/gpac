#!/bin/sh


version="`grep '#define GPAC_VERSION ' \"./include/gpac/version.h\" | cut -d '"' -f 2`"
version_major=`grep '#define GPAC_VERSION_MAJOR ' ./include/gpac/version.h | sed 's/[^0-9]*//g'`
version_minor=`grep '#define GPAC_VERSION_MINOR ' ./include/gpac/version.h | sed 's/[^0-9]*//g'`
version_micro=`grep '#define GPAC_VERSION_MICRO ' ./include/gpac/version.h | sed 's/[^0-9]*//g'`
libgac_version="$version_major.$version_minor.$version_micro"

echo "Version is $version - libgpac $libgac_version"

#patch README.md
source="README.md"
sed -e "s/README for GPAC version.*/README for GPAC version $version/;" $source > ftmp
rm $source
mv ftmp $source


#patch  applications/gpac/ios-Info.plist
source="applications/gpac/ios-Info.plist"
sed -e "/CFBundleShortVersionString/{n;s/.*/	<string>$version<\/string>/;}" $source | sed -e "/CFBundleVersion/{n;s/.*/	<string>$libgac_version<\/string>/;}" > ftmp
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

#patch  packagers/osx/GPAC.app/Contents/Info.plist
source="packagers/osx/GPAC.app/Contents/Info.plist"
sed -e "/CFBundleShortVersionString/{n;s/.*/	<string>$version<\/string>/;}" $source | sed -e "/CFBundleVersion/{n;s/.*/	<string>$libgac_version<\/string>/;}" > ftmp
rm $source
mv ftmp $source

