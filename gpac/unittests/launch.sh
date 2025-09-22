#!/bin/sh

#windows
export PATH=$PATH:unittests/build/bin/gcc

#linux
export LD_LIBRARY_PATH=unittests/build/bin/gcc${LD_LIBRARY_PATH:+:}${LD_LIBRARY_PATH:-}

#macos
export DYLD_LIBRARY_PATH=unittests/build/bin/gcc${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH:-}

unittests/build/bin/gcc/unittests
