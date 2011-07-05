package com.enst.mp4box;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import com.enst.mp4box.R;

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
    

    
    
    private static Map<String, Throwable> errors = null;
    
    synchronized static Map<String, Throwable> loadAllLibraries() 
    {
    	if( errors != null)
    		return errors;
        StringBuilder sb = new StringBuilder();
        final String[] toLoad = { "GLESv1_CM", "dl", "log",//$NON-NLS-3$ //$NON-NLS-2$ //$NON-NLS-1$
                             "jpeg", "javaenv", //$NON-NLS-1$ //$NON-NLS-2$ 
                             "mad", "editline", "ft2", //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
                             "js_osmo", "openjpeg", "png", "z", //$NON-NLS-1$ //$NON-NLS-2$//$NON-NLS-3$ //$NON-NLS-4$
                             "ffmpeg", "faad", "gpac", //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$
                             "stdc++", "mp4box" }; // //$NON-NLS-1$ //$NON-NLS-2$
        HashMap<String, Throwable> exceptions = new HashMap<String, Throwable>();
    for (String s : toLoad) {
        try {
            String msg = "Loading library " + s + "...";//$NON-NLS-1$//$NON-NLS-2$
            sb.append(msg);
            //Log.i(LOG_LIB, msg);
            System.loadLibrary(s);
        } catch (UnsatisfiedLinkError e) {
            sb.append("Failed to load " + s + ", error=" + e.getLocalizedMessage() + " :: " //$NON-NLS-1$//$NON-NLS-2$//$NON-NLS-3$
                      + e.getClass().getSimpleName() + "\n"); //$NON-NLS-1$
            exceptions.put(s, e);
            //Log.e(LOG_LIB, "Failed to load library : " + s + " due to link error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
        } catch (SecurityException e) {
            exceptions.put(s, e);
            //Log.e(LOG_LIB, "Failed to load library : " + s + " due to security error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
        } catch (Throwable e) {
            exceptions.put(s, e);
            //Log.e(LOG_LIB, "Failed to load library : " + s + " due to Runtime error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
        }
    }

    /*if (!exceptions.isEmpty()) {
        try {
            PrintStream out = new PrintStream(config.getGpacConfigDirectory() + "debug_libs.txt", "UTF-8"); //$NON-NLS-1$//$NON-NLS-2$
            out.println("$Revision: 2972 $"); //$NON-NLS-1$
            out.println(new Date());
            out.println("\n*** Configuration\n"); //$NON-NLS-1$
            out.println(config.getConfigAsText());
            sb.append("*** Libs listing: "); //$NON-NLS-1$
            sb.append(config.getGpacLibsDirectory());
            sb.append('\n');
            listing(sb, new File(config.getGpacLibsDirectory()), 2);
            sb.append("*** Modules listing: "); //$NON-NLS-1$
            sb.append(config.getGpacModulesDirectory());
            sb.append('\n');
            listing(sb, new File(config.getGpacModulesDirectory()), 2);
            sb.append("*** Fonts listing: \n"); //$NON-NLS-1$
            sb.append(config.getGpacFontDirectory());
            sb.append('\n');
            listing(sb, new File(config.getGpacFontDirectory()), 2);
            sb.append("*** Exceptions:\n"); //$NON-NLS-1$
            for (Map.Entry<String, Throwable> ex : exceptions.entrySet()) {
                sb.append(ex.getKey()).append(": ") //$NON-NLS-1$
                  .append(ex.getValue().getLocalizedMessage())
                  .append('(')
                  .append(ex.getValue().getClass())
                  .append(")\n"); //$NON-NLS-1$
            }
            out.println(sb.toString());
            out.flush();
            out.close();
        } catch (Exception e) {
            Log.e(LOG_LIB, "Failed to output debug info to debug file", e); //$NON-NLS-1$
        }
    }*/
    errors = Collections.unmodifiableMap(exceptions);
    return errors;
}
    /*public void showKeyboard(boolean showKeyboard) {
        if (keyboardIsVisible == showKeyboard == true)
            return;
        InputMethodManager mgr = ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE));
        this.keyboardIsVisible = showKeyboard;
        if (showKeyboard)
            mgr.showSoftInput(findViewById(R.id.CommandLineEdit ), 0);
        else
            mgr.hideSoftInputFromInputMethod(findViewById(R.id.CommandLineEdit ).getWindowToken(), 0);

    }*/
}