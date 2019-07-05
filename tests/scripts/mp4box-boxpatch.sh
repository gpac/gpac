

test_begin "mp4box-patch_box"
if [ $test_skip = 1 ] ; then
return
fi

mp4file1="$TEMP_DIR/add.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -patch $MEDIA_DIR/boxpatch/box_add.xml -new $mp4file1" "add-box"
do_hash_test $mp4file1 "add-box"

mp4file2="$TEMP_DIR/rem.mp4"
do_test "$MP4BOX -patch $MEDIA_DIR/boxpatch/box_rem.xml $mp4file1 -out $mp4file2" "rem-box"
do_hash_test $mp4file2 "rem-box"

mp4file1="$TEMP_DIR/add-root.mp4"
do_test "$MP4BOX -patch $MEDIA_DIR/boxpatch/box_add_root.xml $mp4file2 -out $mp4file1" "add-box-root"
do_hash_test $mp4file1 "add-box-root"

mp4file2="$TEMP_DIR/rem-root.mp4"
do_test "$MP4BOX -patch $MEDIA_DIR/boxpatch/box_rem_root.xml $mp4file1 -out $mp4file2" "rem-box-root"
do_hash_test $mp4file2 "rem-box-root"

mp4file1="$TEMP_DIR/add-root-flat.mp4"
do_test "$MP4BOX -flat -patch $MEDIA_DIR/boxpatch/box_add_root.xml $mp4file2 -out $mp4file1" "add-box-root-flat"
do_hash_test $mp4file1 "add-box-root-flat"

mp4file1="$TEMP_DIR/add-root-streamable.mp4"
do_test "$MP4BOX -inter 0 -patch $MEDIA_DIR/boxpatch/box_add_root.xml $mp4file2 -out $mp4file1" "add-box-root-streamable"
do_hash_test $mp4file1 "add-box-root-streamable"


test_end



