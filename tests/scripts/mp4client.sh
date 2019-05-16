
single_test "$MP4CLIENT -guid -run-for 2" "mp4client-gui"

single_test "$MP4CLIENT -guid -rmt -rmt-ogl -run-for 2 -stats $MEDIA_DIR/auxiliary_files/sky.jpg" "mp4client-gui-stats"

test_begin "mp4client-playfrom"
if [ $test_skip = 0 ] ; then

do_test "$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -new $TEMP_DIR/file.mp4" "create"

do_test "$MP4CLIENT -for-test -no-save -size 100x100 -speed 2 -rtix $TEMP_DIR/logs.txt -exit -play-from 6 $TEMP_DIR/file.mp4 -cov" "mp4client-playfrom"

fi
test_end

