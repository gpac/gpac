#!/bin/sh -e

WEBKIT=0
GPAC=0  

if [ -z $1 ] ; then
	echo "\nDO NOT USE sudo FOR THIS SCRIPT"
	echo "\nUsage: You must choose options :"
	echo "\n\033[31m full - build the whole package (gpac+webkit+hbbtvplayer) Recommended for computer without gpac \033[0m"
	echo "\n\033[33m webkit - download Webkit sources and install it\033[0m"
	echo "\n\033[33m gpac - download gpac sources and install it\033[0m"
	exit 1
fi

for i in $* ; do
	if [ "$i" = "full" ] ; then 
		echo -e "\033[31m Usage: $0 Full Building : Activated \033[0m" 
		WEBKIT=1
		GPAC=1  
	fi

	if [ "$i" = "webkit" ] ; then 
		echo -e "\033[33m Usage: $0 Webkit Building : Activated \033[0m" 
		WEBKIT=1 
	fi

	if [ "$i" = "gpac" ] ; then 
		echo -e "\033[33m Usage: $0 gpac Building : Activated \033[0m" 
		WEBKIT=1 
	fi
done

if [ $GPAC -eq 1 ] ; then
	svn checkout https://gpac.svn.sourceforge.net/svnroot/gpac/trunk/gpac gpac
fi

if [ $WEBKIT -eq 1 ] ; then
	svn checkout http://svn.webkit.org/repository/webkit/trunk WebKit --depth files -r 97300 # 98458 ?
	svn checkout http://svn.webkit.org/repository/webkit/trunk/Source WebKit/Source  -r 97300
	svn checkout http://svn.webkit.org/repository/webkit/trunk/Tools WebKit/Tools  -r 97300
	svn checkout http://svn.webkit.org/repository/webkit/trunk/WebKitLibraries WebKit/WebKitLibraries  -r 97300
fi


