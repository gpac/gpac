#!/bin/bash

for i in `find . -name '*.sh'` ; do svn propset svn:eol-style native $i ; done
for i in `find . -name '*.txt'` ; do svn propset svn:eol-style native $i ; done
for i in `find . -name '*.cpp'` ; do svn propset svn:eol-style native $i ; done
for i in `find . -name '*.c'` ; do svn propset svn:eol-style native $i ; done
for i in `find . -name '*.h'` ; do svn propset svn:eol-style native $i ; done
for i in `find . -name 'Makefile'` ; do svn propset svn:eol-style native $i ; done
for i in `find . -name '*.vcproj'` ; do svn propset svn:eol-style CRLF $i ; done
for i in `find . -name '*.sln'` ; do svn propset svn:eol-style CRLF $i ; done
for i in `find . -name '*.dsp'` ; do svn propset svn:eol-style CRLF $i ; done
for i in `find . -name '*.dsw'` ; do svn propset svn:eol-style CRLF $i ; done
