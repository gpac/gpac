#!/bin/sh

test_begin "iff"

iff_file="$TEMP_DIR/test.heic"
do_test "$MP4BOX -add-image $MEDIA_DIR/auxiliary_files/counter.hvc:id=1 -set-primary 1 -ab heic -new $iff_file" "create-iff"
do_hash_test $iff_file "create-iff"
do_test "$MP4BOX -diso $iff_file" "diso-iff"

iff_tile_file="$TEMP_DIR/test-tile.heic"
do_test "$MP4BOX -add-image $MEDIA_DIR/auxiliary_files/counter.hvc:tilemode=base:id=1 -set-primary 1 -ab heic -new $iff_tile_file" "create-iff-tiled"
do_hash_test $iff_tile_file "create-iff-tiled"
do_test "$MP4BOX -diso $iff_tile_file" "diso-iff-tiled"

test_end
