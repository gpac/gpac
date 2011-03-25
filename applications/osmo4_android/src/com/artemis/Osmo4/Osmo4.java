/*
 *  Osmo on Android
 *  Aug/2010
 *  NGO Van Luyen
 * test test
 *
 */
package com.artemis.Osmo4;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import android.util.Log;

public class Osmo4 extends Activity{

	private Osmo4GLSurfaceView mGLView = null;
	private String[] m_modules_list;
	//---------------------------------------
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        loadAllModules();

        mGLView = new Osmo4GLSurfaceView(this);
        mGLView.setFocusable(true);
        mGLView.setFocusableInTouchMode(true);
        setContentView(mGLView);

    }
    //---------------------------------------

    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        return true;
    }
    //---------------------------------------
    protected void OpenFileDialog()
    {
    	Intent intent = new Intent();
    	intent.setAction(Intent.ACTION_PICK);
    	Uri startDir = Uri.fromFile(new File("/sdcard/osmo"));
    	// Files and directories
    	intent.setDataAndType(startDir, "vnd.android.cursor.dir/lysesoft.andexplorer.file");
    	// Optional filtering on file extension.
    	intent.putExtra("browser_filter_extension_whitelist", "*.mp4,*.bt,*.xmt,*.xml,*.ts,*.svg,*.mp3,*.m3u8,*.mpg,*.aac,*.m4a,*.jpg");
    	// Title
    	intent.putExtra("explorer_title", "Select MPEG-4 file");
    	// Optional colors
    	//intent.putExtra("browser_title_background_color", "440000AA");
    	//intent.putExtra("browser_title_foreground_color", "FFFFFFFF");
    	//intent.putExtra("browser_list_background_color", "66000000");
    	// Optional font scale
    	//intent.putExtra("browser_list_fontscale", "120%");
    	// Optional 0=simple list, 1 = list with filename and size, 2 = list with filename, size and date.
    	intent.putExtra("browser_list_layout", "2");
    	startActivityForResult(intent, 0);

    }
    //---------------------------------------
    protected void onActivityResult(int requestCode, int resultCode, Intent intent)
    {
       if (requestCode == 0)
       {
	       if (resultCode == RESULT_OK)
	       {
	          Uri uri = intent.getData();
	          String type = intent.getType();
	          if (uri != null)
	          {
	             String path = uri.toString();
	             if (path.toLowerCase().startsWith("file://"))
	             {
	                // Selected file/directory path is below
	            	 GpacObject.gpacconnect(path);

	             }

	          }
	       }
       }
    }
    //---------------------------------------

    public void onConfigurationChanged(Configuration newConfig)
    {
    	super.onConfigurationChanged(newConfig);
    }
    //---------------------------------------

    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
        case R.id.open_file:
            //newGame();
        	OpenFileDialog();
            return true;
        case R.id.quit:
        	this.finish();
            //quit();
            return true;
        default:
            return super.onOptionsItemSelected(item);
        }
    }
    //---------------------------------------
    protected void onDestroy() {
    	super.onDestroy();
    	GpacObject.gpacfree();
    }
    //---------------------------------------
    public void log(String msg1, String msg2){
    	Log.e(msg1, msg2);
    }
    //---------------------------------------
    private void loadAllModules(){
    	int[] ids = getAllRawResources();
    	for (int i=0; i<ids.length; i++){
        	try {
        		InputStream ins = getResources().openRawResource(ids[i]);
        		int size = ins.available();
        		byte[] buffer = new byte[size];
        		ins.read(buffer);
        		ins.close();

        		/*String fn = String.valueOf(ids[i]);
        		fn = "/data/data/com.artemis.Osmo4/gm_" + fn + ".so";*/
        		String fn = m_modules_list[i];
        		fn = "/data/data/com.artemis.Osmo4/" + fn + ".so";

        		FileOutputStream fos = new FileOutputStream(fn);
        		if (fos != null){
        			fos.write(buffer);
        			fos.close();
        		} else { Log.e("Osmo4", "FileOutputStream error"); }

        	} catch (IOException e){
        		Log.e("Osmo4", "loadModules error");
        	}
    	}
    }
    //---------------------------------------
    private int[] getAllRawResources() {
		int[] ids = null;
		R.raw r = new R.raw();

		java.lang.reflect.Field fields[] = R.raw.class.getDeclaredFields();
		ids = new int[fields.length];
		m_modules_list = new String[fields.length];

		try {
			for (int i = 0; i < fields.length; i++) {
				java.lang.reflect.Field f = fields[i];
				ids[i] = f.getInt(r);
				m_modules_list[i] = f.getName();
				Log.i("Osmo4", "R.raw." + f.getName() + " = 0x" + Integer.toHexString(ids[i]));
			}
		} catch (IllegalArgumentException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

		return ids;
      }
    //---------------------------------------
}
