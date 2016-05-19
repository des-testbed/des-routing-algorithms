#!/bin/bash
# FILE: version.sh
version="`sed  's/^ *//' major_version`"
old="`sed  's/^ *//' build` +1"
echo $old | bc > build.temp
mv build.temp build
#versión..
echo "$version`sed  's/^ *//' build` - `date`" > version
#header
echo "#ifndef BUILD_NUMBER_STR" > build.h
echo "#define BUILD_NUMBER_STR \"`sed  's/^ *//' build`\"" >> build.h
echo "#endif" >> build.h

echo "#ifndef VERSION_DATE" >> build.h
echo "#define VERSION_DATE \"`date`\"" >> build.h
echo "#endif" >> build.h

echo "#ifndef VERSION_STR" >> build.h
echo "#define VERSION_STR \"$version`sed  's/^ *//' build`\"" >> build.h
echo "#endif" >> build.h

echo "#ifndef VERSION_NAME" >> build.h
echo "#define VERSION_NAME \"$1\"" >> build.h
echo "#endif" >> build.h
