#!/bin/sh

test_begin "vobsub"

if [ $test_skip  = 1 ] ; then
return
fi

myinspect=$TEMP_DIR/inspect.txt
do_test "$GPAC -i $EXTERNAL_MEDIA_DIR/vobsub/vobsub.idx inspect:all:deep:interleave=false:log=$myinspect -graph -stats"
do_hash_test $myinspect "inspect"

test_end
