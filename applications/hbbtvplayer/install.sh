#!/bin/sh


sudo apt-get install `cat listdependencies`

svn checkout https://gpac.svn.sourceforge.net/svnroot/gpac/trunk/gpac gpac
cd gpac
./configure --use-js=no
make -j2
sudo make install
sudo make install-lib
cd ..

svn checkout http://svn.webkit.org/repository/webkit/trunk WebKit
./WebKit/Tools/Scripts/build-webkit --gtk --with-gtk=2.0 --no-webkit2 --makeargs="-j2 install"


cd hbbtvbrowserplugin
./configure
sudo make install
cd ..

cd hbbtvterminal
./configure
sudo make install
cd ..


