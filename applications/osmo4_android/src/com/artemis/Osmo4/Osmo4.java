/*
 *  Osmo on Android
 *  Aug/2010
 *  NGO Van Luyen
 * test test
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
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

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

    // ---------------------------------------
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        loadAllModules();

        mGLView = new Osmo4GLSurfaceView(this);
        mGLView.setFocusable(true);
        mGLView.setFocusableInTouchMode(true);
        setContentView(mGLView);

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
        Intent intent = new Intent(Intent.ACTION_PICK);
        //Intent intent = new Intent("org.openintents.action.PICK_FILE"); //$NON-NLS-1$
        intent.setData(Uri.fromFile(new File(Osmo4Renderer.GPAC_CFG_DIR)));
        intent.putExtra("org.openintents.extra.TITLE", "Please select a file"); //$NON-NLS-1$//$NON-NLS-2$
        intent.putExtra("browser_filter_extension_whitelist", OSMO_REGISTERED_FILE_EXTENSIONS); //$NON-NLS-1$

        startActivityForResult(intent, 0);

    }

    // ---------------------------------------
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent intent) {
        if (requestCode == 0) {
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
        super.onDestroy();
        GpacObject.gpacfree();
    }

    // ---------------------------------------
    private void loadAllModules() {
        byte buffer[] = new byte[1024];
        int[] ids = getAllRawResources();
        for (int i = 0; i < ids.length; i++) {
            OutputStream fos = null;
            InputStream ins = null;
            String fn = Osmo4Renderer.GPAC_MODULES_DIR + m_modules_list[i] + ".so"; //$NON-NLS-1$
            try {
                int read;
                ins = new BufferedInputStream(getResources().openRawResource(ids[i]));
                fos = new BufferedOutputStream(new FileOutputStream(fn));
                while (0 < (read = ins.read(buffer))) {
                    fos.write(buffer, 0, read);
                }
                ins.close();
                ins = null;
                fos.close();
                fos = null;
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
