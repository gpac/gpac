
compositor_test ()
{
 btfile=$2
 name=$(basename $2)
 name=${name%.*}

 test_begin "compositor-$1-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 do_playback_test "$3 -no-save $2" "play"

 test_end
}

BIFS_DIR="$MEDIA_DIR/bifs"
BIFS_DIR="$MEDIA_DIR/bifs"
SVG_DIR="$MEDIA_DIR/svg"

#we don't check all files for 2D or always mode checking, onnly: backgrounds2D, interactivity (one is enough), material, texturing and bitmap
test_2d_3d()
{
compositor_test $1 "$BIFS_DIR/bifs-2D-background-background2D-bind.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-background-background2D-image.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-background-background2D-layer2D.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-interactivity-nested-sensors.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-lineproperties.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-material2D.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-cap.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-compositetexture2D.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-imagetexture.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-lineargradient.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-positioning-form-align-center.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-positioning-layer2d-in-layer2d.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-positioning-layout-horiz-ltr-nowrap.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-shapes-all.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-shapes-pointset2D.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-compositetexture2D-background.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-compositetexture2D-bitmap.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-compositetexture2D-transparent.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-gradients-transparent.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-imagetexture-shapes.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-2D-viewport-complete.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-3D-positioning-layer3D.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-bitmap-image-meter-metrics.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-bitmap-image-pixel-metrics.bt" $opt
compositor_test $1 "$BIFS_DIR/bifs-misc-hc-proto-offscreengroup.bt" $opt
}

opt="-opt Compositor:OpenGLMode=disable"
test_2d_3d "nogl"

opt="-opt Compositor:OpenGLMode=always"
test_2d_3d "glonly"

#test draw mode on animations
opt="-opt Compositor:OpenGLMode=hybrid -opt Compositor:DrawMode=immediate"
compositor_test "hyb-immediate" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" $opt
opt="-opt Compositor:OpenGLMode=disable -opt Compositor:DrawMode=immediate"
compositor_test "nogl-immediate" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" $opt
opt="-opt Compositor:OpenGLMode=hybrid -opt Compositor:DrawMode=defer-debug"
compositor_test "hyb-defer-debug" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" $opt
opt="-opt Compositor:OpenGLMode=disable -opt Compositor:DrawMode=defer-debug"
compositor_test "nogl-defer-debug" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" $opt


#test opacity on SVG
opt="-opt Compositor:OpenGLMode=hybrid -opt Compositor:DrawMode=immediate"
compositor_test "svgopacity-hyb-immediate" "$SVG_DIR/opacity.svg" $opt
opt="-opt Compositor:OpenGLMode=hybrid -opt Compositor:DrawMode=defer"
compositor_test "svgopacity-hyb-defer" "$SVG_DIR/opacity.svg" $opt
opt="-opt Compositor:OpenGLMode=disable -opt Compositor:DrawMode=immediate"
compositor_test "svgopacity-nogl-immediate" "$SVG_DIR/opacity.svg" $opt
opt="-opt Compositor:OpenGLMode=disable -opt Compositor:DrawMode=defer"
compositor_test "svgopacity-nogl-defer" "$SVG_DIR/opacity.svg" $opt
opt="-opt Compositor:OpenGLMode=always"
compositor_test "svgopacity-gl" "$SVG_DIR/opacity.svg" $opt

