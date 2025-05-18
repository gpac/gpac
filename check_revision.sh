#!/bin/sh

version="`grep '#define GPAC_VERSION ' \"./include/gpac/version.h\" | cut -d '"' -f 2`"

#check for .git - if so use nb commits till last tag for rev + commit id
if [ -d ".git" ]; then
    TAG=$(git describe --tags --abbrev=0 --match "v*"  2>>gtmp)
    VERSION=$(echo `git describe --tags --long --match "v*"  2>>gtmp || echo "UNKNOWN"` | sed "s/^$TAG-//")
    BRANCH=$(git rev-parse --abbrev-ref HEAD 2>>gtmp || echo "UNKNOWN")

    #sanitize branch name for filenames
    DHBRANCH=$(echo "$BRANCH" | sed 's/[^-+.0-9a-zA-Z~]/-/g' )

    revision="$VERSION-$DHBRANCH"

    rm gtmp

    echo "#define GPAC_GIT_REVISION	\"$revision\"" > htmp

    if ! diff htmp ./include/gpac/revision.h > /dev/null ; then
        if [ ! -f ./include/gpac/revision.h ]; then
            echo "Revision has changed"
        fi
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
