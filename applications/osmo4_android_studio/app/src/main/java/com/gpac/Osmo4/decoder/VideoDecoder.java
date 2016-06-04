package com.gpac.Osmo4.decoder;

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

    private final String TAG = "VideoDecoder";
    private MediaCodec codec;
    private MediaExtractor extractor;
    private ByteBuffer[] inputBuffers;
    private ByteBuffer[] outputBuffers;
    private ByteBuffer buffer;
    private MediaFormat outputFormat;
    private String path;
    private int videoWidth;
    private int videoHeight;
    private long TIMEOUT = 1000000;     // equals 1 sec.

    public VideoDecoder(String path) {
        this.path = path;
    }

/*
    public void decode() {
        extractor = new MediaExtractor();

        try {
            extractor.setDataSource(path);
            Log.i(TAG, "Data source set: " + path);
        } catch (IOException e) {
            Log.e(TAG, "Cannot set data source");
            e.printStackTrace();
        }

        int trackNo = selectTrack();

        if (trackNo < 0)
            return;

        codec = getCodecForTrack(trackNo);

        if (codec == null) {
            return;
        }

        codec.start();
        inputBuffers = codec.getInputBuffers(); //should be replaced with ByteBuffer.allocate(int)
        outputBuffers = codec.getOutputBuffers();

        boolean isEOS = false;
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

        while(!Thread.interrupted()) {

            if(!isEOS) {

                int inputIndex = dequeueInputBuffer();
                if (inputIndex >= 0) {

                    ByteBuffer buffer;

                    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                        buffer = inputBuffers[inputIndex];
                        buffer.clear();
                    }
                    else buffer = codec.getInputBuffer(inputIndex);

                    assert buffer != null;
                    int sampleSize = extractor.readSampleData(buffer, 0);

                    if (sampleSize < 0) {
                        queueInputBuffer(inputIndex, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                        isEOS = true;
                        Log.i(TAG, "Input Buffer BUFFER_FLAG_END_OF_STREAM");
                    } else {
                        queueInputBuffer(inputIndex, sampleSize, extractor.getSampleTime(), 0);
                    }

                    inputBuffers[inputIndex].clear();
                    extractor.advance();
                }
            }

            int outputIndex = codec.dequeueOutputBuffer(info, TIMEOUT);

            ByteBuffer outData;
            if(outputIndex >= 0) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                    outData = outputBuffers[outputIndex];
                    outData.clear();
                } else outData = codec.getOutputBuffer(outputIndex);

            */
