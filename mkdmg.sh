#!/bin/sh -e

source_path=.

function rewrite_deps {
# echo rewriting deps for $1
for ref in `otool -L $1 | grep '/local' | awk '{print $1}'`
do
# echo changing $ref to @executable_path/lib/`basename $ref` $1
  install_name_tool -change $ref @executable_path/lib/`basename $ref` $1 || { echo "Failed, permissions issue for $1 ? Try with sudo..." ; exit 1 ;}
  copy_lib $ref
done

}

function copy_lib {
# echo testing $1 for bundle copy
basefile=`basename $1`
if [ ! $basefile == 'libgpac.dylib' ] &&  [ ! -e lib/$basefile ];
then
#  echo copying $1 to bundle
  cp $1 lib/
  rewrite_deps lib/$basefile
fi
}

#copy all libs
echo Copying binaries
if [ -d tmpdmg ]
then
rm -fr tmpdmg
fi
mkdir -p tmpdmg/Osmo4.app
rsync -r --exclude=.svn $source_path/build/osxdmg/Osmo4.app/ ./tmpdmg/Osmo4.app/
ln -s /Applications ./tmpdmg/Applications
cp $source_path/README ./tmpdmg
cp $source_path/COPYING ./tmpdmg

cp bin/gcc/gm* tmpdmg/Osmo4.app/Contents/MacOS/modules
cp bin/gcc/libgpac.dylib tmpdmg/Osmo4.app/Contents/MacOS/lib
cp bin/gcc/MP4Client tmpdmg/Osmo4.app/Contents/MacOS/Osmo4
cp bin/gcc/MP4Box tmpdmg/Osmo4.app/Contents/MacOS/MP4Box
cp bin/gcc/MP42TS tmpdmg/Osmo4.app/Contents/MacOS/MP42TS
if [ -f bin/gcc/DashCast ]
then
cp bin/gcc/DashCast tmpdmg/Osmo4.app/Contents/MacOS/DashCast
fi

cd tmpdmg/Osmo4.app/Contents/MacOS/

#check all external deps, and copy them
echo rewriting DYLIB dependencies
for dylib in lib/*.dylib modules/*.dylib
do
  rewrite_deps $dylib
done

if [ -f DashCast ]
  rewrite_deps DashCast
then
fi

echo rewriting APPS dependencies
install_name_tool -change /usr/local/lib/libgpac.dylib @executable_path/lib/libgpac.dylib Osmo4
install_name_tool -change /usr/local/lib/libgpac.dylib @executable_path/lib/libgpac.dylib MP4Box
install_name_tool -change /usr/local/lib/libgpac.dylib @executable_path/lib/libgpac.dylib MP42TS
install_name_tool -change ../bin/gcc/libgpac.dylib @executable_path/lib/libgpac.dylib Osmo4
install_name_tool -change ../bin/gcc/libgpac.dylib @executable_path/lib/libgpac.dylib MP4Box
install_name_tool -change ../bin/gcc/libgpac.dylib @executable_path/lib/libgpac.dylib MP42TS

if [ -f DashCast ]
then
install_name_tool -change /usr/local/lib/libgpac.dylib @executable_path/lib/libgpac.dylib DashCast
install_name_tool -change ../bin/gcc/libgpac.dylib @executable_path/lib/libgpac.dylib DashCast
fi

cd ../../../..

echo Copying GUI
rsync -r --exclude=.svn $source_path/gui ./tmpdmg/Osmo4.app/Contents/MacOS/

echo Building DMG
version=`grep '#define GPAC_VERSION ' $source_path/include/gpac/version.h | cut -d '"' -f 2`

cur_dir=`pwd`
cd $source_path
rev=`LANG=en_US svn info | grep Revision | tr -d 'Revision: '`
cd $cur_dir

full_version=$version
if [ "$rev" != "" ]
then
	full_version="$full_version-r$rev"
else
	#if no revision can be extracted from SVN, use date
   	$rev = $(date +%Y%m%d)
fi

sed 's/<string>.*<\/string><!-- VERSION_REV_REPLACE -->/<string>'"$version"'<\/string>/' tmpdmg/Osmo4.app/Contents/Info.plist > tmpdmg/Osmo4.app/Contents/Info.plist.new && sed 's/<string>.*<\/string><!-- BUILD_REV_REPLACE -->/<string>'"$rev"'<\/string>/' tmpdmg/Osmo4.app/Contents/Info.plist.new > tmpdmg/Osmo4.app/Contents/Info.plist && rm tmpdmg/Osmo4.app/Contents/Info.plist.new

#create dmg
hdiutil create ./gpac.dmg -volname "GPAC for OSX"  -srcfolder tmpdmg -ov
rm -rf ./tmpdmg

#add SLA
echo "Adding licence"
hdiutil convert -format UDCO -o gpac_sla.dmg gpac.dmg
rm gpac.dmg
hdiutil unflatten gpac_sla.dmg
Rez /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers/*.r $source_path/build/osxdmg/SLA.r -a -o gpac_sla.dmg
hdiutil flatten gpac_sla.dmg
hdiutil internet-enable -yes gpac_sla.dmg

echo "GPAC-$full_version.dmg ready"
chmod o+rx gpac_sla.dmg
chmod g+rx gpac_sla.dmg
mv gpac_sla.dmg GPAC-$full_version.dmg

