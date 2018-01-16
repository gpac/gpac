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
  chmod +w lib/$basefile
  rewrite_deps lib/$basefile
fi
}

#copy all libs
echo Copying binaries
if [ -d tmpdmg ]
then
rm -fr tmpdmg
fi
mkdir -p tmpdmg/GPAC.app
rsync -r --exclude=.git $source_path/packagers/osx/GPAC.app/ ./tmpdmg/GPAC.app/
ln -s /Applications ./tmpdmg/Applications
cp $source_path/README.md ./tmpdmg
cp $source_path/COPYING ./tmpdmg

mkdir -p tmpdmg/GPAC.app/Contents/MacOS/modules
mkdir -p tmpdmg/GPAC.app/Contents/MacOS/lib

cp bin/gcc/gm* tmpdmg/GPAC.app/Contents/MacOS/modules
cp bin/gcc/libgpac.dylib tmpdmg/GPAC.app/Contents/MacOS/lib
cp bin/gcc/MP4Client tmpdmg/GPAC.app/Contents/MacOS/Osmo4
cp bin/gcc/MP4Box tmpdmg/GPAC.app/Contents/MacOS/MP4Box
cp bin/gcc/MP42TS tmpdmg/GPAC.app/Contents/MacOS/MP42TS
if [ -f bin/gcc/DashCast ]
then
cp bin/gcc/DashCast tmpdmg/GPAC.app/Contents/MacOS/DashCast
fi

cd tmpdmg/GPAC.app/Contents/MacOS/

#check all external deps, and copy them
echo rewriting DYLIB dependencies
for dylib in lib/*.dylib modules/*.dylib
do
  rewrite_deps $dylib
done

if [ -f DashCast ]
then
  rewrite_deps DashCast
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
rsync -r --exclude=.git $source_path/gui ./tmpdmg/GPAC.app/Contents/MacOS/
rsync -r --exclude=.git $source_path/shaders ./tmpdmg/GPAC.app/Contents/MacOS/

echo Building DMG
version=`grep '#define GPAC_VERSION ' $source_path/include/gpac/version.h | cut -d '"' -f 2`

cur_dir=`pwd`
cd $source_path
TAG=$(git describe --tags --abbrev=0 2> /dev/null)
REVISION=$(echo `git describe --tags --long 2> /dev/null || echo "UNKNOWN"` | sed "s/^$TAG-//")
BRANCH=$(git rev-parse --abbrev-ref HEAD 2> /dev/null || echo "UNKNOWN")
rev="$REVISION-$BRANCH"
cd $cur_dir

full_version=$version
if [ "$rev" != "" ]
then
	full_version="$full_version-rev$rev"
else
	#if no revision can be extracted, use date
   	$rev = $(date +%Y%m%d)
fi

sed 's/<string>.*<\/string><!-- VERSION_REV_REPLACE -->/<string>'"$version"'<\/string>/' tmpdmg/GPAC.app/Contents/Info.plist > tmpdmg/GPAC.app/Contents/Info.plist.new && sed 's/<string>.*<\/string><!-- BUILD_REV_REPLACE -->/<string>'"$rev"'<\/string>/' tmpdmg/GPAC.app/Contents/Info.plist.new > tmpdmg/GPAC.app/Contents/Info.plist && rm tmpdmg/GPAC.app/Contents/Info.plist.new

#GPAC.app now ready, build pkg
echo "Building PKG"
find ./tmpdmg/ -name '*.DS_Store' -type f -delete
chmod -R 755 ./tmpdmg/
xattr -rc ./tmpdmg/

pkgbuild --install-location /Applications --root ./tmpdmg/ --scripts ./packagers/osx/scripts --ownership preserve ./tmppkg.pkg
cat ./packagers/osx/res/preamble.txt > ./packagers/osx/res/full_license.txt
cat ./COPYING >> ./packagers/osx/res/full_license.txt

productbuild --distribution packagers/osx/distribution.xml --resources ./packagers/osx/res --package-path ./tmppkg.pkg gpac.pkg

rm ./packagers/osx/res/full_license.txt

pck_name="gpac-$full_version.pkg"
if [ "$1" = "snow-leopard" ]; then
pck_name="gpac-$full_version-$1.pkg"
fi
mv gpac.pkg $pck_name

echo "$pck_name ready"
rm -rf tmpdmg
rm -rf tmppkg.pkg


