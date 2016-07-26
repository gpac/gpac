#!/bin/sh
test_begin "set-meta"

do_test "$MP4BOX -set-meta Metadata:id=0 $EXTERNAL_MEDIA_DIR/item/counter_noItems1.mp4" "mp4box-set-meta" do_hash_test $TEMP_DIR/test.mp4 "mp4box-set-meta-id0"
do_test "$MP4BOX -set-meta Metadata:id=1 $EXTERNAL_MEDIA_DIR/item/counter_noItems2.mp4" "mp4box-set-meta" do_hash_test $TEMP_DIR/test.mp4 "mp4box-set-meta-id1"
do_test "$MP4BOX -set-meta Metadata $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4"       "mp4box-set-meta" do_hash_test $TEMP_DIR/test.mp4 "mp4box-set-meta"

test_end


test_begin "add-items"

do_test "$MP4BOX -add-item $EXTERNAL_MEDIA_DIR/item/file.html:mime=html:id=1 -add-item $EXTERNAL_MEDIA_DIR/item/file.css:mime=css:id=2 -add-item $EXTERNAL_MEDIA_DIR/item/file.js:mime=javascript:id=3 -add-item $EXTERNAL_MEDIA_DIR/item/file.svg:mime=svg:id=4 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-add-items"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-add-items"

test_end


test_begin "set-primary"

do_test "$MP4BOX -set-primary id=2 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-set-primary"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-set-primary"

test_end


test_begin "dump-items"

do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 1:path=$TEMP_DIR/file1.html" "mp4box-dump-items-1" do_hash_test $TEMP_DIR/test.mp4 "mp4box-dump-items-1"
do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 2:path=$TEMP_DIR/file1.css" "mp4box-dump-items-2" do_hash_test $TEMP_DIR/test.mp4 "mp4box-dump-items-2"
do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 3:path=$TEMP_DIR/file1.js" "mp4box-dump-items-3" do_hash_test $TEMP_DIR/test.mp4 "mp4box-dump-items-3"
do_test "$MP4BOX $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4 -dump-item 4:path=$TEMP_DIR/file1.svg" "mp4box-dump-items-4" do_hash_test $TEMP_DIR/test.mp4 "mp4box-dump-items-4"

test_end


test_begin "remove-items"

do_test "$MP4BOX -rem-item 1 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-remove-items-1" do_hash_test $TEMP_DIR/test.mp4 "mp4box-remove-items-1"
do_test "$MP4BOX -rem-item 2 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-remove-items-2" do_hash_test $TEMP_DIR/test.mp4 "mp4box-remove-items-2"
do_test "$MP4BOX -rem-item 3 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-remove-items-3" do_hash_test $TEMP_DIR/test.mp4 "mp4box-remove-items-3"
do_test "$MP4BOX -rem-item 4 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-remove-items-4" do_hash_test $TEMP_DIR/test.mp4 "mp4box-remove-items-4"

test_end


test_begin "sets-meta-XML-data"

do_test "$MP4BOX -set-meta XML -set-xml $EXTERNAL_MEDIA_DIR/item/file.xml:id=1 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-sets-meta-XML-data"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-sets-meta-XML-data"

test_end


test_begin "dump-meta-XML-data"

do_test "$MP4BOX -dump-xml $TEMP_DIR/file1.xml:id=1 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-dump-meta-XML-data"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-dump-meta-XML-data"

test_end


test_begin "remove-meta-XML-data"

do_test "$MP4BOX -rem-xml id=1 $EXTERNAL_MEDIA_DIR/item/counter_noItems.mp4" "mp4box-remove-meta-XML-data"
do_hash_test $TEMP_DIR/test.mp4 "mp4box-remove-meta-XML-data"

test_end
