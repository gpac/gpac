#!/bin/sh

test_id3()
{

 name=$(basename $1)
 name=${name%.*}
test_begin "id3-$name"

if [ $test_skip = 1 ] ; then
 return
fi

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $1 inspect:log=$myinspect -graph -stats"
do_hash_test $myinspect "inspect"

test_end

}

#test all bifs
for i in $EXTERNAL_MEDIA_DIR/id3/*.mp3 ; do
test_id3 $i
done




