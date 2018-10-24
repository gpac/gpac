#!/bin/sh

test_begin "dash-dynamic-simple"

if [ $test_skip  = 0 ] ; then

$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -closest -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 4000 $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#hash the final MPD (written as  static)
do_hash_test $TEMP_DIR/file.mpd "hash-mpd"

#hash 3rd segment of video and audio, making sur the tfdt is correct
do_hash_test $TEMP_DIR/file_dash_track1_2.m4s "hash-seg2-video"
do_hash_test $TEMP_DIR/file_dash_track2_2.m4s "hash-seg2-audio"

fi

test_end


test_begin "dash-dynamic-loop"

if [ $test_skip  = 0 ] ; then

#import 2.4sec (2.4 gops) and dash using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=2.4 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=2.4 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -closest -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 4000 $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#hash the final MPD (written as  static)
do_hash_test $TEMP_DIR/file.mpd "hash-mpd"

#hash 3rd segment of video and audio
do_hash_test $TEMP_DIR/file_dash_track1_2.m4s "hash-seg2-video"
do_hash_test $TEMP_DIR/file_dash_track2_2.m4s "hash-seg2-audio"

fi

test_end



test_begin "dash-dynamic-loop-dur-less-than-seg"

if [ $test_skip  = 0 ] ; then

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 4000 $TEMP_DIR/file.mp4#video:dur=1.4 $TEMP_DIR/file.mp4#audio:dur=1.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#hash the final MPD (written as  static)
do_hash_test $TEMP_DIR/file.mpd "hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_2.m4s "hash-seg2-video"
do_hash_test $TEMP_DIR/file_dash_track2_2.m4s "hash-seg2-audio"

fi

test_end


test_begin "dash-dynamic-loop-dur-greater-than-seg"

if [ $test_skip  = 0 ] ; then

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 4000 $TEMP_DIR/file.mp4#video:dur=2.4 $TEMP_DIR/file.mp4#audio:dur=2.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#hash the final MPD (written as  static)
do_hash_test $TEMP_DIR/file.mpd "hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_2.m4s "hash-seg2-video"
do_hash_test $TEMP_DIR/file_dash_track2_2.m4s "hash-seg2-audio"

fi

test_end


test_begin "dash-dynamic-noloop-dur-less-than-seg"

if [ $test_skip  = 0 ] ; then

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 4000 -no-loop $TEMP_DIR/file.mp4#video:dur=1.4 $TEMP_DIR/file.mp4#audio:dur=1.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#hash the final MPD (written as  static)
do_hash_test $TEMP_DIR/file.mpd "hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_p2_1.m4s "hash-seg1p2-video"
do_hash_test $TEMP_DIR/file_dash_track2_p2_1.m4s "hash-seg1p2-audio"

do_hash_test $TEMP_DIR/file_dash_track1_p3_1.m4s "hash-seg1p3-video"
do_hash_test $TEMP_DIR/file_dash_track2_p3_1.m4s "hash-seg1p3-audio"

fi

test_end


test_begin "dash-dynamic-noloop-dur-greater-than-seg"

if [ $test_skip  = 0 ] ; then

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 4000 -no-loop $TEMP_DIR/file.mp4#video:dur=2.4 $TEMP_DIR/file.mp4#audio:dur=2.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#hash the final MPD (written as  static)
do_hash_test $TEMP_DIR/file.mpd "hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_2.m4s "hash-seg2p1-video"
do_hash_test $TEMP_DIR/file_dash_track2_2.m4s "hash-seg2p1-audio"

do_hash_test $TEMP_DIR/file_dash_track1_p2_1.m4s "hash-seg1p2-video"
do_hash_test $TEMP_DIR/file_dash_track2_p2_1.m4s "hash-seg1p2-audio"

fi

test_end

