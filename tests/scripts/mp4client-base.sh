
#test MP4Client -guid
test_begin "mp4client-gui" "play"
if [ $test_skip  = 1 ] ; then
return
fi

do_playback_test "-guid" "MP4ClientGUI"

test_end
