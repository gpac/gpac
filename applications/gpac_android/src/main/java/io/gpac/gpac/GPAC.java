/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, NGO Van Luyen, Ivica Arsov
 *			Copyright (c) Telecom Paris 2010-2022
 *			All rights reserved
 *
 *  This file is part of GPAC / JavaScript libgpac Core bindings
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

package io.gpac.gpac;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.HashMap;
import java.util.Set;
import java.util.Map;
import java.util.Date;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.hardware.SensorManager;
import android.location.LocationManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.os.Vibrator;
import android.text.InputType;
import android.util.Log;
import android.view.View;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.LinearLayout;
import android.widget.Toast;
import android.content.res.AssetManager;
import android.os.Environment;
import android.Manifest;
import android.support.v4.app.ActivityCompat;
import android.os.Build;
import android.content.pm.PackageManager;

import android.os.Handler;
import android.os.Message;
import android.view.GestureDetector;
import android.view.MotionEvent;

import io.gpac.gpac.GPACGLSurfaceView;
import io.gpac.gpac.R;
import io.gpac.gpac.extra.FileChooserActivity;
import io.gpac.gpac.extra.URLAuthenticate;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import android.content.pm.PackageManager.NameNotFoundException;

import com.lge.real3d.Real3D;
import com.lge.real3d.Real3DInfo;

import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.text.method.ScrollingMovementMethod;
import android.view.Gravity;



/**
 * The main GPAC activity, used to launch everything
 *
 * @version $Revision$
 *
 */
public class GPAC extends Activity {
    private final static String LOG_TAG = "GPAC";
    private final static int DEFAULT_BUFFER_SIZE = 8192;

    // Activity request ID for picking a file from local file system
    public final static int PICK_FILE_REQUEST = 1;
    public final static int AUTH_REQUEST = 2;

    public final static int SERVICE_TYPE_URL = 0;
    public final static int SERVICE_TYPE_GPAC = 1;
    public final static int SERVICE_TYPE_MP4BOX = 2;

    // List of all extensions recognized by GPAC - we should fetch this from libgpac
    public final static String REGISTERED_FILE_EXTENSIONS = "*.mp4,*.bt,*.xmt,*.xml,*.ts,*.svg,*.mp3,*.m3u8,*.mpg,*.aac,*.m4a,*.jpg,*.png,*.wrl,*.mpd,*.m3u8,*.mkv,*.mov,*.ac3,*.pls,*.m3u";

    //underlying C object
    private long gpac_jni_handle;

	//orientation and location sensors
    private SensorServices sensors;

	//Real3D rendering
    private Real3D real3d_dev;

    private static boolean keyboard_visible = false;
    private static boolean ui_visible;

    private Thread gpac_render_thread;

    private String gpac_data_dir;
    private String external_storage_dir;
    private String gpac_install_logs;

    private String init_url = null;
    private PowerManager.WakeLock wl = null;

    private GPACGLSurfaceView gpac_gl_view;

    private ProgressDialog startupProgress;
    private String currentURL;

    private int last_orientation = 0;
	private boolean has_display_cutout = false;

    private String last_displayed_message;
    //logs view for command line
	private TextView gpac_log_view;
	private boolean is_gui_mode = true;
	private boolean in_session = false;
	private boolean session_aborted = false;

	private static final int WRITE_PERM_REQUEST = 112;

    // ---------------------------------------
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		//fetch init URL
		init_url = null;
        if (Intent.ACTION_VIEW.equals(getIntent().getAction())) {
            Uri uri = getIntent().getData();
            if (uri != null) {
                synchronized (this) {
                    init_url = uri.toString();
                }
            }
		}
        if (startupProgress == null) {
            startupProgress = new ProgressDialog(this);
            startupProgress.setCancelable(false);
			startupProgress.setMessage(getResources().getText(R.string.gpacLoading));
			startupProgress.setTitle(R.string.gpacLoading);
        }
        startupProgress.show();

		//reopen of activity ("open with GPAC" called), post open and return
        if (gpac_gl_view != null) {
            setContentView(R.layout.main);
            if (init_url != null)
                openURLasync(init_url);

            return;
        }

		if (Build.VERSION.SDK_INT >= 28) {
			getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
		}

		//locate data dirs and external storage
		Context context = getApplicationContext();
        try {
            if (context == null || context.getPackageManager() == null) {
                gpac_data_dir = Environment.getDataDirectory() + "/data/io.gpac.gpac";
                Log.e("GPAC", "Cannot get context or PackageManager, using default directory=" + gpac_data_dir);
            } else
                gpac_data_dir = context.getPackageManager().getApplicationInfo(context.getPackageName(), 0).dataDir;
        } catch (NameNotFoundException e) {
            Log.e("GPAC", "This is bad, we cannot find ourself : " + context.getPackageName(), e); //$NON-NLS-1$
            throw new RuntimeException("Cannot find package " + context.getPackageName(), e); //$NON-NLS-1$
        }

        external_storage_dir = Environment.getExternalStorageDirectory().getAbsolutePath();
        gpac_install_logs = external_storage_dir + "/gpac_android_launch_logs.txt";

