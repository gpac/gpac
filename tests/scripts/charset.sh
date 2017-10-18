

charset_test() {

	infile=$1
	fname=$(basename $infile)

	test_begin "charset-$fname"

	do_test "$MP4BOX -add $infile -new $TEMP_DIR/$fname" "openwrite"
	do_hash_test "$TEMP_DIR/$fname" "openwrite"

	do_test "$MP4BOX -brand TEST $TEMP_DIR/$fname" "edit"
	do_hash_test "$TEMP_DIR/$fname" "edit"

	do_playback_test "$TEMP_DIR/$fname" "play"

	test_end

}



charset_tests() {

	for f in $EXTERNAL_MEDIA_DIR/utf8-names/*.mp4 ; do
		charset_test $f
	done
}

charset_tests

