#!/bin/sh

test_sock_tcp()
{

test_begin "sock-tcp-$1"

if [ $test_skip  = 1 ] ; then
return
fi

src_file=$2
dst_file_o=$TEMP_DIR/o-$3
dst_file_i=$TEMP_DIR/i-$3

#first do test with sending as server
do_test "$GPAC -i $src_file -o tcp://localhost:1234:ext=$4:listen -graph"  "osrv-out" &

sleep .1
do_test "$GPAC -i tcp://localhost:1234:ext=$4 -o $dst_file_o -graph"  "osrv-in"

do_hash_test "$dst_file_o" "osrv-in"

if [ $5 != 0 ] ; then
  $DIFF $dst_file_o $src_file > /dev/null
  rv=$?
  if [ $rv != 0 ] ; then
  	result="source and copied files differ"
  fi
fi

#then do test with receiver as server
do_test "$GPAC -i tcp://localhost:1234:ext=$4:listen -o $dst_file_i -graph"  "isrv-in" &

sleep .1
do_test "$GPAC -i $src_file -o tcp://localhost:1234:ext=$4 -graph"  "isrv-out"

do_hash_test "$dst_file_i" "isrv-in"

$DIFF $dst_file_o $dst_file_i > /dev/null
rv=$?
if [ $rv != 0 ] ; then
 result="output files in client and server modes differ"
fi

test_end

}

test_sock_udp()
{

test_begin "sock-udp-$1"

if [ $test_skip  = 1 ] ; then
return
fi

src_file=$2
dst_file=$TEMP_DIR/$3

#start udp reciever first, with 2s timeout - if we test out of order / drop, skip receiver
if [ -z $6 ] ; then
do_test "$GPAC -i udp://localhost:1234:ext=$4:timeout=1000 -o $dst_file -graph"  "in"  &

sleep .1
fi

do_test "$GPAC -i $src_file -o udp://localhost:1234:ext=$4:rate=1m$6 -graph"  "out"

wait

#if we test out of order / drop, skip hash
if [ -z $6 ] ; then
do_hash_test "$dst_file" "in"
fi

if [ $5 != 0 ] ; then
  $DIFF $src_file $dst_file > /dev/null
  rv=$?
  if [ $rv != 0 ] ; then
  	result="source and copied files differ"
  fi
fi

test_end

}



#raw file | tcp | raw file
test_sock_tcp "raw-raw" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.aac" "aac" 1

#raw file | pipe | mp4 file
test_sock_tcp "raw-mp4" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.mp4" "aac" 0

#raw file -> demux -> gsf | pipe | mp4 file
test_sock_tcp "raw-gsf-mp4" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.mp4" "gsf" 0


#raw file | udp | raw file
test_sock_udp "raw-raw" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.aac" "aac" 1 ""

#raw file | udp | mp4 file
test_sock_udp "raw-mp4" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.mp4" "aac" 0 ""

#raw file -> demux -> gsf | pipe | mp4 file
test_sock_udp "raw-gsf-mp4" $MEDIA_DIR/auxiliary_files/enst_audio.aac "test.mp4" "gsf" 0 ":pckr=0/10:pckd=0/100"
