#!/bin/sh -e

PLAYER=0
DEPENDENCIES=0
WEBKIT=0
GPAC=0  

if [ -z $1 ] ; then
	echo "\nUsage: You must choose options :"
	echo "\n\033[31m full - build the whole package (gpac+webkit+hbbtvplayer) Recommanded for computer without gpac \033[0m"
	echo "\n\033[32m player - build the HBBTV player \033[0m"
	echo "\n\033[33m webkit - download Webkit sources and install it\033[0m"
	echo "\n\033[33m gpac - download gpac sources and install it\033[0m"
	echo "\n\033[34m dependencies - get the dependencies needed to build the HBBTVPlayer \033[0m"
	exit 1
fi

for i in $* ; do
	if [ "$i" = "full" ] ; then 
		echo -e "\033[31m Usage: $0 Full Building : Activated \033[0m" 
		PLAYER=1
		DEPENDENCIES=1
		WEBKIT=1
		GPAC=1  
	fi

	if [ "$i" = "player" ] ; then 
		echo -e "\033[32m Usage: $0 Player Building : Activated \033[0m" 
		PLAYER=1
	fi

	if [ "$i" = "dependencies" ] ; then 
		echo -e "\033[34m Usage: $0 Dependecies Building : Activated \033[0m" 
		DEPENDENCIES=1
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

if [ $DEPENDENCIES -eq 1 ] ; then
	sudo apt-get install `cat listdependencies`
fi

if [ $GPAC -eq 1 ] ; then
	cd gpac
	./configure --use-js=no
	make -j2
	sudo make install
	sudo make install-lib
	cd ..
fi

if [ $WEBKIT -eq 1 ] ; then
	./WebKit/Tools/Scripts/build-webkit --gtk --with-gtk=2.0 --no-webkit2 --makeargs="-j2 install"
fi


if [ $PLAYER -eq 1 ] ; then
	cd hbbtvbrowserplugin
	./autogen.sh
	sudo make install
	cd ..

	cd hbbtvterminal
	./autogen.sh
	sudo make install
	cd ..
fi


