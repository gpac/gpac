
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=555 -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-555-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=555 -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-555-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=555 -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-555-yuv"

single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=565 -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-565-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=565 -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-565-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=565 -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-565-yuv"

single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgb -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-rgb-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgb -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-rgb-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgb -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-rgb-yuv"

single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=bgr -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-bgr-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=bgr -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-bgr-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=bgr -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-bgr-yuv"

single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgb32 -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-rgb32-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgb32 -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-rgb32-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgb32 -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-rgb32-yuv"

single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=bgr32 -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-bgr32-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=bgr32 -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-bgr32-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=bgr32 -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-bgr32-yuv"

single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgba -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-rgba-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgba -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-rgba-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=rgba -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-rgba-yuv"

single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=argb -no-save $MEDIA_DIR/auxiliary_files/logo.jpg" "mp4client-rawout-argb-jpg"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=argb -no-save $MEDIA_DIR/auxiliary_files/count_video.cmp" "mp4client-rawout-argb-yuv"
single_playback_test "-opt Video:DriverName=raw_out -opt RAWVideo:PixelFormat=argb -no-save $MEDIA_DIR/bifs/bifs-2D-background-background2D-image.bt" "mp4client-rawout-argb-yuv"

