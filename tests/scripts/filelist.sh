#!/bin/sh

test_flist()
{

test_begin "filelist-$1"

if [ $test_skip  = 1 ] ; then
return
fi

dump=$TEMP_DIR/dump.rgb
myinspect=$TEMP_DIR/inspect.txt

do_test "$GPAC $2 inspect:all:deep:interleave=false:log=$myinspect -graph -stats -logs=app@debug" "inspect"
do_hash_test $myinspect "inspect"

if [ $3 = 1 ] ; then
do_test "$GPAC -no-reassign=0 $2 ffsws:osize=192x192:ofmt=rgb @ -o $dump -blacklist=nvdec,vtbdec" "dump"
do_hash_test $dump "dump"

do_play_test "dump" "$dump:size=192x192"

fi

test_end

}

test_flist "codecs" "flist:dur=1/1:in=$MEDIA_DIR/auxiliary_files/logo.jpg,$MEDIA_DIR/auxiliary_files/logo.png" 0

plist=$TEMP_DIR/plist.m3u

echo "" > $plist
#check decoder swapping (flist->png->ffsws to flist->m4v->ffsws)
echo "$MEDIA_DIR/auxiliary_files/logo.png" >> $plist
echo "$MEDIA_DIR/auxiliary_files/count_video.cmp" >> $plist
#check decoder removal (flist->m4v->ffsws to flist->ffsws)
echo "$EXTERNAL_MEDIA_DIR/raw/raw.rgb:size=128x128" >> $plist
#check decoder insertion (flist->ffsws to flist->jpg->ffsws), with repeat of frame
echo "$MEDIA_DIR/auxiliary_files/sky.jpg" >> $plist

test_flist "filter-swap" "-i $plist" 1

plist=$TEMP_DIR/plist-params.m3u
echo "" > $plist
#check decoder swapping (flist->png->ffsws to flist->m4v->ffsws)
echo "#repeat=24" >> $plist
echo "$MEDIA_DIR/auxiliary_files/logo.jpg" >> $plist

test_flist "params" "-i $plist:dur=1/1" 0

test_flist "enum" "flist:in=$MEDIA_DIR/auxiliary_files/\*.jpg" 0
