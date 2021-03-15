package com.gpac.mp4box;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import com.gpac.mp4box.R;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.util.Log;
import android.view.View;



public class mp4box extends Activity {
	private mp4terminal myTerminal;

    /** Called when the activity is first created. */
    @Override


    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        myTerminal = new mp4terminal();
        errors = loadAllLibraries();
        System.out.println( "hello world java" );
        final Button button = (Button) findViewById(R.id.OkButton);

        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
            	EditText oCommandLine = (EditText) findViewById(R.id.CommandLineEdit );
            	CharSequence sCommandLine = oCommandLine.getText();
            	myTerminal.run( sCommandLine.toString() );
            	//showKeyboard( true );
            }
        });

    }


    private final static String LOG_LIB = "com.gpac.mp4box.libloader";

    private static Map<String, Throwable> errors = null;

    synchronized static Map<String, Throwable> loadAllLibraries()
    {
    	if( errors != null)
    		return errors;
        StringBuilder sb = new StringBuilder();
        // final String[] toLoad = { "GLESv1_CM", "dl", "log",//$NON-NLS-3$ //$NON-NLS-2$ //$NON-NLS-1$
        //                      "jpeg", "javaenv", //$NON-NLS-1$ //$NON-NLS-2$
        //                      "mad", "editline", "ft2", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
        //                      "js_osmo", "openjpeg", "png", "z", //$NON-NLS-1$ //$NON-NLS-2$//$NON-NLS-3$ //$NON-NLS-4$
        //                      "ffmpeg", "faad", "gpac", //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$
        //                      "stdc++", "mp4box" }; // //$NON-NLS-1$ //$NON-NLS-2$
        final String[] toLoad = {"avutil", "swresample", "swscale", "avcodec", "avformat", "avfilter", "avdevice",
                "editline", "faad", "ft2",
                "jpegdroid", "js_osmo", "mad", "openjpeg",
                "png", "stlport_shared",
                "z", "gpac", "mp4box"};
        HashMap<String, Throwable> exceptions = new HashMap<String, Throwable>();
    for (String s : toLoad) {
        try {
            String msg = "Loading library " + s + "...";
            sb.append(msg);
            Log.i(LOG_LIB, msg);
            System.loadLibrary(s);
        } catch (UnsatisfiedLinkError e) {
            sb.append("Failed to load " + s + ", error=" + e.getLocalizedMessage() + " :: "
                      + e.getClass().getSimpleName() + "\n"); //$NON-NLS-1$
            exceptions.put(s, e);
            Log.e(LOG_LIB, "Failed to load library : " + s + " due to link error " + e.getLocalizedMessage(), e);
        } catch (SecurityException e) {
            exceptions.put(s, e);
            Log.e(LOG_LIB, "Failed to load library : " + s + " due to security error " + e.getLocalizedMessage(), e);
        } catch (Throwable e) {
            exceptions.put(s, e);
            Log.e(LOG_LIB, "Failed to load library : " + s + " due to Runtime error " + e.getLocalizedMessage(), e);
        }
    }

    errors = Collections.unmodifiableMap(exceptions);
    return errors;
}

}