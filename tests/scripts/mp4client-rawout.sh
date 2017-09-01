#test all the rendering color mode and texturing tools of the soft rasterizer using raw output module

run_rawout_tests()
{
 opts="-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=$1 -no-save"
 single_playback_test "$opts -run-for 1" "mp4client-rawout-$1-gui"
 single_playback_test "$opts $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-$1-jpg"
 single_playback_test "$opts $MEDIA_DIR/auxiliary_files/count_video.mp4" "mp4client-rawout-$1-yuv"
 single_playback_test "$opts $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-$1-back"
 single_playback_test "$opts $MEDIA_DIR/bifs/bifs-2D-texturing-gradients-transparent.bt" "mp4client-rawout-$1-gradalpha"
 single_playback_test "$opts $MEDIA_DIR/bifs/bifs-2D-texturing-imagetexture-shapes.bt" "mp4client-rawout-$1-texture"
 single_playback_test "$opts $MEDIA_DIR/bifs/bifs-2D-painting-material2D.bt" "mp4client-rawout-$1-alpha"
 single_playback_test "$opts $MEDIA_DIR/bifs/bifs-2D-painting-colortransform-alpha.bt" "mp4client-rawout-$1-cmatalpha"
 single_playback_test "$opts $MEDIA_DIR/bifs/bifs-2D-painting-colortransform-texture.bt" "mp4client-rawout-$1-cmattexture"

 single_playback_test "$opts $testfile" "mp4client-rawout-$1-textyuv"


}

testfile=$TEMP_DIR/text_yuv.mp4
$MP4BOX -mp4 "$MEDIA_DIR/bifs/bifs-2D-texturing-movietexture-shapes.bt" -out $testfile 2> /dev/null

#RGB555 tests are commented since we don't build GPAC with RGB555 support
#run_rawout_tests "555"
run_rawout_tests "565"
run_rawout_tests "rgb"
run_rawout_tests "bgr"
run_rawout_tests "rgb32"
run_rawout_tests "bgr32"
run_rawout_tests "rgba"
run_rawout_tests "argb"


rm $testfile 2> /dev/null

