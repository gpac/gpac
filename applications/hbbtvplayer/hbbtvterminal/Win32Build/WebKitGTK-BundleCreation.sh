 #! /bin/bash 
if [ -z "$1" ]
then
  echo "Usage: $0 PATH To Visual Studio 8 Folder, put it as '/c/Program Files/Microsoft Visual Studio X'/ "
  exit 1
fi

export PATH="$1/VC/bin:$1/Common7/Tools:$1/Common7/IDE:$PATH"

if [ -d "WebKitGTK-Bundle" ]; then
	rm -rf WebKitGTK-Bundle/*
else
	mkdir WebKitGTK-Bundle
fi 

if [ ! -d "RPM" ]; then
	mkdir RPM
fi

# Moving the RPF file into the RPM folder
for i in $(ls *.rpm)
do
echo " Copy $i"
mv "$i" "RPM/"
done

# Create the WebkitGTK-Bundle
echo " Creation of the WebKitBundle"
for i in $(ls -d */ | grep -E -v 'WebKitGTK-Bundle|RPM')
do
echo " Copy $i"
cp -rf $i/* "WebKitGTK-Bundle/"
done

echo " Creation of the .lib files"
cd WebKitGTK-Bundle/bin
# Create the file exports.sed
touch exports.sed
echo "/[ \t]*ordinal hint/,/^[ \t]*Summary/{" >> exports.sed
echo " /^[ \t]\+[0-9]\+/{" >> exports.sed
echo "  s/^[ \t]\+[0-9]\+[ \t]\+[0-9A-Fa-f]\+[ \t]\+[0-9A-Fa-f]\+[ \t]\+\(.*\)/\1/p" >> exports.sed
echo " }" >> exports.sed
echo "}" >> exports.sed

#Starting Windows Environnement
cmd /c vsvars32.bat

# DLL syntax correction
for i in $(ls *.dll | sed 's/\.[^\.]*$//')
do
echo "Unifing the syntax of $i"
pexports  "$i.dll" | sed -f exports.sed > "$i.def"
#sed -i "s/"$i.dll"/" "/g" "$i.def"
lib /def:"$i.def" /out:"$i.lib" /machine:X86
done

echo "Moving the .lib files to the Bundle/lib repository"
for i in $(ls *.lib)
do 
mv "$i" "../lib/"
done





