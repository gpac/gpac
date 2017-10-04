
#create our test file
mp4file="$TEMP_DIR/source_media.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


crypto_test()
{

cryptfile="$TEMP_DIR/$1-crypted.mp4"
decryptfile="$TEMP_DIR/$1-decrypted.mp4"

test_begin "encryption-$1"

if [ $test_skip  = 1 ] ; then
 return
fi

do_test "$MP4BOX -crypt $2 -out $cryptfile $mp4file" "Encrypt"
do_hash_test $cryptfile "crypt"

do_test "$MP4BOX -decrypt $2 -out $decryptfile $mp4file" "Decrypt"
do_hash_test $decryptfile "decrypt"

#compare hashes of source and decrypted
do_compare_file_hashes $mp4file $decryptfile
rv=$?

if [ $rv != 0 ] ; then
result="Hash is not the same between source content and decrypted content"
fi

if [ $1 != "adobe" ] ; then
do_playback_test "$cryptfile" "play"
fi

test_end

}

#test adobe
crypto_test "adobe" $MEDIA_DIR/encryption/drm_adobe.xml &

#test isma
crypto_test "isma" $MEDIA_DIR/encryption/drm_isma.xml &

#test cenc CTR
crypto_test "cenc-ctr" $MEDIA_DIR/encryption/drm_ctr.xml &

#test cenc CBC
crypto_test "cenc-cbc" $MEDIA_DIR/encryption/drm_cbc.xml &

#test cenc CENS
crypto_test "cenc-cens" $MEDIA_DIR/encryption/drm_cens.xml &

#test cenc CBCS
crypto_test "cenc-cbcs" $MEDIA_DIR/encryption/drm_cbcs.xml &

#test cenc CBCS constant
crypto_test "cenc-cbcs-const" $MEDIA_DIR/encryption/drm_cbcs_const.xml &


wait
rm -f $mp4file 2> /dev/null