		//extract assets
		checkAssetsCopy();

		//create layout + GLSurface
        setContentView(R.layout.main);


		if (Build.VERSION.SDK_INT >= 23) {
		    String[] PERMISSIONS = {android.Manifest.permission.WRITE_EXTERNAL_STORAGE};
		    if (!hasPermissions(context, PERMISSIONS)) {
		        ActivityCompat.requestPermissions((Activity) context, PERMISSIONS, WRITE_PERM_REQUEST );
		    }
		}

		final View contentView = (LinearLayout)findViewById(R.id.surface_gl);
		View decor_view = getWindow().getDecorView();
		decor_view.setOnSystemUiVisibilityChangeListener(
			new View.OnSystemUiVisibilityChangeListener() {
				@Override
				public void onSystemUiVisibilityChange(int visibility) {
					if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
						ui_visible = true;
					} else {
						ui_visible = false;
					}
				}
			}
		);

		contentView.setClickable(true);
        final GestureDetector clickDetector = new GestureDetector(this,
			new GestureDetector.SimpleOnGestureListener() {
				@Override
				public boolean onSingleTapUp(MotionEvent e) {
					if (ui_visible) {
						if (GPAC.this.keyboard_visible)
							((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE)).hideSoftInputFromWindow(gpac_gl_view.getWindowToken(), 0);
						hideSystemUI();
					}
					return true;
				}
			}
		);
        contentView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, MotionEvent motionEvent) {
                return clickDetector.onTouchEvent(motionEvent);
            }
        });

		showSystemUI();

        gpac_log_view = new TextView(this);
        gpac_log_view.setTextSize(16);
		gpac_log_view.setGravity(Gravity.BOTTOM);
        gpac_log_view.setMovementMethod(new ScrollingMovementMethod());
        gpac_log_view.setTextIsSelectable(true);

		gpac_gl_view = new GPACGLSurfaceView(this);

		final LinearLayout view_layout = (LinearLayout)findViewById(R.id.surface_gl);

		//init sensor manager
        sensors = new SensorServices(this);

		//real3D renderering
		boolean real3d_lib_loaded = false;
        try {
			Class c = Class.forName("com.lge.real3d.Real3D");
			final String LGE_3D_DISPLAY = "lge.hardware.real3d.barrier.landscape";
			if (this.getPackageManager().hasSystemFeature(LGE_3D_DISPLAY)) {
				real3d_dev = new Real3D(gpac_gl_view.getHolder());
				real3d_dev.setReal3DInfo(new Real3DInfo(true, Real3D.REAL3D_TYPE_SS, Real3D.REAL3D_ORDER_LR));
			}
        } catch (ClassNotFoundException e) { }

        service.submit(new Runnable() {
            @Override
            public void run() {
                // loadAllModules();
                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {
                        startupProgress.setIndeterminate(true);
                        startupProgress.setMessage(getResources().getText(R.string.gpacLoading));
                        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                        WakeLock wl = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, LOG_TAG);
                        if (wl != null)
                            wl.acquire();
                        synchronized (GPAC.this) {
                            GPAC.this.wl = wl;
                        }

						//we need a renderer to catch resize and draw
                        GPACRenderer renderer = new GPACRenderer(GPAC.this);
                        gpac_gl_view.setRenderer(renderer);

                        view_layout.addView(gpac_gl_view);
                    }
                });
            }
        });
		//done
	}

	private static boolean hasPermissions(Context context, String[] permissions) {
	    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && context != null && permissions != null) {
	        for (String permission : permissions) {
	            if (ActivityCompat.checkSelfPermission(context, permission) != PackageManager.PERMISSION_GRANTED) {
	                return false;
	            }
	        }
	    }
	    return true;
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
	    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
	    switch (requestCode) {
	        case WRITE_PERM_REQUEST: {
	            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
	            } else {
	                displayPopup("The app was not allowed to write in your storage", "Error Requesting file write permissions");
	            }
	        }
	    }
	}
    /*
     * Copy /share elements
     *
     * Reference: http://stackoverflow.com/questions/4447477/android-how-to-copy-files-from-assets-folder-to-sdcard
    */
    private final static String ROOT_ASSET_DIR = "share";

    private void checkAssetsCopy() {
        //copy APK/assets in APP_DATA/share - we could optimize by using asset manager in NDK
        File share_dir = new File( gpac_data_dir + "/" + ROOT_ASSET_DIR);
        //already copied
        if (share_dir.isDirectory())
			return;

		// there was a file which have the same name with GUI dir, delete it
		if (share_dir.exists()) {
			if (!share_dir.delete())
				Log.e(LOG_TAG, "Failed to delete " + share_dir);
		}
		if (!share_dir.mkdirs())
			Log.e(LOG_TAG, "Failed to create directory " + share_dir);

		StringBuilder sb = new StringBuilder();
		HashMap<String, Throwable> exceptions = new HashMap<String, Throwable>();
		AssetManager assetManager = getAssets();
		String[] list = null;
		Log.d(LOG_TAG, "Copy assets of " + ROOT_ASSET_DIR);
		try {
			list = assetManager.list(ROOT_ASSET_DIR);
		} catch (IOException e) {
			Log.e(LOG_TAG, "Failed to get asset file list.", e);
			exceptions.put("Failed to get asset file list", e);
		}
		for(String path : list) {
			try {
			  copyFileOrDir(ROOT_ASSET_DIR, path);
			} catch(IOException e) {
				Log.e(LOG_TAG, "Failed to copy: " + path, e);
				exceptions.put("Failed to copy " + path, e);
			}
		}

		if (!exceptions.isEmpty()) {
			try {
				PrintStream out = new PrintStream(gpac_install_logs, "UTF-8");
				sb.append("*** Exceptions:\n");
				for (Map.Entry<String, Throwable> ex : exceptions.entrySet()) {
					sb.append(ex.getKey()).append(": ")
						.append(ex.getValue().getLocalizedMessage())
						.append('(')
						.append(ex.getValue().getClass())
						.append(")\n");
				}
				out.println(sb.toString());
				out.flush();
				out.close();
			} catch (Exception e) {
				Log.e(LOG_TAG, "Failed to output debug info to debug file", e); //$NON-NLS-1$
			}
		}
	}

	private void copyFileOrDir(String root, String path) throws IOException {
		AssetManager assetManager = getAssets();
		String assets[] = null;
		assets = assetManager.list(root + "/" + path);
		if (assets.length == 0) {
		   copyFile(root, path);
		} else {
			String fullPath = null;
			fullPath = gpac_data_dir + "/" + ROOT_ASSET_DIR + "/" + path;

			File dir = new File(fullPath);
			if (!dir.exists())
				dir.mkdir();
			for (int i = 0; i < assets.length; ++i) {
				copyFileOrDir(root, path + "/" + assets[i]);
			}
		}
	}

	private void copyFile(String root, String filename) throws IOException {
		if (root.equals(ROOT_ASSET_DIR)){
			if ((new File(gpac_data_dir + "/" + ROOT_ASSET_DIR + "/" + filename).exists()))
				return;
		}

		AssetManager assetManager = getAssets();
		InputStream in = null;
		OutputStream out = null;
		in = assetManager.open(root + "/" + filename);

		String newFileName = null;
		newFileName = gpac_data_dir + "/" + ROOT_ASSET_DIR + "/" + filename;
		out = new FileOutputStream(newFileName);

		byte[] buffer = new byte[1024];
		int read;
		while ((read = in.read(buffer)) != -1) {
			out.write(buffer, 0, read);
		}
		in.close();
		in = null;
		out.flush();
		out.close();
		out = null;
	}

	//if system UI or keyboard is shown, don't forward to GPAC
    public static boolean noEventForward() {
//		return (ui_visible || keyboard_visible);
		return false;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
	public boolean onPrepareOptionsMenu(Menu menu) {
		MenuItem item = menu.findItem(R.id.runGPAC);
		item.setTitle(in_session ? R.string.abortSession : R.string.runGPAC);

		item = menu.findItem(R.id.reloadLast);
		item.setEnabled((currentURL==null) ? false : true);
		if ((currentURL!=null) && (currentURL.startsWith("gpac ") || currentURL.startsWith("MP4Box "))) {
			item.setTitle(R.string.reloadSession);
		} else {
			item.setTitle(R.string.reloadLast);
		}
		return true;
	}

    private String getRecentURLsFile(int service_type) {
		String fname = "/GPAC/history_";
		if (service_type==SERVICE_TYPE_MP4BOX) fname += "mp4box";
		else if (service_type==SERVICE_TYPE_GPAC) fname += "gpac";
		else fname += "urls";
		//if we have a /GPAC profile folder, use it for recent URLs, otherwise use global one (avoids creating a GPAC profile folder)
        File prof_dir = new File(external_storage_dir + "/GPAC");
		if (prof_dir.exists()) {
			return external_storage_dir + fname;
		}
		return gpac_data_dir + fname;
    }

    private boolean openURL(final int service_type) {
        Future<String[]> res = service.submit(new Callable<String[]>() {

            @Override
            public String[] call() throws Exception {
                BufferedReader reader = null;
                try {
                    reader = new BufferedReader(new InputStreamReader(new FileInputStream(getRecentURLsFile(service_type)),
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
        builder.setMessage( (service_type==SERVICE_TYPE_MP4BOX) ? R.string.selectMP4Box : ((service_type==SERVICE_TYPE_GPAC) ? R.string.selectGPAC : R.string.selectURL) )
               .setCancelable(true)
               .setPositiveButton((service_type!=SERVICE_TYPE_URL) ? R.string.open_url_ok : R.string.open_cmd_ok, new DialogInterface.OnClickListener() {

				@Override
				public void onClick(DialogInterface dialog, int id) {
					dialog.cancel();
					String the_url = textView.getText().toString();
					if ((service_type==SERVICE_TYPE_MP4BOX) && !the_url.toLowerCase().startsWith("mp4box ")) {
						the_url = "MP4Box " + the_url;
					}
					else if ((service_type==SERVICE_TYPE_GPAC) && !the_url.toLowerCase().startsWith("gpac ")) {
						the_url = "gpac " + the_url;
					}
					final String newURL = the_url;
					openURLasync(newURL);
					service.execute(new Runnable() {

                           @Override
                           public void run() {
                               addAllRecentURLs(Collections.singleton(newURL));
                               File tmp = new File(getRecentURLsFile(service_type) + ".tmp"); //$NON-NLS-1$
                               BufferedWriter w = null;
                               try {
                                   w = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(tmp),
                                                                                 DEFAULT_ENCODING), DEFAULT_BUFFER_SIZE);
                                   Collection<String> toWrite = getAllRecentURLs();
                                   for (String s : toWrite) {
                                       w.write(s);
                                       w.write("\n"); //$NON-NLS-1$
                                   }
                                   w.close();
                                   w = null;
                                   if (tmp.renameTo(new File(getRecentURLsFile(service_type))))
                                       Log.e(LOG_TAG, "Failed to rename " + tmp + " to " + getRecentURLsFile(service_type)); //$NON-NLS-1$//$NON-NLS-2$

                               } catch (IOException e) {
                                   Log.e(LOG_TAG, "Failed to write recent URLs to " + tmp, e); //$NON-NLS-1$
                                   try {
                                       if (w != null)
                                           w.close();
                                   } catch (IOException ex) {
                                       Log.e(LOG_TAG, "Failed to close stream " + tmp, ex); //$NON-NLS-1$
                                   }
                               }

                           }
                       });
                   }
               })
               .setNegativeButton(R.string.cancel, null);

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
            Log.e(LOG_TAG, "Error while parsing recent URLs", e); //$NON-NLS-1$
        } catch (TimeoutException e) {
            Log.e(LOG_TAG, "It took too long to parse recent URLs", e); //$NON-NLS-1$
        } catch (InterruptedException e) {
            Log.e(LOG_TAG, "Interrupted while parsing recent URLs", e); //$NON-NLS-1$
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
        String title = getResources().getString(R.string.selectFile);
        Uri uriDefaultDir = Uri.fromFile(new File( gpac_data_dir ));
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_PICK);
        // Files and directories
        intent.setDataAndType(uriDefaultDir, "vnd.android.cursor.dir/lysesoft.andexplorer.file"); //$NON-NLS-1$
        // Optional filtering on file extension.
        intent.putExtra("browser_filter_extension_whitelist", REGISTERED_FILE_EXTENSIONS); //$NON-NLS-1$
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
            intent.putExtra("browser_filter_extension_whitelist", REGISTERED_FILE_EXTENSIONS); //$NON-NLS-1$
            try {
                startActivityForResult(intent, PICK_FILE_REQUEST);
                return true;
            } catch (ActivityNotFoundException ex) {
                // Not found neither... We build our own dialog to display error
                // Note that this should happen only if we did not embed out own intent
                AlertDialog.Builder builder = new AlertDialog.Builder(this);
                builder.setMessage("Impossible to find an Intent to choose a file... Cannot open file !") //$NON-NLS-1$
                       .setCancelable(true)
                       .setPositiveButton(R.string.cancel, null);
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
                    Log.i(LOG_TAG, "Requesting opening local file " + url); //$NON-NLS-1$
                    openURLasync(url);
                }
            }
            return;
        }
        if (requestCode == AUTH_REQUEST) {
            if (resultCode == RESULT_OK) {
				String user = intent.getStringExtra("USER");
				String passwd = intent.getStringExtra("PASSWD");
				displayPopup("User " + user + " Pass " + passwd, "Success");
            }
            return;
		}
    }

	private void openURLasync(final String url) {
		if (url==null) return;

		if (url.startsWith("gpac ") || url.startsWith("MP4Box ")) {
			if (is_gui_mode) {
				is_gui_mode = false;
				replaceView(gpac_gl_view, gpac_log_view);
			}
			gpac_log_view.setText(null);
			gpac_log_view.append("Executing \"" + url + "\"\n\n");
		} else {
			if (!is_gui_mode) {
				//switching log view to GLSurfaceView must be done on UI thread otherwsie we crash
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
						is_gui_mode = true;
						gpac_render_thread = null;
						replaceView(gpac_log_view, gpac_gl_view);
                    }
                });
			}
		}

        service.execute(new Runnable() {

            @Override
            public void run() {
				final int ex_code;
				if (is_gui_mode) {
					gpac_gl_view.connect(url);
					ex_code = 0;
				} else {
					session_aborted = false;
					in_session = true;
					ex_code = load_service(url);
					in_session = false;
				}
				currentURL = url;

                runOnUiThread(new Runnable() {

                    @Override
                    public void run() {
						if (!is_gui_mode) {
							setTitle(getResources().getString(R.string.app_name));
							gpac_log_view.append("\n" + (session_aborted ? "Aborted - " : "") + "exit code: " + ex_code + "\n");
						} else {
							setTitle(getResources().getString(R.string.app_name));
						}
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

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle item selection
        switch (item.getItemId()) {
            case R.id.open_url:
                return openURL(SERVICE_TYPE_URL);
            case R.id.open_file:
                return openFileDialog();
            case R.id.reloadLast:
				if (in_session) {
					session_aborted=true;
					abort_session();
				}
                openURLasync(currentURL);
                return true;
            case R.id.runGPAC:
				if (in_session) {
					session_aborted=true;
					abort_session();
					return true;
                }
                return openURL(SERVICE_TYPE_GPAC);
            case R.id.runMP4Box:
				if (in_session) {
					session_aborted=true;
					abort_session();
					return true;
                }
                return openURL(SERVICE_TYPE_MP4BOX);
            case R.id.quit:
                this.finish();
                return true;
            case R.id.resetConfig:
				if (in_session) {
					session_aborted=true;
					abort_session();
				}
				reset_config();
		        runOnUiThread(new Runnable() {

		            @Override
		            public void run() {
		                Toast toast = Toast.makeText(GPAC.this, "GPAC Configuration file reset", Toast.LENGTH_SHORT);
		                toast.show();
		            }
		        });
		        
                openURLasync("gpac -cfg");
                return true;
                
/*			case R.id.testAuth:
                Intent intent = new Intent(GPAC.this, URLAuthenticate.class);
				intent.putExtra("URL", "http://bar.foo/test");
				startActivityForResult(intent, AUTH_REQUEST);
                return true;
*/
            default:
                return super.onOptionsItemSelected(item);
        }
    }


    @Override
    protected void onDestroy() {
        service.shutdown();
        synchronized (this) {
            if (wl != null)
                wl.release();
        }
        Log.d(LOG_TAG, "Destroying GPAC instance..."); //$NON-NLS-1$
        if (isLoaded() ) {
            uninit();
            gpac_jni_handle = 0;
            load_errors = null;
        }
        super.onDestroy();
        //we use JNI and cleaned up GPAC, kill parent process otherwise we will get relaunched with old libgpac loaded ...
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Handle the back button
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            // Ask the user if they want to quit
            new AlertDialog.Builder(this).setIcon(android.R.drawable.ic_dialog_alert)
                                         .setTitle(R.string.quit)
                                         .setMessage(R.string.really_quit)
                                         .setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {

                                             @Override
                                             public void onClick(DialogInterface dialog, int which) {
											   GPAC.this.finish();
                                             }

                                         })
                                         .setNegativeButton(R.string.no, null)
                                         .show();

            return true;
        } else {
            return super.onKeyDown(keyCode, event);
        }

    }


    private void displayPopup(final CharSequence message, final CharSequence title) {
        final String fullMsg = getResources().getString(R.string.displayPopupFormat, title, message);
        synchronized (this) {
			// In case of an error message, always popup it
            if (fullMsg.equals(last_displayed_message) && !title.equals("Error"))
                return;
            last_displayed_message = fullMsg;
        }
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                Toast toast = Toast.makeText(GPAC.this, fullMsg, Toast.LENGTH_SHORT);
                toast.show();
            }
        });

    }

    /**
     * Display a message
     *
     * @param message
     * @param title
     * @param errorCode
     */
    public void messageDialog(final String message, final String title, final int status) {
        if (status == 0) {//GF_OK
			if (is_gui_mode) {
				Log.d(LOG_TAG, message);
			} else {
				runOnUiThread(new Runnable() {
					@Override
					public void run() {
						gpac_log_view.append(message);
/*						if (!gpac_log_view.hasSelection()) {
							final int scrollAmount = gpac_log_view.getLayout().getLineTop(gpac_log_view.getLineCount()) - gpac_log_view.getHeight();
							gpac_log_view.scrollTo(0,  (scrollAmount > 0) ? scrollAmount : 0);
						}
*/

					}
				});
			}
			return;
        }
        //show a message box
		runOnUiThread(new Runnable() {

			@Override
			public void run() {
				StringBuilder sb = new StringBuilder();
				sb.append(error_to_string(status) );
				sb.append(' ');
				sb.append(title);
				AlertDialog.Builder builder = new AlertDialog.Builder(GPAC.this);
				builder.setTitle(sb.toString());
				sb.append('\n');
				sb.append(message);
				builder.setMessage(sb.toString());
				builder.setCancelable(true);
				builder.setPositiveButton(R.string.ok, new OnClickListener() {

					@Override
					public void onClick(DialogInterface dialog, int which) {
						dialog.cancel();
					}
				});
				try {
					builder.create().show();
				} catch (WindowManager.BadTokenException e) {
					// May happen when we close the window and there are still somes messages
					Log.e(LOG_TAG, "Failed to display Message " + sb.toString(), e); //$NON-NLS-1$
				}
			}
		});
    }

    /**
     * Called when progress is set, only in player mode with no GUI
     *
     * @param msg
     * @param done
     * @param total
     */
    public void onProgress(final String msg, final int done, final int total) {
        if (Log.isLoggable(LOG_TAG, Log.DEBUG))
            Log.d(LOG_TAG, "Setting progress to " + done + "/" + total + ", message=" + msg); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
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

    private static Map<String, Throwable> load_errors = null;
    /**
     * Loads all libraries
     * @return a map of exceptions containing the library as key and the exception as value. If map is empty, no error
     */
    synchronized void loadAllLibraries() {
        if (load_errors != null)
            return;
        StringBuilder sb = new StringBuilder();
        final String[] toLoad = { "GLESv2", "log",
                                 "jpegdroid", "javaenv",
                                 "mad", "ft2", "faad",
                                 "openjpeg", "png", "z",
                                 "avutil", "swscale", "swresample", "avcodec", "avformat", "avfilter", "avdevice",
                                 "gpacWrapper", "gpac"};
        HashMap<String, Throwable> exceptions = new HashMap<String, Throwable>();
        for (String s : toLoad) {
            try {
                String msg = "Loading library " + s + "...";//$NON-NLS-1$//$NON-NLS-2$
                sb.append(msg);
                Log.i(LOG_TAG, msg);
                System.loadLibrary(s);
            } catch (UnsatisfiedLinkError e) {
                sb.append("Failed to load " + s + ", error=" + e.getLocalizedMessage() + " :: " //$NON-NLS-1$//$NON-NLS-2$//$NON-NLS-3$
                          + e.getClass().getSimpleName() + "\n"); //$NON-NLS-1$
                exceptions.put(s, e);
                Log.e(LOG_TAG, "Failed to load library : " + s + " due to link error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
            } catch (SecurityException e) {
                exceptions.put(s, e);
                Log.e(LOG_TAG, "Failed to load library : " + s + " due to security error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
            } catch (Throwable e) {
                exceptions.put(s, e);
                Log.e(LOG_TAG, "Failed to load library : " + s + " due to Runtime error " + e.getLocalizedMessage(), e); //$NON-NLS-1$ //$NON-NLS-2$
            }
        }

        if (!exceptions.isEmpty()) {
            try {
                PrintStream out = new PrintStream(gpac_install_logs, "UTF-8"); //$NON-NLS-1$//$NON-NLS-2$
                out.println("$Revision$"); //$NON-NLS-1$
                out.println(new Date());
                out.println("\n*** Configuration\n"); //$NON-NLS-1$
                out.println("Data dir ");
                out.println(gpac_data_dir);
                out.println("\nExternal data dir: ");
                out.println(external_storage_dir);
                sb.append("\n*** Exceptions:\n"); //$NON-NLS-1$
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
                Log.e(LOG_TAG, "Failed to output debug info to debug file", e); //$NON-NLS-1$
            }
        }
        load_errors = Collections.unmodifiableMap(exceptions);
    }

	public boolean isLoaded() {
		return (gpac_jni_handle!=0) ? true : false;
	}
    public void initGPAC() throws Exception {
		if (isLoaded())
			return;

        StringBuilder sb = new StringBuilder();
        loadAllLibraries();
        if (!load_errors.isEmpty()) {
            sb.append("Exceptions while loading libraries:"); //$NON-NLS-1$
            for (Map.Entry<String, Throwable> x : load_errors.entrySet()) {
                sb.append('\n')
                  .append(x.getKey())
                  .append('[')
                  .append(x.getValue().getClass().getSimpleName())
                  .append("]: ") //$NON-NLS-1$
                  .append(x.getValue().getLocalizedMessage());
            }
            Log.e(LOG_TAG, sb.toString());
        }

        try {
			int width = gpac_gl_view.getWidth();
			int height = gpac_gl_view.getHeight();
			if (Build.VERSION.SDK_INT >= 28) {
				if ( getWindow().getDecorView().getRootWindowInsets().getDisplayCutout() != null)
					has_display_cutout = true;
		    }
            gpac_jni_handle = init(this, width, height, has_display_cutout, init_url, external_storage_dir, gpac_data_dir );
			if (gpac_jni_handle == 0) {
				throw new Exception("Error while creating instance, no handle created!\n" + sb.toString()); //$NON-NLS-1$
			}
        } catch (Throwable e) {
			onGPACError(e);
            throw new Exception("Error while creating instance\n" + sb.toString()); //$NON-NLS-1$
        }
        gpac_render_thread = Thread.currentThread();

        startupProgress.dismiss();
        Log.i(LOG_TAG, "GPAC is ready"); //$NON-NLS-1$
    }


    /**
     * Called when GPAC initialization fails
     *
     * @param e
     */
    public void onGPACError(final Throwable e) {
        startupProgress.dismiss();
        Log.e(LOG_TAG, "GPAC Error", e); //$NON-NLS-1$
        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                StringBuilder sb = new StringBuilder();
                sb.append("Failed to init GPAC due to ");
                sb.append(e.getClass().getSimpleName());
                AlertDialog.Builder builder = new AlertDialog.Builder(GPAC.this);
                builder.setTitle(sb.toString());
                sb.append("\nDescription: ");
                sb.append(e.getLocalizedMessage());
                sb.append("\nRevision: $Revision$"); //$NON-NLS-1$
                sb.append("\nConfiguration information :");
                sb.append("\nData dir:");
                sb.append(gpac_data_dir);
                sb.append("\nExternal Data dir:");
                sb.append(external_storage_dir);

                builder.setMessage(sb.toString());
                builder.setCancelable(true);
                builder.setPositiveButton(R.string.ok, new OnClickListener() {

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
        if (gpac_gl_view != null)
            gpac_gl_view.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (gpac_gl_view != null)
            gpac_gl_view.onResume();
    }

    /**
     * @see android.app.Activity#onStop()
     */
    @Override
    protected void onStop() {
        Log.i(LOG_TAG, "onStop called on activity"); //$NON-NLS-1$
        super.onStop();
    }

    /**
     * Show the virtual keyboard
     *
     * @param showKeyboard
     */
    public void showKeyboard(boolean showKeyboard) {
        if (this.keyboard_visible == showKeyboard) return;
        this.keyboard_visible = showKeyboard;

        runOnUiThread(new Runnable() {

            @Override
            public void run() {
                InputMethodManager mgr = ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE));
                if (GPAC.this.keyboard_visible) {
                	mgr.showSoftInput(gpac_gl_view, 0);
					getActionBar().hide();
					getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
					getWindow().addFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
				} else {
                	mgr.hideSoftInputFromWindow(gpac_gl_view.getWindowToken(), 0);
					getActionBar().show();
					getWindow().clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
					getWindow().clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
					hideSystemUI();
				}
                //mgr.toggleSoftInputFromWindow(gpac_gl_view.getWindowToken(), 0, 0);
				gpac_gl_view.requestFocus();
            }
        });
    }
	@Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        // When the window loses focus (e.g. the action overflow is shown),
        // cancel any pending hide action. When the window gains focus,
        // hide the system UI.
        if (hasFocus) {
            delayedHide(100);
        } else {
            mHideHandler.removeMessages(0);
        }
    }

    private void hideSystemUI() {
		View decor_view = getWindow().getDecorView();
        decor_view.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
										 | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
										 | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
										 | View.SYSTEM_UI_FLAG_FULLSCREEN
										 | View.SYSTEM_UI_FLAG_LOW_PROFILE
										 | View.SYSTEM_UI_FLAG_IMMERSIVE);
    }

     private void showSystemUI() {
		View decor_view = getWindow().getDecorView();
        decor_view.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
										 | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
										 | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    private final Handler mHideHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            hideSystemUI();
        }
    };

    private void delayedHide(int delayMillis) {
        mHideHandler.removeMessages(0);
        mHideHandler.sendEmptyMessageDelayed(0, delayMillis);
    }

    private void replaceView(View oldV, View newV){
        ViewGroup par = (ViewGroup)oldV.getParent();
        if(par == null) return;

        int idx = par.indexOfChild(oldV);
        par.removeViewAt(idx);
        par.addView(newV, idx);
    }

    /**
     * Toggle Sensors on/off
     *
     * @param active sensor (true/false)
     */
    public void sensorSwitch(final int type, final boolean active) {
		Log.i(LOG_TAG, "Received " + (active ? "Un-" : "") + "Register Sensors call for type " + type);
        if (active) {
			switch (type) {
			case 0:
				sensors.registerSensors();
				break;
			case 1:
				sensors.registerLocation();
				break;
			}
        } else {
			switch (type) {
			case 0:
				sensors.unregisterSensors();
				break;
			case 1:
				sensors.unregisterLocation();
				break;
			}
        }
    }
    public void setOrientation(final int type) {

		if (type > 0) {
			last_orientation = this.getRequestedOrientation();
			if ((type == 1) || (type == 4))
				this.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
			else
				this.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
		} else {
			this.setRequestedOrientation(last_orientation);
		}
	}

    private boolean checkCurrentThread() throws RuntimeException {
		if (!is_gui_mode) return false;
        if (Thread.currentThread() != gpac_render_thread)
            throw new RuntimeException("Method called outside allowed Thread scope !"); //$NON-NLS-1$
		return true;
    }

    public void connect(String url) {
        if (gpac_render_thread == null)
			gpac_render_thread = Thread.currentThread();
        else if (!checkCurrentThread())
			return;

		Log.i(LOG_TAG, "connect(" + url + ")"); //$NON-NLS-1$ //$NON-NLS-2$
		load_service(url);
    }

    /**
     * Call this method when a key has been pressed
     *
     * @param keyCode
     * @param event
     * @param pressed true if key is pressed, false if key is released
     * @param unicode
     */
    public void eventKey(int keyCode, KeyEvent event, boolean pressed, int unicode) {
        if (!checkCurrentThread()) return;
        eventkeypress(keyCode, event.getScanCode(), pressed ? 1 : 0, event.getFlags(), unicode);
    }

    /**
     * Renders the current frame
     */
    public void render() {
        if (!checkCurrentThread()) return;
        render_frame();
    }

    /**
     * Resizes the object to new dimensions
     *
     * @param width
     * @param height
     */
    public void resize(int width, int height) {
        if (!checkCurrentThread()) return;
        Log.i(LOG_TAG, "Resizing to " + width + "x" + height); //$NON-NLS-1$ //$NON-NLS-2$

		int orientation = getResources().getConfiguration().orientation;
		int rotation = getWindowManager().getDefaultDisplay().getRotation();
		int actual_orientation = -1;
		if ((orientation == Configuration.ORIENTATION_LANDSCAPE)
			&& ((rotation == android.view.Surface.ROTATION_0) || (rotation == android.view.Surface.ROTATION_90))
		) {
			orientation = 2; //GF_DISPLAY_MODE_LANDSCAPE
		} else if ((orientation == Configuration.ORIENTATION_PORTRAIT)
			&& ((rotation == android.view.Surface.ROTATION_0) || (rotation == android.view.Surface.ROTATION_90))
		) {
			orientation = 1; //GF_DISPLAY_MODE_PORTRAIT
		} else if ((orientation == Configuration.ORIENTATION_LANDSCAPE)
			&& ((rotation == android.view.Surface.ROTATION_180) || (rotation == android.view.Surface.ROTATION_270))
		) {
			orientation = 3; //GF_DISPLAY_MODE_LANDSCAPE_INV
		} else if ((orientation == Configuration.ORIENTATION_PORTRAIT)
			&& ((rotation == android.view.Surface.ROTATION_180) || (rotation == android.view.Surface.ROTATION_270))
		) {
			orientation = 4; //GF_DISPLAY_MODE_PORTRAIT_INV
		} else {
			orientation = 0; //GF_DISPLAY_MODE_UNKNOWN
		}
        set_size(width, height, orientation);
    }

    private boolean touched = false;

    /**
     * Call this when a motion event occurs
     *
     * @param event
     */
    public void motionEvent(MotionEvent event) {
        if (!checkCurrentThread()) return;
        final float x = event.getX();
        final float y = event.getY();
        switch (event.getAction()) {
            // not in 1.6 case MotionEvent.ACTION_POINTER_1_DOWN:
            case MotionEvent.ACTION_DOWN:
               eventmousemove(x, y);
               eventmousedown(x, y);
               touched = true;
               break;
            // not in 1.6 case MotionEvent.ACTION_POINTER_1_UP:
            case MotionEvent.ACTION_UP:
               if ( !touched ) break;
               eventmouseup(x, y);
               eventmousemove(0, 0);
               touched = false;
               break;
            case MotionEvent.ACTION_MOVE:
            	eventmousemove(x, y);
            	if ( !touched ) {
            		touched = true;
            		eventmousedown(x, y);
            	}
            	break;
            case MotionEvent.ACTION_CANCEL:
            	if ( !touched )
            		break;
              eventmouseup(x, y);
              touched = false;
              break;
        }
    }

    public void onOrientationChange(float yaw, float pitch, float roll){
        eventorientationchange(yaw, pitch, roll);
    }

    public void onLocationChange(double longitude, double latitude, double altitude, float bearing, float accuracy) {
        eventlocationchange(longitude, latitude, altitude, bearing, accuracy);
    }

    /**
     * All Native functions, those functions are binded using the handle
     */

    /**
     * Used to create the GPAC instance
     *
     * @param main_act
     * @param width
     * @param height
     * @param cfg_dir
     * @param modules_dir
     * @param cache_dir
     * @param font_dir
     * @param gui_dir
     * @return
     */
    private native long init(GPAC main_act, int width, int height, boolean has_display_cutout, String url_to_open, String profile, String data_dir);

    /**
     * Opens an URL
     *
     * @param url The URL to open
     */
    private native int load_service(String url);


    /**
     * Renders
     *
     * @param handle
     */
    private native void render_frame();

    /**
     * Resizes the current view
     *
     * @param width The new width to set
     * @param height The new height to set
     */
    private native void set_size(int width, int height, int orientation);

    /**
     * Free all GPAC resources
     */
    private native void uninit();

    /**
     * To call when a key has been pressed
     *
     * @param keycode
     * @param rawkeycode
     * @param up
     * @param flag
     */
    private native void eventkeypress(int keycode, int rawkeycode, int up, int flag, int unicode);

    /**
     * To call when a mouse is down
     *
     * @param x Position in pixels
     * @param y Position in pixels
     */
    private native void eventmousedown(float x, float y);

    /**
     * To call when a mouse is up (released)
     *
     * @param x Position in pixels
     * @param y Position in pixels
     */
    private native void eventmouseup(float x, float y);

    /**
     * To call when a mouse is moving
     *
     * @param x Position in pixels
     * @param y Position in pixels
     */
    private native void eventmousemove(float x, float y);

    /**
     * To call when a change in orientation is returned from the sensors
     * All angles are in RADIANS and positive in the COUNTER-CLOCKWISE direction
     *
     * @param x yaw (rotation around the -Z axis) range: [-PI, PI]
     * @param y pitch (rotation around the -X axis) range: [-PI/2, PI/2]
     * @param z roll (rotation around the Y axis) range: [-PI, PI]
     */
    private native void eventorientationchange(float x, float y, float z);

    private native void eventlocationchange(double longitude, double latitude, double altitude, float bearing, float accuracy);

    public native String error_to_string(int err_code);

	public native void abort_session();
    private native void reset_config();

}
