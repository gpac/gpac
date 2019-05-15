#this tests a web radio, exercising icy meta skip and chunk transfer

test_begin "webradio"
if [ $test_skip  = 1 ] ; then
return
fi

do_test "$GPAC -i http://direct.fipradio.fr/live/fip-midfi.mp3 inspect:dur=1/1" "fip"

test_end

