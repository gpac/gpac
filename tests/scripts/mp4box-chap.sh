

test_begin "mp4box-chap"
if [ "$test_skip" = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/chap-stream.mp4"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_video.cmp -add $MEDIA_DIR/auxiliary_files/subtitle.srt:chap -new $mp4file" "chap-stream"

do_hash_test $mp4file "chap-stream"

mp4file="$TEMP_DIR/chap-zoom.mp4"
ofile="$TEMP_DIR/chap-zoom.txt"

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_video.cmp:chapfile=$EXTERNAL_MEDIA_DIR/chapters/chapters.chap -new $mp4file" "chap-zoom"

do_hash_test $mp4file "chap-zoom"

do_test "$MP4BOX -dump-chap $mp4file -out $ofile" "dump-zoom"
do_hash_test $ofile "dump-zoom"


do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_video.cmp:chapfile=$EXTERNAL_MEDIA_DIR/chapters/chapters.txt -new $mp4file" "chap-file"

do_hash_test $mp4file "chap-file"

ofile="$TEMP_DIR/chap.txt"
do_test "$MP4BOX -dump-chap $mp4file -out $ofile" "dump-chap"
do_hash_test $ofile "dump-chap"

test_end


