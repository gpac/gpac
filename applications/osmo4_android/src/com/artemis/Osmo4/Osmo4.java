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
import java.text.MessageFormat;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Date;
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
import android.app.Dialog;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.DialogInterface.OnClickListener;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.text.InputType;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Toast;
import com.artemis.Osmo4.extra.FileChooserActivity;
import com.artemis.Osmo4.logs.GpacLogger;

/**
 * The main Osmo4 activity, used to launch everything
 * 
 * @version $Revision$
 * 
 */
public class Osmo4 extends Activity implements GpacCallback {

    private String[] m_modules_list;

    private final static String CFG_STARTUP_CATEGORY = "General"; //$NON-NLS-1$

    private final static String CFG_STARTUP_NAME = "StartupFile"; //$NON-NLS-1$

    private boolean keyboardIsVisible = false;

    private final static int DEFAULT_BUFFER_SIZE = 8192;

    private final String DEFAULT_OPEN_URL = Osmo4Renderer.GPAC_CFG_DIR + "gui/gui.bt"; //$NON-NLS-1$

    /**
     * Activity request ID for picking a file from local filesystem
     */
    public final static int PICK_FILE_REQUEST = 1;

    private final static String LOG_OSMO_TAG = "Osmo4"; //$NON-NLS-1$

    private final static String OK_BUTTON = "OK"; //$NON-NLS-1$

    /**
     * List of all extensions recognized by Osmo
     */
    public final static String OSMO_REGISTERED_FILE_EXTENSIONS = "*.mp4,*.bt,*.xmt,*.xml,*.ts,*.svg,*.mp3,*.m3u8,*.mpg,*.aac,*.m4a,*.jpg,*.png"; //$NON-NLS-1$

    private PowerManager.WakeLock wl = null;

    private Osmo4GLSurfaceView mGLView;

    private final GpacLogger logger = new GpacLogger();

    private ProgressDialog startupProgress;

    // ---------------------------------------
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        logger.onCreate();
        requestWindowFeature(Window.FEATURE_PROGRESS);
        // requestWindowFeature(Window.FEATURE_CUSTOM_TITLE);
        requestWindowFeature(Window.FEATURE_INDETERMINATE_PROGRESS);

