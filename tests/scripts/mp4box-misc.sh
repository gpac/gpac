


test_begin "mp4box-misc"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/test.mp4"
mp4file2="$TEMP_DIR/test2.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264+$MEDIA_DIR/auxiliary_files/enst_audio.aac -isma -new $mp4file" "add-plus"
do_hash_test $mp4file "add-plus"


do_test "$MP4BOX -diod $mp4file" "diod"
do_hash_test $mp4file "diod"
do_test "$MP4BOX -nosys $mp4file" "nosys"
do_hash_test $mp4file "nosys"
do_test "$MP4BOX -timescale 1000 $mp4file" "settimescale"
do_hash_test $mp4file "settimescale"
do_test "$MP4BOX -delay 1=1000 $mp4file" "delay"
do_hash_test $mp4file "delay"
do_test "$MP4BOX -swap-track-id 101:201 $mp4file" "swaptrackid"
do_hash_test $mp4file "swaptrackid"
do_test "$MP4BOX -set-track-id 101:10 $mp4file" "settrackid"
do_hash_test $mp4file "settrackid"
do_test "$MP4BOX -name 10=test $mp4file" "sethandler"
do_hash_test $mp4file "sethandler"
do_test "$MP4BOX -disable 10 $mp4file" "disable-track"
do_hash_test $mp4file "disable-track"
do_test "$MP4BOX -enable 10 $mp4file" "enable-track"
do_hash_test $mp4file "enable-track"
do_test "$MP4BOX -ref 10:GPAC:1 $mp4file" "set-ref"
do_hash_test $mp4file "set-ref"
do_test "$MP4BOX -rap 10 $mp4file -out $mp4file2" "raponly"
do_hash_test $mp4file2 "raponly"
do_test "$MP4BOX -refonly 10 $mp4file -out $mp4file2" "refonly"
do_hash_test $mp4file2 "refonly"


test_end

