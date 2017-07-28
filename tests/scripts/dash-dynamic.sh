#!/bin/sh

test_begin "dash-dynamic-simple"

$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 8000 $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait


#this is dynamic, use the stored MPD to avoid the varying availablilityStartTime
do_hash_test $TEMP_DIR/file-Period_DID1.mpd "hash-mpd"

#hash 3rd segment of video and audio, making sur the tfdt is correct
do_hash_test $TEMP_DIR/file_dash_track1_3.m4s "hash-seg3-video"
do_hash_test $TEMP_DIR/file_dash_track2_3.m4s "hash-seg3-audio"

echo "TFDT for track 1 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 1 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS

test_end


test_begin "dash-dynamic-loop"
#import 2.4sec (2.4 gops) and dash using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=2.4 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=2.4 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -mpd-refresh 10 -time-shift -1 -run-for 8000 $TEMP_DIR/file.mp4#video $TEMP_DIR/file.mp4#audio -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#this is dynamic, use the stored MPD to avoid the varying availablilityStartTime
do_hash_test $TEMP_DIR/file-Period_DID1.mpd "hash-mpd"

#hash 3rd segment of video and audio, making sur the tfdt is correct
do_hash_test $TEMP_DIR/file_dash_track1_3.m4s "hash-seg3-video"
do_hash_test $TEMP_DIR/file_dash_track2_3.m4s "hash-seg3-audio"


echo "TFDT for track 1 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 1 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS


test_end



test_begin "dash-dynamic-loop-dur-less-than-seg"

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -bs-switching single -mpd-refresh 10 -time-shift -1 -run-for 8000 $TEMP_DIR/file.mp4#video:dur=1.4 $TEMP_DIR/file.mp4#audio:dur=1.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#this is dynamic, use the stored MPD to avoid the varying availablilityStartTime
do_hash_test $TEMP_DIR/file-Period_DID1.mpd "hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_3.m4s "hash-seg3-video"
do_hash_test $TEMP_DIR/file_dash_track2_3.m4s "hash-seg3-audio"

echo "TFDT for track 1 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 1 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS


test_end

test_begin "dash-dynamic-loop-dur-greater-than-seg"

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -bs-switching single -mpd-refresh 10 -time-shift -1 -run-for 8000 $TEMP_DIR/file.mp4#video:dur=2.4 $TEMP_DIR/file.mp4#audio:dur=2.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#this is dynamic, use the stored MPD to avoid the varying availablilityStartTime
do_hash_test $TEMP_DIR/file-Period_DID1.mpd "hash-mpd"

do_hash_test $TEMP_DIR/file_dash_track1_3.m4s "hash-seg3-video"
do_hash_test $TEMP_DIR/file_dash_track2_3.m4s "hash-seg3-audio"


echo "TFDT for track 1 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 2" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_2.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 1 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track1_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS
echo "TFDT for track 2 segment 3" >> $LOGS
$MP4BOX -diso -std $TEMP_DIR/file_dash_track2_3.m4s 2> /dev/null | grep "tfdt" >> $LOGS


test_end


test_begin "dash-dynamic-noloop-dur-less-than-seg"

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -bs-switching single -mpd-refresh 10 -time-shift -1 -run-for 8000 -no-loop $TEMP_DIR/file.mp4#video:dur=1.4 $TEMP_DIR/file.mp4#audio:dur=1.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#this is dynamic, use the stored MPD to avoid the varying availablilityStartTime
do_hash_test $TEMP_DIR/file-Period_DID1.mpd "hash-mpd1"
do_hash_test $TEMP_DIR/file-Period_DID2.mpd "hash-mpd2"

do_hash_test $TEMP_DIR/file_dash_track1__p3_1.m4s "hash-seg3-video"
do_hash_test $TEMP_DIR/file_dash_track2__p3_1.m4s "hash-seg3-audio"


test_end

test_begin "dash-dynamic-noloop-dur-greater-than-seg"

#import 10 sec (10 gops) and dash for a shorter media duration using loop mode
$MP4BOX -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_I25_baseline_1280x720_512kbps.264:dur=10 -add $EXTERNAL_MEDIA_DIR/counter/counter_30s_audio.aac:dur=10 -new $TEMP_DIR/file.mp4 2> /dev/null

do_test "$MP4BOX -dash-live 2000 -subdur 2000 -profile live -bs-switching single -mpd-refresh 10 -time-shift -1 -run-for 8000 -no-loop $TEMP_DIR/file.mp4#video:dur=4.4 $TEMP_DIR/file.mp4#audio:dur=2.4 -out $TEMP_DIR/file.mpd" "dash-gen" &

sleep 1

dump_dur=2
do_playback_test "$TEMP_DIR/file.mpd" "dash-play"

#wait for the end of the MP4Box
wait

#this is dynamic, use the stored MPD to avoid the varying availablilityStartTime
do_hash_test $TEMP_DIR/file-Period_DID1.mpd "hash-mpd1"
do_hash_test $TEMP_DIR/file-Period_DID2.mpd "hash-mpd2"

do_hash_test $TEMP_DIR/file_dash_track1_2.m4s "hash-seg2-video"

do_hash_test $TEMP_DIR/file_dash_track2_2.m4s "hash-seg2-audio"


do_hash_test $TEMP_DIR/file_dash_track1__p2_1.m4s "hash-seg3-video"

do_hash_test $TEMP_DIR/file_dash_track2__p2_1.m4s "hash-seg3-audio"

test_end

