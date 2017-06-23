#!/bin/sh

if [ $EXTERNAL_MEDIA_AVAILABLE = 0 ] ; then
  return
fi

COUNTERFILE=$EXTERNAL_MEDIA_DIR/counter/counter_1280_720_I_25_tiled_500kb.hevc

test_begin "iff"

iff_file="$TEMP_DIR/basic.heic"
do_test "$MP4BOX -add-image $COUNTERFILE -ab heic -new $iff_file" "create-iff-basic"
do_hash_test $iff_file "create-iff-basic"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic"

iff_file="$TEMP_DIR/basic-time.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:time=1.2 -ab heic -new $iff_file" "create-iff-basic-time"
do_hash_test $iff_file "create-iff-basic-time"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic-time"

iff_file="$TEMP_DIR/basic-id.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:id=42 -ab heic -new $iff_file" "create-iff-basic-id"
do_hash_test $iff_file "create-iff-basic-id"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic-id"

iff_file="$TEMP_DIR/basic-primary.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:primary -ab heic -new $iff_file" "create-iff-basic-primary"
do_hash_test $iff_file "create-iff-basic-primary"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic-primary"

iff_file="$TEMP_DIR/basic-name.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:name=MyImage -ab heic -new $iff_file" "create-iff-basic-name"
do_hash_test $iff_file "create-iff-basic-name"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic-name"

iff_file="$TEMP_DIR/basic-hidden.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:hidden -ab heic -new $iff_file" "create-iff-basic-hidden"
do_hash_test $iff_file "create-iff-basic-hidden"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic-hidden"

iff_file="$TEMP_DIR/basic-rotation.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:rotation=90 -ab heic -new $iff_file" "create-iff-basic-rotation"
do_hash_test $iff_file "create-iff-basic-rotation"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic-rotation"

iff_file="$TEMP_DIR/basic-all.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:id=2:primary:name=Test:time=4.2:hidden:rotation=90 -ab heic -new $iff_file" "create-iff-basic-all"
do_hash_test $iff_file "create-iff-basic-all"
do_test "$MP4BOX -diso $iff_file" "diso-iff-basic-all"

iff_file="$TEMP_DIR/2images.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:time=0:primary -add-image $COUNTERFILE:time=4.2 -ab heic -new $iff_file" "create-iff-2images"
do_hash_test $iff_file "create-iff-2images"
do_test "$MP4BOX -diso $iff_file" "diso-iff-2images"

iff_file="$TEMP_DIR/2images-ref.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:id=1:time=0:primary -add-image $COUNTERFILE:time=4.2:id=2:ref=thmb,1 -ab heic -new $iff_file" "create-iff-2images-ref"
do_hash_test $iff_file "create-iff-2images-ref"
do_test "$MP4BOX -diso $iff_file" "diso-iff-2images-ref"

iff_tile_file="$TEMP_DIR/tiled.heic"
do_test "$MP4BOX -add-image $COUNTERFILE:split_tiles:primary -ab heic -new $iff_tile_file" "create-iff-tiled"
do_hash_test $iff_tile_file "create-iff-tiled"
do_test "$MP4BOX -diso $iff_tile_file" "diso-iff-tiled"

test_end
