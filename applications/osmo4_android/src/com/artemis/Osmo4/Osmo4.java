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
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
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
import android.widget.TextView;
import android.widget.TextView.BufferType;

/**
 * The main Osmo4 activity, used to launch everything
 * 
 * @version $Revision$
 * 
 */
public class Osmo4 extends Activity {

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
        TextView text = new TextView(this);
        text.setText("Loading...", BufferType.NORMAL); //$NON-NLS-1$
        setContentView(text);
        loadAllModules();

        mGLView = new Osmo4GLSurfaceView(this);
        mGLView.setFocusable(true);
        mGLView.setFocusableInTouchMode(true);
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wl = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, LOG_OSMO_TAG);
        setContentView(mGLView);
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

    // ---------------------------------------
    protected void OpenFileDialog() {
        Intent intent = new Intent("org.openintents.action.PICK_FILE"); //$NON-NLS-1$
        //Intent intent = new Intent("org.openintents.action.PICK_FILE"); //$NON-NLS-1$
        intent.setData(Uri.fromFile(new File(Osmo4Renderer.GPAC_CFG_DIR)));
        intent.putExtra("org.openintents.extra.TITLE", "Please select a file"); //$NON-NLS-1$//$NON-NLS-2$
        intent.putExtra("browser_filter_extension_whitelist", OSMO_REGISTERED_FILE_EXTENSIONS); //$NON-NLS-1$

        try {
            startActivityForResult(intent, 1);
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
            case R.id.open_file:
                // newGame();
                OpenFileDialog();
                return true;
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
            if (finalFile.exists() && finalFile.canRead())
                continue;
            try {
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

    // ---------------------------------------
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
}
