#!/bin/sh

version="`grep '#define GPAC_VERSION ' \"./include/gpac/version.h\" | cut -d '"' -f 2`"

#check for .git - if so use nb commits till last tag for rev + commit id
if [ -d ".git" ]; then
TAG=$(git describe --tags --abbrev=0 2>>gtmp)
VERSION=$(echo `git describe --tags --long 2>>gtmp || echo "UNKNOWN"` | sed "s/^$TAG-//")
BRANCH=$(git rev-parse --abbrev-ref HEAD 2>>gtmp || echo "UNKNOWN")
revision="$VERSION-$BRANCH"

rm gtmp

echo "#define GPAC_GIT_REVISION	\"$revision\"" > htmp

if ! diff htmp ./include/gpac/revision.h > /dev/null ; then
echo "revision has changed"
rm ./include/gpac/revision.h
mv htmp ./include/gpac/revision.h
else
rm htmp
fi

else
#otherwise, check id -DEV is in the version name. If not consider this a release

if [ ! -e ".include/gpac/revision.h" ]; then

if test "$version" != *"-DEV"* ; then
echo "#define GPAC_GIT_REVISION \"release\"" > ./include/gpac/revision.h
else
echo "#define GPAC_GIT_REVISION \"UNKNOWN_REV\"" > ./include/gpac/revision.h
fi

fi

fi
