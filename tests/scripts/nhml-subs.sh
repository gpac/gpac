#!/bin/sh

test_begin "NHMLSubs"

nhml_ttml_file="$TEMP_DIR/nhml_ttml.mp4"
do_test "$MP4BOX -add $MEDIA_DIR/ttml/ttml.nhml -new $nhml_ttml_file" "create-nhml_ttml"
do_hash_test $nhml_ttml_file "create-nhml_ttml"

test_end
