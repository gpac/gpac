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
cp bin/gcc/gf_* tmpdmg/GPAC.app/Contents/MacOS/modules
cp bin/gcc/libgpac.dylib tmpdmg/GPAC.app/Contents/MacOS/lib
if [ -f bin/gcc/libopenhevc.1.dylib ]; then
    cp bin/gcc/libopenhevc.1.dylib tmpdmg/GPAC.app/Contents/MacOS/lib
fi
cp bin/gcc/MP4Box tmpdmg/GPAC.app/Contents/MacOS/MP4Box
cp bin/gcc/gpac tmpdmg/GPAC.app/Contents/MacOS/gpac

cd tmpdmg/GPAC.app/Contents/MacOS/

#check all external deps, and copy them
echo rewriting DYLIB dependencies
for dylib in lib/*.dylib modules/*.dylib
do
  rewrite_deps $dylib
done

echo rewriting APPS dependencies
install_name_tool -change /usr/local/lib/libgpac.dylib @executable_path/lib/libgpac.dylib MP4Box
install_name_tool -change /usr/local/lib/libgpac.dylib @executable_path/lib/libgpac.dylib gpac
install_name_tool -change ../bin/gcc/libgpac.dylib @executable_path/lib/libgpac.dylib MP4Box
install_name_tool -change ../bin/gcc/libgpac.dylib @executable_path/lib/libgpac.dylib gpac

cd ../../../..

echo Copying shared resources
rsync -r --exclude=.git $source_path/share/res ./tmpdmg/GPAC.app/Contents/MacOS/share/
rsync -r --exclude=.git $source_path/share/gui ./tmpdmg/GPAC.app/Contents/MacOS/share/
rsync -r --exclude=.git $source_path/share/vis ./tmpdmg/GPAC.app/Contents/MacOS/share/
rsync -r --exclude=.git $source_path/share/shaders ./tmpdmg/GPAC.app/Contents/MacOS/share/
rsync -r --exclude=.git $source_path/share/scripts ./tmpdmg/GPAC.app/Contents/MacOS/share/
rsync -r --exclude=.git $source_path/share/python ./tmpdmg/GPAC.app/Contents/MacOS/share/
cp $source_path/share/default.cfg ./tmpdmg/GPAC.app/Contents/MacOS/share/

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
   	rev = $(date +%Y%m%d)
fi

sed 's/<string>.*<\/string><!-- VERSION_REV_REPLACE -->/<string>'"$version"'<\/string>/' tmpdmg/GPAC.app/Contents/Info.plist > tmpdmg/GPAC.app/Contents/Info.plist.new && sed 's/<string>.*<\/string><!-- BUILD_REV_REPLACE -->/<string>'"$rev"'<\/string>/' tmpdmg/GPAC.app/Contents/Info.plist.new > tmpdmg/GPAC.app/Contents/Info.plist && rm tmpdmg/GPAC.app/Contents/Info.plist.new

#GPAC.app now ready, build pkg
echo "Building PKG"
find ./tmpdmg/ -name '*.DS_Store' -type f -delete
chmod -R 755 ./tmpdmg/
xattr -rc ./tmpdmg/

pkgbuild --install-location /Applications --component ./tmpdmg/GPAC.app --scripts ./packagers/osx/scripts --ownership preserve ./tmppkg.pkg
cat ./packagers/osx/res/preamble.txt > ./packagers/osx/res/full_license.txt
cat ./COPYING >> ./packagers/osx/res/full_license.txt

productbuild --distribution packagers/osx/distribution.xml --resources ./packagers/osx/res --package-path ./tmppkg.pkg gpac.pkg

rm ./packagers/osx/res/full_license.txt

pck_name="gpac-$full_version.pkg"
if [ "$1" = "snow-leopard" ]; then
pck_name="gpac-$full_version-$1.pkg"
fi
mv gpac.pkg $pck_name
chmod 755 $pck_name

echo "$pck_name ready"
rm -rf tmpdmg
rm -rf tmppkg.pkg


