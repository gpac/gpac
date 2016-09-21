
nhml_test  ()
{
 name=$(basename $1)
 name=${name%.*}

 test_begin "xmlin-$name"
 if [ $test_skip  = 1 ] ; then
  return
 fi

 mp4file=$TEMP_DIR/$name.mp4
 src="$TEMP_DIR/$name""_track1.nhml"
 dst="$TEMP_DIR/$name""_track1.mp4"

 do_test "$MP4BOX -add $1.nhml -new $mp4file" "import"
 do_hash_test $mp4file "import"

 do_test "$MP4BOX -info $mp4file" "info"

 do_test "$MP4BOX -raw 1 $mp4file -out $TEMP_DIR/test.xml " "export-track"
 do_hash_test $TEMP_DIR/test.xml "export-track"

 do_test "$MP4BOX -raws 1 $mp4file" "export-samples"

 do_test "$MP4BOX -nhml 1 $mp4file" "export-nhml"
 do_hash_test $src "export-nhml"

 do_test "$MP4BOX -add $src -new $dst" "nhml-reimport"
 do_hash_test $dst "nhml-reimport"

 test_end
}


nhml_test "$MEDIA_DIR/xmlin4/meta-metx"

nhml_test "$MEDIA_DIR/xmlin4/meta-mett"

nhml_test "$MEDIA_DIR/xmlin4/meta-mett-no-mime"

nhml_test "$MEDIA_DIR/xmlin4/meta-mett-xml"

nhml_test "$MEDIA_DIR/xmlin4/meta-mett-xml-header"

nhml_test "$MEDIA_DIR/xmlin4/subt-stpp"

nhml_test "$MEDIA_DIR/xmlin4/subt-sbtt"

nhml_test "$MEDIA_DIR/xmlin4/subt-sbtt-no-mime"

nhml_test "$MEDIA_DIR/xmlin4/text-stxt"

nhml_test "$MEDIA_DIR/xmlin4/text-stxt-no-mime"

nhml_test "$MEDIA_DIR/xmlin4/text-stxt-header"


#Testing 'metx' import when namespace is not given, shoud fail
test_begin "xmlin-meta-metx-no-namespace"
 do_test "$MP4BOX -add $MEDIA_DIR/xmlin4/meta-metx-no-namespace.nhml -new $TEMP_DIR/meta-metx-no-namespace.mp4" "import"
test_end

#Testing 'stpp' import when namespace is not provided, should fail
test_begin "xmlin-subt-stpp-no-namespace"
 do_test "$MP4BOX -add $MEDIA_DIR/xmlin4/subt-stpp-no-namespace.nhml -new $TEMP_DIR/subt-stpp-no-namespace.mp4" "import"
test_end

# Testing SWF conversion as SVG and import as 'stxt' stream
test_begin "xmlin-swf-stxt"

do_test "$MP4BOX -add $MEDIA_DIR/xmlin4/anim.swf:fmt=svg -new $TEMP_DIR/text-stxt-svg.mp4" "import"
do_hash_test $TEMP_DIR/text-stxt-svg.mp4 "import"

do_test "$MP4BOX -raw 1 $TEMP_DIR/text-stxt-svg.mp4 -out $TEMP_DIR/test.svg" "export-track"
do_hash_test $TEMP_DIR/test.svg "export-track"

do_test "$MP4BOX -raws 1 $TEMP_DIR/text-stxt-svg.mp4" "export-samples"

test_end

# Testing TTML import as 'stpp' stream
test_begin "xmlin-ttml-stpp"

do_test "$MP4BOX -add $MEDIA_DIR/xmlin4/ebu-ttd_sample.ttml -new $TEMP_DIR/subt-stpp-ttml.mp4" "import"
do_hash_test $TEMP_DIR/subt-stpp-ttml.mp4 "import"

do_test "$MP4BOX -raw 1 $TEMP_DIR/subt-stpp-ttml.mp4 -out $TEMP_DIR/test.ttml" "export-track"
do_hash_test $TEMP_DIR/test.ttml "export-track"

do_test "$MP4BOX -raws 1 $TEMP_DIR/subt-stpp-ttml.mp4" "export-samples"

test_end

