
compositor_test ()
{
 btfile=$2
 name=$(basename $2)
 name=${name%.*}
 name=${name/bifs/bt}

 test_begin "compositor-$1-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 #for the time being we don't check hashes nor use same size/dur for our tests. We will redo the UI tests once finaizing filters branch
 dump_dur=5
 dump_size=192x192
 RGB_DUMP=$TEMP_DIR/$name.rgb
 do_test "$GPAC -blacklist=vtbdec,nvdec -i $2 compositor:osize=$dump_size:vfr:dur=$dump_dur:$3 @ -o $RGB_DUMP" "play"
 if [ -f $RGB_DUMP ] ; then
# do_hash_test_bin $RGB_DUMP "play"
  do_play_test "play" "$RGB_DUMP:size=$dump_size"
 else
  result="no output"
 fi

 test_end
}

BIFS_DIR="$MEDIA_DIR/bifs"
SVG_DIR="$MEDIA_DIR/svg"

#we don't check all files for 2D or always mode checking, onnly: backgrounds2D, interactivity (one is enough), material, texturing and bitmap
test_2d_3d()
{
compositor_test $1 "$BIFS_DIR/bifs-2D-background-background2D-bind.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-background-background2D-image.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-background-background2D-layer2D.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-interactivity-nested-sensors.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-lineproperties.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-material2D.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-cap.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-compositetexture2D.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-imagetexture.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-painting-xlineproperties-lineargradient.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-positioning-form-align-center.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-positioning-layer2d-in-layer2d.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-positioning-layout-horiz-ltr-nowrap.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-shapes-all.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-shapes-pointset2D.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-compositetexture2D-background.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-compositetexture2D-bitmap.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-compositetexture2D-transparent.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-gradients-transparent.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-texturing-imagetexture-shapes.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-2D-viewport-complete.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-3D-positioning-layer3D.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-bitmap-image-meter-metrics.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-bitmap-image-pixel-metrics.bt" "$opt"
compositor_test $1 "$BIFS_DIR/bifs-misc-hc-proto-offscreengroup.bt" "$opt"
}


opt="ogl=off"
test_2d_3d "nogl"

opt="ogl=on:drv=yes"
test_2d_3d "glonly"

#the other mode (auto/hybrid) is tested in bt.sh


#test draw mode on animations
compositor_test "hyb-immediate" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" "ogl=hybrid:mode2d=immediate"
compositor_test "nogl-immediate" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" "ogl=off:mode2d=immediate"
compositor_test "hyb-defer-debug" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" "ogl=hybrid:mode2d=debug"
compositor_test "nogl-defer-debug" "$BIFS_DIR/bifs-interpolation-positioninterpolator2D-position.bt" "ogl=off:mode2d=debug"

#test group opacity on SVG
compositor_test "svgopacity-hyb-immediate" "$SVG_DIR/opacity.svg" "ogl=hybrid:mode2d=immediate"
compositor_test "svgopacity-hyb-defer" "$SVG_DIR/opacity.svg" "ogl=hybrid:mode2d=defer"
compositor_test "svgopacity-nogl-immediate" "$SVG_DIR/opacity.svg" "ogl=off:mode2d=immediate"
compositor_test "svgopacity-nogl-defer" "$SVG_DIR/opacity.svg" "ogl=off:mode2d=defer"
compositor_test "svgopacity-gl" "$SVG_DIR/opacity.svg" "ogl=on"

compositor_test "gl-noshader" "$BIFS_DIR/bifs-3D-lighting-fog.bt" "ogl=on:vertshader=NULL"
compositor_test "gl-noshader-tx" "$BIFS_DIR/bifs-2D-background-background2D-image.bt" "ogl=on:drv=yes:vertshader=NULL"


compositor_test "gl-stereo" "$BIFS_DIR/bifs-3D-lighting-fog.bt" "ogl=on:stereo=spv5:bvol=box"

compositor_test "gl-norms" "$BIFS_DIR/bifs-3D-lighting-fog.bt" "ogl=on:bvol=aabb:norms=face"

compositor_test "gl-strike" "$BIFS_DIR/bifs-2D-painting-material2D.bt" "ogl=on:linegl:bvol=box"

compositor_test "gl-text" "$BIFS_DIR/bifs-text-length.bt" "ogl=on:drv=yes:textxt=never"

compositor_test "nogl-text-texture" "$BIFS_DIR/bifs-text-length.bt" "textxt=always"

compositor_test "svg-gl" "$SVG_DIR/shapes-rect-01-t.svg" "ogl=on:drv=yes"

single_test "$GPAC -i $BIFS_DIR/bifs-2D-background-background2D-image.bt compositor:ogl=on:drv=yes @ vout" "compositor-gltexout"
