package com.gpac.mp4box;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import com.gpac.mp4box.R;

import android.app.Activity;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Environment;
import android.text.InputType;
import android.text.method.ScrollingMovementMethod;
import android.widget.Button;
import android.widget.EditText;
import android.util.Log;
import android.view.View;

import android.content.pm.PackageManager;
import android.content.Context;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.widget.TextView;

import java.io.OutputStreamWriter;
import java.io.IOException;

public class mp4box extends Activity {

	private mp4terminal myTerminal;


    private final static String LOG_LIB = "com.gpac.mp4box.loader";
    private static Map<String, Throwable> errors = null;

    synchronized static Map<String, Throwable> loadAllLibraries() {

        final String[] toLoad = {"avutil", "swresample", "swscale", "avcodec", "avformat", "avfilter", "avdevice",
                "editline", "faad", "ft2",
                "jpegdroid", "js_osmo", "mad", "openjpeg",
                "png", "stlport_shared",
                "z", "gpac", "mp4box"};

        HashMap<String, Throwable> exceptions = new HashMap<String, Throwable>();

        for (String s : toLoad) {
            try {
                Log.i(LOG_LIB, "Loading library " + s + "...");
                System.loadLibrary(s);

            } catch (UnsatisfiedLinkError e) {
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


    public void reloadUIStatus() {

        final EditText cmdlineTxt = (EditText) findViewById(R.id.CommandLineEdit);
        ArrayList<String> cmdlineFile = readFromFile("mp4box.cmdline.txt");
        if (cmdlineFile.size()>0) {
            cmdlineTxt.setText(cmdlineFile.get(0));
        }


        final TextView stdoutTxt = (TextView) findViewById(R.id.stdoutView);
        ArrayList<String> stdoutFile = readFromFile("mp4box.stderrout.txt");
        if (stdoutFile.size()>0) {
            stdoutTxt.setText("");
            for (String line: stdoutFile) {
                stdoutTxt.append(line + "\n");
            }
        }
        stdoutTxt.setMovementMethod(new ScrollingMovementMethod());

    }

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        System.out.println( "hello world java" );

        setContentView(R.layout.main);

        reloadUIStatus();

        myTerminal = new mp4terminal();
        errors = loadAllLibraries();

        final Button button = (Button) findViewById(R.id.OkButton);
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {

            	EditText oCommandLine = (EditText) findViewById(R.id.CommandLineEdit );
            	CharSequence sCommandLine = oCommandLine.getText();
                writeToFile(sCommandLine.toString(), "mp4box.cmdline.txt");

            	myTerminal.run( sCommandLine.toString() );


                doRestart(getApplicationContext());
            }
        });

    }

    private void writeToFile(String string, String filename) {
        try {

            File file = new File(Environment.getExternalStorageDirectory(), filename);
            if (!file.exists())
                file.createNewFile();

            FileOutputStream fos;
            byte[] data = string.getBytes();
            fos = new FileOutputStream(file);
            fos.write(data);
            fos.flush();
            fos.close();

        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    private ArrayList<String> readFromFile(String filename) {

        ArrayList<String> lines = new ArrayList<>();

        try {

            File file = new File(Environment.getExternalStorageDirectory(), filename);
            BufferedReader br = new BufferedReader(new FileReader(file));

            String line;
            while ((line = br.readLine()) != null) {
                lines.add(line);
            }
            br.close();
        }

        catch (Exception e) {
            Log.d(LOG_LIB, "While opening " + filename + " got: " + e.toString());
        }

        return lines;
    }



    public static void doRestart(Context c) {
        try {
            //check if the context is given
            if (c != null) {
                //fetch the packagemanager so we can get the default launch activity
                // (you can replace this intent with any other activity if you want
                PackageManager pm = c.getPackageManager();
                //check if we got the PackageManager
                if (pm != null) {
                    //create the intent with the default start activity for your application
                    Intent mStartActivity = pm.getLaunchIntentForPackage(
                            c.getPackageName()
                    );
                    if (mStartActivity != null) {
                        mStartActivity.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
                        //create a pending intent so the application is restarted after System.exit(0) was called.
                        // We use an AlarmManager to call this intent in 100ms
                        int mPendingIntentId = 223344;
                        PendingIntent mPendingIntent = PendingIntent
                                .getActivity(c, mPendingIntentId, mStartActivity,
                                        PendingIntent.FLAG_CANCEL_CURRENT);
                        AlarmManager mgr = (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
                        mgr.set(AlarmManager.RTC, System.currentTimeMillis() + 100, mPendingIntent);
                        //kill the application
                        System.exit(0);
                    } else {
                        Log.e(LOG_LIB, "Was not able to restart application, mStartActivity null");
                    }
                } else {
                    Log.e(LOG_LIB, "Was not able to restart application, PM null");
                }
            } else {
                Log.e(LOG_LIB, "Was not able to restart application, Context null");
            }
        } catch (Exception ex) {
            Log.e(LOG_LIB, "Was not able to restart application");
        }
    }

}