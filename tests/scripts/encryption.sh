
#create our test file
mp4file="$TEMP_DIR/crypto_test.mp4"
$MP4BOX -add $MEDIA_DIR/auxiliary_files/enst_video.h264 -add $MEDIA_DIR/auxiliary_files/enst_audio.aac -new $mp4file 2> /dev/null


crypto_test()
{

cryptfile="$TEMP_DIR/$1-crypted.mp4"
decryptfile="$TEMP_DIR/$1-decrypted.mp4"

test_begin $1 "crypt" "decrypt"
if [ $test_skip  = 1 ] ; then
 return
fi

do_test "$MP4BOX -crypt $2 -out $cryptfile $mp4file" "Encrypt"
do_hash_test $cryptfile "crypt"


do_test "$MP4BOX -decrypt $2 -out $decryptfile $mp4file" "Decrypt"
do_hash_test $decryptfile "decrypt"

rm $cryptfile 2> /dev/null
rm $decryptfile 2> /dev/null

test_end

}

#test adobe
crypto_test "adobe" $MEDIA_DIR/crypto/drm_adobe.xml &

#test isma
crypto_test "isma" $MEDIA_DIR/crypto/drm_isma.xml &

#test cenc CTR
crypto_test "cenc-ctr" $MEDIA_DIR/crypto/drm_ctr.xml &

#test cenc CBC
crypto_test "cenc-cbc" $MEDIA_DIR/crypto/drm_cbc.xml &



wait
rm -f $mp4file 2> /dev/null