/*
            if(outputIndex >= 0 && info.size != 0) {

                outData.position(info.offset);
                outData.limit(info.offset + info.size);
            }
            *//*


                switch (outputIndex) {

                    case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                        Log.i(TAG, "Output buffers INFO_OUTPUT_BUFFERS_CHANGED");
                        outputBuffers = codec.getOutputBuffers();
                        break;

                    case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                        outputFormat = codec.getOutputFormat();
                        Log.i(TAG, "Output buffers INFO_OUTPUT_FORMAT_CHANGED. New format = " + outputFormat);
                        break;

                    case MediaCodec.INFO_TRY_AGAIN_LATER:
                        Log.i(TAG, "dequeueOutputBuffer() timed out");
                        break;

                    default:
                        codec.releaseOutputBuffer(outputIndex, false);
                        break;

                }
            }

            if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                Log.i(TAG, "Output buffer BUFFER_FLAG_END_OF_STREAM");
                break;
            }
        }

        release();
    }
*/


    private void init(){

        extractor = new MediaExtractor();
        try {
            extractor.setDataSource(path);
            Log.i(TAG, "Data source set: " + path);
        } catch (IOException e) {
            Log.e(TAG, "Cannot set data source");
            e.printStackTrace();
        }

        int trackNo = selectTrack();

        if (trackNo < 0)
            return;

        codec = getCodecForTrack(trackNo);

        if (codec == null) {
            return;
        }

        inputBuffers = codec.getInputBuffers();
        outputBuffers = codec.getOutputBuffers();

        codec.start();
    }


    private int selectTrack() {
        int trackCount = extractor.getTrackCount();

        for (int i = 0; i < trackCount; ++i) {
            MediaFormat format = extractor.getTrackFormat(i);
            String mime = format.getString(MediaFormat.KEY_MIME);
            Log.i(TAG, "Track num.: " + i + ", mime type: " + mime);
            if (mime.startsWith("video/")) {
                Log.i(TAG, "Selected Track num.: " + i + ", mime type: " + mime);
                extractor.selectTrack(i);
                videoWidth = format.getInteger(MediaFormat.KEY_WIDTH);
                videoHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
                Log.i(TAG, "Video size = " + videoWidth + " x " + videoHeight);
                return i;
            }
        }
        Log.e(TAG, "Video Track not found");
        return -1;
    }


    private MediaCodec getCodecForTrack(int trackNo) {
        if(trackNo >= 0) {
            try {
                MediaFormat format = extractor.getTrackFormat(trackNo);
                String mime = format.getString(MediaFormat.KEY_MIME);
                MediaCodec codec = MediaCodec.createDecoderByType(mime);
                codec.configure(format, null, null, 0);
                return codec;
            } catch (Exception e) {
                Log.e(TAG, "Exception while creating codec");
                e.printStackTrace();
                return null;
            }
        }
        Log.e(TAG, "Codec not found");
        return null;
    }


    private boolean release() {
        try {
            codec.stop();
            codec.release();
            extractor.release();
            codec = null;
            extractor = null;
            return true;
        } catch (Exception e) {
            Log.e(TAG, "release failed" + e);
            return false;
        }
    }


    private int dequeueInputBuffer() {
        try {
            return codec.dequeueInputBuffer(TIMEOUT);
        } catch (Exception e) {
            Log.e(TAG, "DequeueInputBuffer failed " + e);
            return -2;
        }
    }

    private boolean queueInputBuffer(int index, int size, long timeout,int flags) {
        try {
            inputBuffers[index].limit(size);
            inputBuffers[index].position(0);
            codec.queueInputBuffer(index, 0, size, timeout, flags);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "queueInputBuffer failed" + e);
            return false;
        }
    }

    private int dequeueOutputBuffer() {

        int outputIndex = -1;
        try {
            MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
            outputIndex = codec.dequeueOutputBuffer(info, TIMEOUT);

            switch (outputIndex) {

                case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:        //value -3
                    Log.i(TAG, "Output buffers INFO_OUTPUT_BUFFERS_CHANGED");
                    outputBuffers = codec.getOutputBuffers();
                    break;

                case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:         //value -2
                    Log.i(TAG, "Output buffers INFO_OUTPUT_FORMAT_CHANGED. New format = " + codec.getOutputFormat());
                    outputFormat = codec.getOutputFormat();
                    videoWidth = outputFormat.getInteger(MediaFormat.KEY_WIDTH);
                    videoHeight = outputFormat.getInteger(MediaFormat.KEY_HEIGHT);
                    break;

                case MediaCodec.INFO_TRY_AGAIN_LATER:               //value -1
                    Log.i(TAG, "dequeueOutputBuffer timed out");
                    break;

                default:
                    codec.releaseOutputBuffer(outputIndex, false);
                    break;

            }
        } catch (Exception e) {
            Log.e(TAG, "dequeuOutputBuffer failed " + e);
            e.printStackTrace();
        }

        return outputIndex;
    }

    private boolean releaseOutputBuffer(int index) {
        try {
            codec.releaseOutputBuffer(index, false);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "releaseOutputBuffer failed " + e);
            return false;
        }
    }


    private int readSampleData(){
        try {
            return extractor.readSampleData(buffer, 0);
        } catch (Exception e) {
            Log.e(TAG, "readSampleData failed " + e);
            return -1;
        }
    }

    private boolean advance() {
        try {
            return extractor.advance();
        } catch (Exception e) {
            Log.e(TAG, "advance failed " + e);
            return false;
        }
    }

    public int getVideoWidth() {
        return videoWidth;
    }

    public int getVideoHeight() {
        return videoHeight;
    }

}
