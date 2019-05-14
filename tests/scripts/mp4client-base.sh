
single_test "$MP4CLIENT -guid -run-for 2" "mp4client-gui"

single_test "$MP4CLIENT -guid -run-for 2 -stats $MEDIA_DIR/auxiliary_files/sky.jpg" "mp4client-gui-stats"

single_test "$MP4CLIENT -no-save -size 100x100 -speed 2 -rtix $TEMP_DIR/logs.txt -exit -play-from 6 $MEDIA_DIR/auxiliary_files/enst_video.h264 -cov" "mp4client-playfrom"

