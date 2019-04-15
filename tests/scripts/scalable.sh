#!/bin/sh

scalable_test()
{
testname=$(basename "$1" | cut -d '.' -f1)


test_begin $testname
 if [ $test_skip  = 1 ] ; then
  return
 fi

mp4file="$TEMP_DIR/$testname.mp4"
splitfile="$TEMP_DIR/$testname-split.mp4"
mergefile="$TEMP_DIR/$testname-merge.mp4"
splitdump="$TEMP_DIR/$testname-split.yuv"

$MP4BOX -add $1 -new $mp4file 2> /dev/null

do_test "$MP4BOX -add "$1:svcmode=split" -new $splitfile" "Split"
do_hash_test $splitfile "split"

do_test "$MP4BOX -add "$splitfile:svcmode=merge" -new $mergefile" "Merge"
do_hash_test $mergefile "merge"

#compare hashes of source and merge
do_compare_file_hashes $mp4file $mergefile
rv=$?

if [ $rv != 0 ] ; then
result="Hash is not the same between source content and merge content"
fi


#decode and dump 2 frames
case $1 in
*sxc*)
 do_test "$GPAC -blacklist=nvdec,vtbdec,ffdec,ohevcdec -i $splitfile -o $splitdump:sstart=8:send=9" "decode"
 #commented for now, issue in the decoder
 #do_hash_test "$splitdump" "decode"
 do_play_test "split" "$splitdump:size=704x576";;
*shvc*)
 do_test "$GPAC -blacklist=nvdec,vtbdec,ffdec -i $splitfile -o $splitdump:sstart=8:send=9" "decode"
 do_hash_test "$splitdump" "decode"
 do_play_test "split" "$splitdump:size=3840x1600";;
esac


do_test "$MP4BOX -dash 1000 $splitfile -out $TEMP_DIR/$testname.mpd" "dash"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $TEMP_DIR/$testname.mpd inspect:all:deep:interleave=false:log=$myinspect"
do_hash_test $myinspect "inspect"


test_end

}

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
 return
fi

for i in `ls $EXTERNAL_MEDIA_DIR/scalable/* | grep -v "html"` ; do

scalable_test $i

done

