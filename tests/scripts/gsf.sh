#!/bin/sh

#rewind video while dumping to yuv

test_gsf()
{

test_begin "gsf-$1"

if [ $test_skip  = 1 ] ; then
return
fi

dst_file=$TEMP_DIR/dump.gsf

myinspect=$TEMP_DIR/inspect.txt

do_test "$GPAC $2 reframer @ -o $dst_file$3$4 -graph -logs=container@debug"  "gsf-mux"

#do not hash the mux for AV: gsf mux does not guarantee the order of the stream declaration, nor the packets / stream order. We however hash the demux result
if [ "$1" != "av" ] ; then
do_hash_test "$dst_file" "gsf-mux"
fi

do_test "$GPAC -i $dst_file$4 inspect:allp:deep:interleave=false:log=$myinspect -graph" "gsf-demux"
do_hash_test $myinspect "gsf-demux"

test_end

}

#raw avc file to gsf
test_gsf "simple" "-i $MEDIA_DIR/auxiliary_files/enst_video.h264" "" " -logs=container@debug"

#raw avc and aac files to gsf
test_gsf "av" "-i $MEDIA_DIR/auxiliary_files/enst_video.h264 -i $MEDIA_DIR/auxiliary_files/enst_audio.aac" "" ""


#raw avc file to gsf with all options
test_gsf "full" "-i $MEDIA_DIR/auxiliary_files/enst_video.h264" ":sigsn:sigbo:sigdur=no:sigdts=no:minp:mpck=100" ":magic=SomeKindOfMagic"

#raw avc file to gsf with encryption, skip prop and max packet size
test_gsf "crypted" "-i $MEDIA_DIR/auxiliary_files/enst_video.h264" ":IV=0x279926496a7f5d25da69f2b3b2799a7f" ":key=0x279926496a7f5d25da69f2b3b2799a7f"

#raw avc file to gsf with pattern encryption
test_gsf "crypted-pattern" "-i $MEDIA_DIR/auxiliary_files/enst_video.h264" ":IV=0x279926496a7f5d25da69f2b3b2799a7f:pattern=1/9" ":key=0x279926496a7f5d25da69f2b3b2799a7f"

#source test filter with all props
test_gsf "props-check" "-ltf UTSource:gsftest" ":crate=2.0:skp=FPS:sigbo" ""
