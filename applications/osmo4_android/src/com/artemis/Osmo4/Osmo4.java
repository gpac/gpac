/**
 *  Osmo on Android
 *  Aug/2010
 *  NGO Van Luyen
 * $Id$
 *
 */
package com.artemis.Osmo4;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.os.PowerManager;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Toast;

/**
 * The main Osmo4 activity, used to launch everything
 * 
 * @version $Revision$
 * 
 */
public class Osmo4 extends Activity implements GpacCallback {

    private Osmo4GLSurfaceView mGLView = null;

    private String[] m_modules_list;

    private final static String LOG_OSMO_TAG = "Osmo4"; //$NON-NLS-1$

    /**
     * List of all extensions recognized by Osmo
     */
    public final static String OSMO_REGISTERED_FILE_EXTENSIONS = "*.mp4,*.bt,*.xmt,*.xml,*.ts,*.svg,*.mp3,*.m3u8,*.mpg,*.aac,*.m4a,*.jpg,*.png"; //$NON-NLS-1$

    private PowerManager.WakeLock wl = null;

    // ---------------------------------------
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final String name = "Osmo4"; //$NON-NLS-1$
        displayMessage("Loading native libraries...", name); //$NON-NLS-1$
        loadAllModules();
        displayMessage("Initializing view...", name); //$NON-NLS-1$
        mGLView = new Osmo4GLSurfaceView(this);
        mGLView.setRenderer(new Osmo4Renderer(this));
        mGLView.setFocusable(true);
        mGLView.setFocusableInTouchMode(true);
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wl = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, LOG_OSMO_TAG);
        setContentView(mGLView);
        displayMessage("Now loading, please wait...", name); //$NON-NLS-1$
        if (wl != null)
            wl.acquire();
    }

    // ---------------------------------------

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        return true;
    }

    private String getRecentURLsFile() {
        return Osmo4Renderer.GPAC_CFG_DIR + "recentURLs.txt"; //$NON-NLS-1$
    }

    private boolean openURL() {
        Future<String[]> res = service.submit(new Callable<String[]>() {

            @Override
            public String[] call() throws Exception {
                BufferedReader reader = null;
                try {
                    reader = new BufferedReader(new InputStreamReader(new FileInputStream(getRecentURLsFile()),
                                                                      DEFAULT_ENCODING));

                    String s = null;
                    Set<String> results = new HashSet<String>();
                    while (null != (s = reader.readLine())) {
                        results.add(s);
                    }
                    addAllRecentURLs(results);
                    return results.toArray(new String[0]);
                } finally {
                    if (reader != null)
                        reader.close();
                }
            }
        });
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        final AutoCompleteTextView textView = new AutoCompleteTextView(this);

        builder.setMessage("Please enter an URL to connect to...") //$NON-NLS-1$
               .setCancelable(true)
               .setPositiveButton("Open URL", new DialogInterface.OnClickListener() { //$NON-NLS-1$

                                      public void onClick(DialogInterface dialog, int id) {
                                          dialog.cancel();
                                          final String newURL = textView.getText().toString();
                                          GpacObject.gpacconnect(newURL);
                                          service.execute(new Runnable() {

                                              @Override
                                              public void run() {
                                                  addAllRecentURLs(Collections.singleton(newURL));
                                                  File tmp = new File(getRecentURLsFile() + ".tmp"); //$NON-NLS-1$
                                                  BufferedWriter w = null;
                                                  try {
                                                      w = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(tmp),
                                                                                                    DEFAULT_ENCODING));
                                                      Collection<String> toWrite = getAllRecentURLs();
                                                      for (String s : toWrite) {
                                                          w.write(s);
                                                          w.write("\n"); //$NON-NLS-1$
                                                      }
                                                      w.close();
                                                      w = null;
                                                      if (tmp.renameTo(new File(getRecentURLsFile())))
                                                          Log.e(LOG_OSMO_TAG,
                                                                "Failed to rename " + tmp + " to " + getRecentURLsFile()); //$NON-NLS-1$//$NON-NLS-2$

                                                  } catch (IOException e) {
                                                      Log.e(LOG_OSMO_TAG, "Failed to write recent URLs to " + tmp, e); //$NON-NLS-1$
                                                      try {
                                                          if (w != null)
                                                              w.close();
                                                      } catch (IOException ex) {
                                                          Log.e(LOG_OSMO_TAG, "Failed to close stream " + tmp, ex); //$NON-NLS-1$
                                                      }
                                                  }

                                              }
                                          });
                                      }
                                  })
               .setNegativeButton("Cancel", new DialogInterface.OnClickListener() { //$NON-NLS-1$

                                      public void onClick(DialogInterface dialog, int id) {
                                          dialog.cancel();
                                      }
                                  });

        textView.setText("http://"); //$NON-NLS-1$
        builder.setView(textView);

        builder.create();
        builder.show();
        ArrayAdapter<String> adapter;
        try {
            adapter = new ArrayAdapter<String>(this,
                                               android.R.layout.simple_dropdown_item_1line,
                                               res.get(1, TimeUnit.SECONDS));
            textView.setAdapter(adapter);
        } catch (ExecutionException e) {
            // Ignored
            Log.e(LOG_OSMO_TAG, "Error while parsing recent URLs", e); //$NON-NLS-1$
        } catch (TimeoutException e) {
            Log.e(LOG_OSMO_TAG, "It took too long to parse recent URLs", e); //$NON-NLS-1$
        } catch (InterruptedException e) {
            Log.e(LOG_OSMO_TAG, "Interrupted while parsing recent URLs", e); //$NON-NLS-1$
        }

        return true;
    }

    private final ExecutorService service = Executors.newSingleThreadExecutor();

    private final Set<String> allRecentURLs = new HashSet<String>();

    private synchronized void addAllRecentURLs(Collection<String> urlsToAdd) {
        allRecentURLs.addAll(urlsToAdd);
    }

    private synchronized Collection<String> getAllRecentURLs() {
        return new ArrayList<String>(allRecentURLs);
    }

    private final static Charset DEFAULT_ENCODING = Charset.forName("UTF-8"); //$NON-NLS-1$

    /**
     * Opens a new activity to select a file
     * 
     * @return true if activity has been selected
     */
    private boolean openFileDialog() {
        Intent intent = new Intent("org.openintents.action.PICK_FILE"); //$NON-NLS-1$
        //Intent intent = new Intent("org.openintents.action.PICK_FILE"); //$NON-NLS-1$
        intent.setData(Uri.fromFile(new File(Osmo4Renderer.GPAC_CFG_DIR)));
        intent.putExtra("org.openintents.extra.TITLE", "Please select a file"); //$NON-NLS-1$//$NON-NLS-2$
        intent.putExtra("browser_filter_extension_whitelist", OSMO_REGISTERED_FILE_EXTENSIONS); //$NON-NLS-1$

        try {
            startActivityForResult(intent, 1);
            return true;
        } catch (ActivityNotFoundException e) {
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setMessage("Impossible to find an Intent to choose a file... Cannot open file !") //$NON-NLS-1$
                   .setCancelable(true)
                   .setPositiveButton("Close", new DialogInterface.OnClickListener() { //$NON-NLS-1$

                                          public void onClick(DialogInterface dialog, int id) {
                                              dialog.cancel();
                                          }
                                      });
            AlertDialog alert = builder.create();
            alert.show();
            return false;
        }
    }

    // ---------------------------------------
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        if (requestCode == 1) {
            if (resultCode == RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    String path = uri.toString();
                    GpacObject.gpacconnect(path);
                }
            }
        }
    }

    // ---------------------------------------

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    // ---------------------------------------

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
            case R.id.open_url:
                return openURL();
            case R.id.open_file:
                // newGame();
                return openFileDialog();
            case R.id.quit:
                this.finish();
                // quit();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    // ---------------------------------------
    @Override
    protected void onDestroy() {
        service.shutdown();
        if (wl != null)
            wl.release();
        super.onDestroy();
        GpacObject.gpacfree();
    }

    // ---------------------------------------
    private void loadAllModules() {
        Log.i(LOG_OSMO_TAG, "Start loading all modules..."); //$NON-NLS-1$
        long start = System.currentTimeMillis();
        byte buffer[] = new byte[1024];
        int[] ids = getAllRawResources();
        for (int i = 0; i < ids.length; i++) {
            OutputStream fos = null;
            InputStream ins = null;
            String fn = Osmo4Renderer.GPAC_MODULES_DIR + m_modules_list[i] + ".so"; //$NON-NLS-1$
            File finalFile = new File(fn);
            // If file has already been copied, not need to redo it
            if (finalFile.exists() && finalFile.canRead() && 0 < finalFile.length())
                continue;
            try {
                Log.i(LOG_OSMO_TAG, "Copying resource " + ids[i] + " to " //$NON-NLS-1$//$NON-NLS-2$
                                    + finalFile.getAbsolutePath());
                File tmpFile = new File(fn + ".tmp"); //$NON-NLS-1$
                int read;
                ins = new BufferedInputStream(getResources().openRawResource(ids[i]));
                fos = new BufferedOutputStream(new FileOutputStream(tmpFile));
                while (0 < (read = ins.read(buffer))) {
                    fos.write(buffer, 0, read);
                }
                ins.close();
                ins = null;
                fos.close();
                fos = null;
                if (!tmpFile.renameTo(finalFile))
                    Log.e(LOG_OSMO_TAG, "Failed to rename " + tmpFile.getAbsolutePath() + " to " //$NON-NLS-1$//$NON-NLS-2$
                                        + finalFile.getAbsolutePath());
            } catch (IOException e) {
                Log.e(LOG_OSMO_TAG, "IOException for resource : " + ids[i], e); //$NON-NLS-1$
            } finally {
                if (ins != null) {
                    try {
                        ins.close();
                    } catch (IOException e) {
                        Log.e(LOG_OSMO_TAG, "Error while closing read stream", e); //$NON-NLS-1$
                    }
                }
                if (fos != null) {
                    try {
                        fos.close();
                    } catch (IOException e) {
                        Log.e(LOG_OSMO_TAG, "Error while closing write stream", e); //$NON-NLS-1$
                    }
                }
            }
        }
        Log.i(LOG_OSMO_TAG, "Done loading all modules, took " + (System.currentTimeMillis() - start) + "ms."); //$NON-NLS-1$ //$NON-NLS-2$
    }

    private int[] getAllRawResources() throws RuntimeException {
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
                Log.i(LOG_OSMO_TAG, "R.raw." + f.getName() + " = 0x" + Integer.toHexString(ids[i])); //$NON-NLS-1$ //$NON-NLS-2$
            }
        } catch (IllegalArgumentException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }

        return ids;
    }

    // ---------------------------------------

    /**
     * @see com.artemis.Osmo4.GpacCallback#displayMessage(java.lang.String, java.lang.String)
     */
    @Override
    public void displayMessage(String message, String title) {
        final String fullMsg = title + '\n' + message;
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                Toast toast = Toast.makeText(Osmo4.this, fullMsg, Toast.LENGTH_SHORT);
                toast.show();
            }
        });

    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#log(int, int, java.lang.String, java.lang.Object[])
     */
    @Override
    public void log(int level, int module, String message, Object... arguments) {
    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#onProgress(java.lang.String, int, int)
     */
    @Override
    public void onProgress(String msg, int done, int total) {
        displayMessage(msg + " " + done + "/" + total, "Progress");
    }
}