        final String toOpen;
        if (Intent.ACTION_VIEW.equals(getIntent().getAction())) {
            Uri uri = getIntent().getData();
            if (uri != null) {
                synchronized (this) {
                    toOpen = uri.toString();
                }
            } else
                toOpen = null;
        } else
            toOpen = null; // "http://vizionr.fr:8000/fun.ts";
        if (startupProgress == null) {
            startupProgress = new ProgressDialog(this);
            startupProgress.setCancelable(false);
        }
        startupProgress.setMessage(getResources().getText(R.string.osmoLoading));
        startupProgress.setTitle(null);
        startupProgress.setIndeterminate(true);
        startupProgress.show();
        if (mGLView != null) {
            setContentView(mGLView);
            if (toOpen != null)
                openURLasync(toOpen);
            // Ok, it means activity has already been started
            return;
        }
        mGLView = new Osmo4GLSurfaceView(Osmo4.this);
        service.submit(new Runnable() {

            @Override
            public void run() {
                loadAllModules();
                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {
                        startupProgress.setMessage(getResources().getText(R.string.gpacLoading));
                        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                        WakeLock wl = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, LOG_OSMO_TAG);
                        if (wl != null)
                            wl.acquire();
                        synchronized (Osmo4.this) {
                            Osmo4.this.wl = wl;
                        }

                        Osmo4Renderer renderer = new Osmo4Renderer(Osmo4.this, toOpen);
                        mGLView.setRenderer(renderer);
                        setContentView(mGLView);
                    }
                });

            }
        });

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
                                                                      DEFAULT_ENCODING), DEFAULT_BUFFER_SIZE);

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
        textView.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);

        builder.setMessage("Please enter an URL to connect to...") //$NON-NLS-1$
               .setCancelable(true)
               .setPositiveButton("Open URL", new DialogInterface.OnClickListener() { //$NON-NLS-1$

                                      public void onClick(DialogInterface dialog, int id) {
                                          dialog.cancel();
                                          final String newURL = textView.getText().toString();
                                          openURLasync(newURL);
                                          service.execute(new Runnable() {

                                              @Override
                                              public void run() {
                                                  addAllRecentURLs(Collections.singleton(newURL));
                                                  File tmp = new File(getRecentURLsFile() + ".tmp"); //$NON-NLS-1$
                                                  BufferedWriter w = null;
                                                  try {
                                                      w = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(tmp),
                                                                                                    DEFAULT_ENCODING),
                                                                             DEFAULT_BUFFER_SIZE);
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
        String title = getResources().getString(R.string.pleaseSelectAFile);
        Uri uriDefaultDir = Uri.fromFile(new File(Osmo4Renderer.GPAC_CFG_DIR));
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_PICK);
        // Files and directories
        intent.setDataAndType(uriDefaultDir, "vnd.android.cursor.dir/lysesoft.andexplorer.file"); //$NON-NLS-1$
        // Optional filtering on file extension.
        intent.putExtra("browser_filter_extension_whitelist", OSMO_REGISTERED_FILE_EXTENSIONS); //$NON-NLS-1$
        // Title
        intent.putExtra("explorer_title", title); //$NON-NLS-1$

        try {
            startActivityForResult(intent, PICK_FILE_REQUEST);
            return true;
        } catch (ActivityNotFoundException e) {
            // OK, lets try with another one... (it includes out bundled one)
            intent = new Intent("org.openintents.action.PICK_FILE"); //$NON-NLS-1$
            intent.setData(uriDefaultDir);
            intent.putExtra(FileChooserActivity.TITLE_PARAMETER, title);
            intent.putExtra("browser_filter_extension_whitelist", OSMO_REGISTERED_FILE_EXTENSIONS); //$NON-NLS-1$
            try {
                startActivityForResult(intent, PICK_FILE_REQUEST);
                return true;
            } catch (ActivityNotFoundException ex) {
                // Not found neither... We build our own dialog to display error
                // Note that this should happen only if we did not embed out own intent
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
    }

    // ---------------------------------------
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        if (requestCode == PICK_FILE_REQUEST) {
            if (resultCode == RESULT_OK) {
                Uri uri = intent.getData();
                if (uri != null) {
                    String url = uri.toString();
                    String file = "file://"; //$NON-NLS-1$
                    if (url.startsWith(file)) {
                        url = uri.getPath();
                    }
                    Log.i(LOG_OSMO_TAG, "Requesting opening local file " + url); //$NON-NLS-1$
                    openURLasync(url);
                }
            }
        }
    }

    private String currentURL;

    private void openURLasync(final String url) {
        service.execute(new Runnable() {

            @Override
            public void run() {
                mGLView.connect(url);
                currentURL = url;
                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {
                        if (DEFAULT_OPEN_URL.equals(url))
                            setTitle(LOG_OSMO_TAG + " - Home"); //$NON-NLS-1$
                        else
                            setTitle(LOG_OSMO_TAG + " - " + url); //$NON-NLS-1$
                    }
                });
            }
        });
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
            case R.id.cleanCache:
                return cleanCache();
            case R.id.showVirtualKeyboard:
                showKeyboard(true);
                return true;
            case R.id.setAsStartupFile: {
                AlertDialog.Builder b = new AlertDialog.Builder(this);
                b.setTitle(R.string.setAsStartupFileTitle);
                if (currentURL != null) {
                    b.setMessage(getResources().getString(R.string.setAsStartupFileMessage, currentURL));
                    b.setPositiveButton(R.string.setAsStartupFileYes, new OnClickListener() {

                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                            mGLView.setGpacPreference(CFG_STARTUP_CATEGORY, CFG_STARTUP_NAME, currentURL);
                        }
                    });
                } else {
                    b.setMessage(getResources().getString(R.string.setAsStartupFileMessageNoURL));
                }
                b.setNegativeButton(R.string.setAsStartupFileNo, new OnClickListener() {

                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });
                b.setNeutralButton(R.string.setAsStartupFileNull, new OnClickListener() {

                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                        mGLView.setGpacPreference(CFG_STARTUP_CATEGORY, CFG_STARTUP_NAME, null);
                    }
                });
                b.show();
                return true;
            }
            case R.id.about: {
                Dialog d = new Dialog(this);
                d.setTitle(R.string.aboutTitle);
                d.setCancelable(true);
                d.setContentView(R.layout.about_dialog);
                d.show();
            }
                return true;
            case R.id.quit:
                this.finish();
                mGLView.destroy();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    protected boolean cleanCache() {
        final CharSequence oldTitle = getTitle();
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                Osmo4.this.setTitle(Osmo4.this.getResources().getText(R.string.cleaningCache));
            }
        });
        service.submit(new Runnable() {

            @Override
            public void run() {
                File dir = new File(Osmo4Renderer.GPAC_CACHE_DIR);
                if (!dir.exists() || !dir.canRead() || !dir.canWrite() || !dir.isDirectory()) {
                    return;
                }
                File files[] = dir.listFiles();
                int i = 0;
                for (File f : files) {
                    if (f.isFile())
                        if (!f.delete()) {
                            Log.w(LOG_OSMO_TAG, "Failed to delete file " + f); //$NON-NLS-1$
                        } else {
                            Log.v(LOG_OSMO_TAG, f + " has been deleted"); //$NON-NLS-1$
                        }
                    final int percent = (++i) * 100 / files.length;
                    runOnUiThread(new Runnable() {

                        @Override
                        public void run() {
                            Osmo4.this.setProgress(100 * percent);

                        }
                    });
                }
                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {
                        Osmo4.this.setTitle(oldTitle);
                    }
                });
            }
        });
        return true;
    }

    // ---------------------------------------
    @Override
    protected void onDestroy() {
        service.shutdown();
        logger.onDestroy();
        synchronized (this) {
            if (wl != null)
                wl.release();
        }
        Log.d(LOG_OSMO_TAG, "Disconnecting instance..."); //$NON-NLS-1$
        mGLView.disconnect();
        Log.d(LOG_OSMO_TAG, "Destroying GPAC instance..."); //$NON-NLS-1$
        mGLView.destroy();
        super.onDestroy();
    }

    // ---------------------------------------
    private void loadAllModules() {
        Log.i(LOG_OSMO_TAG, "Start loading all modules..."); //$NON-NLS-1$
        long start = System.currentTimeMillis();
        byte buffer[] = new byte[1024];
        int[] ids = getAllRawResources();
        String currentRevision = "$Revision$"; //$NON-NLS-1$
        File revisionFile = new File(Osmo4Renderer.GPAC_CACHE_DIR, "lastRev.txt"); //$NON-NLS-1$
        boolean fastStartup = false;
        // We check if we already copied all the modules once without error...
        if (revisionFile.exists() && revisionFile.canRead()) {
            BufferedReader r = null;
            try {
                r = new BufferedReader(new InputStreamReader(new FileInputStream(revisionFile), DEFAULT_ENCODING),
                                       DEFAULT_BUFFER_SIZE);
                String rev = r.readLine();
                if (currentRevision.equals(rev)) {
                    fastStartup = true;
                }
            } catch (IOException ignored) {
            } finally {
                // Exception or not, always close the stream...
                if (r != null)
                    try {
                        r.close();
                    } catch (IOException ignored) {
                    }
            }
        }
        boolean noErrors = true;
        final StringBuilder errorsMsg = new StringBuilder();
        for (int i = 0; i < ids.length; i++) {
            OutputStream fos = null;
            InputStream ins = null;
            String fn = Osmo4Renderer.GPAC_MODULES_DIR + m_modules_list[i] + ".so"; //$NON-NLS-1$
            File finalFile = new File(fn);
            // If file has already been copied, not need to do it again
            if (fastStartup && finalFile.exists() && finalFile.canRead()) {
                Log.i(LOG_OSMO_TAG, "Skipping " + finalFile); //$NON-NLS-1$
                continue;
            }
            try {
                final String msg = getResources().getString(R.string.copying_native_libs,
                                                            finalFile.getName(),
                                                            finalFile.getParent());
                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {
                        startupProgress.setMessage(msg);
                    }
                });
                Log.i(LOG_OSMO_TAG, "Copying resource " + ids[i] + " to " //$NON-NLS-1$//$NON-NLS-2$
                                    + finalFile.getAbsolutePath());
                File tmpFile = new File(fn + ".tmp"); //$NON-NLS-1$
                int read;
                ins = new BufferedInputStream(getResources().openRawResource(ids[i]), DEFAULT_BUFFER_SIZE);
                fos = new BufferedOutputStream(new FileOutputStream(tmpFile), DEFAULT_BUFFER_SIZE);
                while (0 < (read = ins.read(buffer))) {
                    fos.write(buffer, 0, read);
                }
                ins.close();
                ins = null;
                fos.close();
                fos = null;
                if (!tmpFile.renameTo(finalFile)) {
                    if (finalFile.exists() && finalFile.delete() && !tmpFile.renameTo(finalFile))
                        Log.e(LOG_OSMO_TAG, "Failed to rename " + tmpFile.getAbsolutePath() + " to " //$NON-NLS-1$//$NON-NLS-2$
                                            + finalFile.getAbsolutePath());
                }
                final int percent = i * 10000 / ids.length;
                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {
                        startupProgress.setIndeterminate(false);
                        startupProgress.setProgress(percent);
                    }
                });
            } catch (IOException e) {
                noErrors = false;
                String msg = "IOException for resource : " + ids[i]; //$NON-NLS-1$
                errorsMsg.append(msg).append('\n').append(finalFile.getAbsolutePath()).append('\n');
                errorsMsg.append(e.getLocalizedMessage());
                Log.e(LOG_OSMO_TAG, msg, e);
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
        // If no error during copy, fast startup will be enabled for next time
        if (noErrors && !fastStartup) {
            BufferedWriter w = null;
            try {
                w = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(revisionFile), DEFAULT_ENCODING),
                                       DEFAULT_BUFFER_SIZE);
                w.write(currentRevision);
                w.write('\n');
                // We add the date as second line to ease debug in case of problem
                w.write(String.valueOf(new Date()));
            } catch (IOException ignored) {
            } finally {
                if (w != null)
                    try {
                        w.close();
                    } catch (IOException ignored) {
                    }
            }
        } else {
            if (!noErrors)
                displayMessage(errorsMsg.toString(), "Errors while copying modules !", GF_Err.GF_IO_ERR.value); //$NON-NLS-1$
        }
        Log.i(LOG_OSMO_TAG, "Done loading all modules, took " + (System.currentTimeMillis() - start) + "ms."); //$NON-NLS-1$ //$NON-NLS-2$
        startupProgress.setProgress(100);
        startupProgress.setIndeterminate(true);
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                setTitle(LOG_OSMO_TAG);
            }
        });
    }

    private int[] getAllRawResources() throws RuntimeException {
        R.raw r = new R.raw();

        java.lang.reflect.Field fields[] = R.raw.class.getDeclaredFields();
        final int ids[] = new int[fields.length];
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

    private String lastDisplayedMessage;

    private void displayPopup(CharSequence message, CharSequence title) {
        final String fullMsg = MessageFormat.format(String.valueOf(getResources().getText(R.string.displayPopupFormat)),
                                                    title,
                                                    message);
        synchronized (this) {
            if (fullMsg.equals(lastDisplayedMessage))
                return;
            lastDisplayedMessage = fullMsg;
        }
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                Toast toast = Toast.makeText(Osmo4.this, fullMsg, Toast.LENGTH_SHORT);
                toast.show();
            }
        });

    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#displayMessage(String, String, int)
     */
    @Override
    public void displayMessage(final String message, final String title, final int status) {
        if (status == GF_Err.GF_OK.value)
            displayPopup(message, title);
        else {
            runOnUiThread(new Runnable() {

                @Override
                public void run() {
                    StringBuilder sb = new StringBuilder();
                    sb.append(GF_Err.getError(status));
                    sb.append(' ');
                    sb.append(title);
                    AlertDialog.Builder builder = new AlertDialog.Builder(Osmo4.this);
                    builder.setTitle(sb.toString());
                    sb.append('\n');
                    sb.append(message);
                    builder.setMessage(sb.toString());
                    builder.setCancelable(true);
                    builder.setPositiveButton(OK_BUTTON, new OnClickListener() {

                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.cancel();
                        }
                    });
                    try {
                        builder.create().show();
                    } catch (WindowManager.BadTokenException e) {
                        // May happen when we close the window and there are still somes messages
                        Log.e(LOG_OSMO_TAG, "Failed to display Message " + sb.toString(), e); //$NON-NLS-1$
                    }
                }
            });
        }
    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#onLog(int, int, String)
     */
    @Override
    public void onLog(int level, int module, String message) {
        logger.onLog(level, module, message);
    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#onProgress(java.lang.String, int, int)
     */
    @Override
    public void onProgress(final String msg, final int done, final int total) {
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                // GPAC sometimes return total = 0
                if (total < 1) {
                    setProgressBarIndeterminate(true);
                } else {
                    int progress = done * 10000 / (total < 1 ? 1 : total);
                    if (progress > 9900)
                        progress = 10000;
                    setProgressBarIndeterminate(false);
                    setProgress(progress);
                }
            }
        });
    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#onGPACReady()
     */
    @Override
    public void onGPACReady() {
        startupProgress.dismiss();
        Log.i(LOG_OSMO_TAG, "GPAC is ready"); //$NON-NLS-1$
    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#onGPACError(java.lang.Throwable)
     */
    @Override
    public void onGPACError(final Throwable e) {
        startupProgress.dismiss();
        Log.e(LOG_OSMO_TAG, "GPAC Error", e); //$NON-NLS-1$
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                StringBuilder sb = new StringBuilder();
                sb.append("Failed to init GPAC due to "); //$NON-NLS-1$
                sb.append(e.getClass().getSimpleName());
                AlertDialog.Builder builder = new AlertDialog.Builder(Osmo4.this);
                builder.setTitle(sb.toString());
                sb.append('\n');
                sb.append("Description: "); //$NON-NLS-1$
                sb.append(e.getLocalizedMessage());
                sb.append('\n');
                sb.append("Revision: $Revision$"); //$NON-NLS-1$
                builder.setMessage(sb.toString());
                builder.setCancelable(true);
                builder.setPositiveButton(OK_BUTTON, new OnClickListener() {

                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.cancel();
                    }
                });
                builder.create().show();
            }
        });
    }

    @Override
    protected void onPause() {
        super.onPause();
        mGLView.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mGLView.onResume();
    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#setCaption(java.lang.String)
     */
    @Override
    public void setCaption(final String newCaption) {
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                setTitle(newCaption);
            }
        });
    }

    /**
     * @see com.artemis.Osmo4.GpacCallback#showKeyboard(boolean)
     */
    @Override
    public void showKeyboard(boolean showKeyboard) {
        if (keyboardIsVisible == showKeyboard == true)
            return;
        InputMethodManager mgr = ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE));
        this.keyboardIsVisible = showKeyboard;
        if (showKeyboard)
            mgr.showSoftInput(mGLView, 0);
        else
            mgr.hideSoftInputFromInputMethod(mGLView.getWindowToken(), 0);

    }
}
