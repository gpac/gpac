#!/bin/sh

#rewind video while dumping to yuv

test_pipe()
{

test_begin "pipe-$1"

if [ $test_skip  = 1 ] ; then
return
fi

src_file=$2
dst_file_o=$TEMP_DIR/o-$3
dst_file_i=$TEMP_DIR/i-$3

#first do test with sending pipe as server
do_test "$GPAC -i $src_file -o pipe://gpac_pipe:mkp:ext=$4 -graph"  "pipe-osrv-out" &

sleep .1
do_test "$GPAC -i pipe://gpac_pipe:ext=$4 -o $dst_file_o -graph"  "pipe-osrv-in"

do_hash_test "$dst_file_o" "pipe-osrv-in"

if [ $5 != 0 ] ; then
  $DIFF $dst_file_o $src_file > /dev/null
  rv=$?
  if [ $rv != 0 ] ; then
  	result="source and copied files differ"
  fi
fi

#then do test with receiving pipe as server
do_test "$GPAC -i pipe://gpac_pipe:ext=$4:mkp -o $dst_file_i -graph"  "pipe-isrv-in" &

sleep .1
do_test "$GPAC -i $src_file -o pipe://gpac_pipe:ext=$4 -graph"  "pipe-isrv-out"

do_hash_test "$dst_file_i" "pipe-isrv-in"

$DIFF $dst_file_o $dst_file_i > /dev/null
rv=$?
if [ $rv != 0 ] ; then
 result="output files in client and server modes differ"
fi

test_end

}

#raw file | pipe | raw file
test_pipe "raw-raw" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.aac" "aac" 1

#raw file | pipe | mp4 file
test_pipe "raw-mp4" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.mp4" "aac" 0

#raw file -> demux -> gsf | pipe | mp4 file
test_pipe "raw-gsf-mp4" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.mp4" "gsf" 0

#raw file -> muxts | pipe | ts file
test_pipe "raw-ts" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.ts" "ts" 0

