
single_test "$MP4CLIENT -guid -run-for 2" "mp4client-gui"

single_test "$MP4CLIENT -guid -run-for 2 -stats $MEDIA_DIR/auxiliary_files/sky.jpg" "mp4client-gui-stats"
