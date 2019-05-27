

test_begin "mp4box-tsel"
if [ $test_skip = 1 ] ; then
return
fi

mp4file="$TEMP_DIR/base.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/count_video.cmp -add $MEDIA_DIR/auxiliary_files/count_french.mp3 -add $MEDIA_DIR/auxiliary_files/count_english.mp3 -add $MEDIA_DIR/auxiliary_files/count_german.mp3 -new $mp4file" "add"

do_hash_test $mp4file "add"

do_test "$MP4BOX -group-add switchID=1:criteria=main:trackID=1 -group-add switchID=2:criteria=lang:trackID=2:trackID=3:trackID=4 $mp4file" "group-add"
do_hash_test $mp4file "group-add"

do_test "$MP4BOX -group-rem-track 2 $mp4file" "group-rem-track"
do_hash_test $mp4file "group-rem-track"

do_test "$MP4BOX -group-rem 3 $mp4file" "group-rem"
do_hash_test $mp4file "group-rem"

do_test "$MP4BOX -group-clean $mp4file" "group-clean"
do_hash_test $mp4file "group-clean"


test_end



