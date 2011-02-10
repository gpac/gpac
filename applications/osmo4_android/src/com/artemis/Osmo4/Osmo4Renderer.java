package com.artemis.Osmo4;

import java.io.File;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.graphics.Bitmap;
import android.opengl.GLSurfaceView;
import android.os.Environment;

public class Osmo4Renderer implements GLSurfaceView.Renderer {
	private String m_cfg_dir = "/sdcard/osmo/";
	private String m_modules_dir = "/data/data/com.artemis.Osmo4/";
	private String m_cache_dir = "/sdcard/osmo/cache/";
	private String m_font_dir = "/system/fonts/";
	
	//------------------------------------
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
    	initGPACDir();
    }
    
    boolean inited = false;
    //------------------------------------
    public void onSurfaceChanged(GL10 gl, int w, int h) {
        //gl.glViewport(0, 0, w, h);
    	if ( !inited )
    	{
    		GpacObject.gpacinit(null, w, h, m_cfg_dir, m_modules_dir, m_cache_dir, m_font_dir);
    		GpacObject.gpacresize(w, h);
    		inited = true;
    	}
    	else
    		GpacObject.gpacresize(w, h);
    }
    //------------------------------------
    public void onDrawFrame(GL10 gl) {
    	GpacObject.gpacrender(null);
    }
    //------------------------------------
	private void initGPACDir(){
		
	    File root = Environment.getExternalStorageDirectory();
	    if (root.canWrite()){
	        File osmo_dir = new File(root, "osmo");
	        if (!osmo_dir.exists()){
	        	osmo_dir.mkdirs();
	        }
	        
	        File cache_dir = new File(root, "osmo/cache");
	        if (!cache_dir.exists()){
	        	cache_dir.mkdirs();
	        }
	    }
	}
	//------------------------------------
}
