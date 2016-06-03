package com.gpac.Osmo4.decoder;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.os.Build;
import android.util.Log;
import android.view.Surface;

import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Created by abdul on 1/6/16.
 */
public class VideoDecoder {

    private final int VIDEO_TRACK_NOT_FOUND = -1;

    private final String TAG = "VideoDecoder";
    private MediaExtractor extractor;
    private MediaCodec codec;
    private MediaFormat format;
    private Surface surface;
    private String path;
    private int videoWidth;
    private int videoHeight;

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public VideoDecoder(String path, Surface surface) throws IOException {
        this.extractor = new MediaExtractor();
        this.path = path;
        this.surface = surface;
    }


    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    public void decode() {

        try {
            extractor.setDataSource(path);
        } catch (IOException e) {
            Log.e(TAG, "Cannot set data source");
            e.printStackTrace();
        }

        int trackNo = selectTrack();

        if(trackNo >= 0) {
            try {
                String mime = format.getString(MediaFormat.KEY_MIME);
                codec = MediaCodec.createDecoderByType(mime);
                codec.configure(format, surface, null, 0);
            } catch (Exception e) {
                Log.e(TAG, "codec not found");
                e.printStackTrace();
                Thread.currentThread().interrupt();
            }
        }

        if (codec == null) {
            Log.e(TAG, "Cannot create the codec");
            Thread.currentThread().stop();
        }

        codec.start();
        ByteBuffer[] inputBuffers = codec.getInputBuffers(); //should be replaced with ByteBuffer.allocate(int)
        ByteBuffer[] outputBuffers = codec.getOutputBuffers();

        MediaFormat outputFormat = codec.getOutputFormat();
        long TIMEOUT = 10000;
        boolean isEOS = false;
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

        while(!Thread.interrupted()) {

            if(!isEOS) {

                int inputIndex = codec.dequeueInputBuffer(TIMEOUT);
                if (inputIndex >= 0) {

                    ByteBuffer buffer;
                    buffer = inputBuffers[inputIndex];    //replace with call to getInputBuffer()
                    buffer.clear();

                    int sampleSize = extractor.readSampleData(buffer, 0);

                    if (sampleSize < 0) {
                        codec.queueInputBuffer(inputIndex, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                        isEOS = true;
                        Log.i(TAG, "Input Buffer BUFFER_FLAG_END_OF_STREAM");
                    } else {
                        codec.queueInputBuffer(inputIndex, 0, sampleSize, extractor.getSampleTime(), 0);
                    }

                    inputBuffers[inputIndex].clear();
                }
            }

            int outputIndex = codec.dequeueOutputBuffer(info, TIMEOUT);

            ByteBuffer outData;
            if(outputIndex >= 0) {
                outData = outputBuffers[outputIndex];
            }

            /*
            if(outputIndex >= 0 && info.size != 0) {

                outData.position(info.offset);
                outData.limit(info.offset + info.size);
            }
            */

            switch (outputIndex) {

                case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                    Log.i(TAG, "Output buffers INFO_OUTPUT_BUFFERS_CHANGED");
                    outputBuffers = codec.getOutputBuffers();
                    break;

                case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                    Log.i(TAG, "Output buffers INFO_OUTPUT_FORMAT_CHANGED. New format = " + codec.getOutputFormat());
                    outputFormat = codec.getOutputFormat();
                    break;

                case MediaCodec.INFO_TRY_AGAIN_LATER:
                    Log.i(TAG, "dequeueOutputBuffer() timed out");
                    break;

                default:
                    codec.releaseOutputBuffer(outputIndex, true);
                    break;

            }

            extractor.advance();

            if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                Log.i(TAG, "Output buffer BUFFER_FLAG_END_OF_STREAM");
                break;
            }
        }

        codec.stop();
        codec.release();
        extractor.release();
    }

    @TargetApi(Build.VERSION_CODES.JELLY_BEAN)
    private int selectTrack() {
        int trackCount = extractor.getTrackCount();

        for (int i = 0; i < trackCount; ++i) {
            format = extractor.getTrackFormat(i);
            String mime = format.getString(MediaFormat.KEY_MIME);
            Log.i(TAG, "Track id: " + i + ", mime type: " + mime);
            if (mime.startsWith("video/")) {
                extractor.selectTrack(i);
                videoWidth = format.getInteger(MediaFormat.KEY_WIDTH);
                videoHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
                Log.i(TAG, "Selected Track id: " + i + ", mime type: " + mime);
                Log.i(TAG, "Video size = " + videoWidth + " x " + videoHeight);
                return i;
            }
        }
        Log.i(TAG, "Video Track not found");
        return VIDEO_TRACK_NOT_FOUND;
    }



    public int getVideoWidth() {
        return videoWidth;
    }

    public int getVideoHeight() {
        return videoHeight;
    }


}
