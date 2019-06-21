#!/bin/sh

#rewind video while dumping to yuv

pcmfile="$TEMP_DIR/file.pcm"

myinspect="inspect:test=encode:fmt=@pn@-@dts@-@cts@@lf@"

test_resample()
{
 name=$(basename $1)
 name=${name%.*}
test_begin "resample-$name"

if [ $test_skip  = 1 ] ; then
return
fi

dumpfile=$TEMP_DIR/dump.pcm
do_test "$GPAC -blacklist=ffdec -i $1:index=0 resample$2 @ -o $dumpfile:sstart=1:send=250"  "resample"

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $dumpfile$3 inspect:deep:allp:fmt=%pn%-%cts%-%size%%lf%:log=$myinspect"  "inspect"
do_hash_test "$myinspect" "inspect"

test_end

}

test_resample "$EXTERNAL_MEDIA_DIR/import//aac_vbr_51_128k.aac" ":ch=2" ":ch=2:sr=22050"
test_resample "$MEDIA_DIR/auxiliary_files/enst_audio.aac" ":sr=22050" ":ch=2:sr=22050"


