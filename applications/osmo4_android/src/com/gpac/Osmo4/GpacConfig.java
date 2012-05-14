/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.gpac.Osmo4;

import java.io.File;
import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Environment;
import android.util.Log;

/**
 * This class handles all GPAC configuration directories
 * 
 * @author Pierre Souchay (VizionR SAS) (last changed by $LastChangedBy$)
 * @version $Revision$
 * 
 */
public class GpacConfig {

    private final static String LOG_GPAC_CONFIG = GpacConfig.class.getSimpleName();

    /**
     * Default Constructor
     * 
     * @param context
     */
    public GpacConfig(Context context) {
        File rootCfg = Environment.getExternalStorageDirectory();
        File osmo = new File(rootCfg, "osmo"); //$NON-NLS-1$
        gpacConfigDirectory = osmo.getAbsolutePath() + '/';
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacConfigDirectory + " for osmo"); //$NON-NLS-1$ //$NON-NLS-2$
        // gpacCacheDirectory = Environment.getDownloadCacheDirectory().getAbsolutePath();
        // if (Build.VERSION.SDK_INT > 7){
        gpacCacheDirectory = context.getCacheDir().getAbsolutePath();
        // } else {
        // gpacCacheDirectory =
        // }
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacCacheDirectory + " for cache"); //$NON-NLS-1$ //$NON-NLS-2$
        //
        //Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacModulesDirectory + " for modules"); //$NON-NLS-1$ //$NON-NLS-2$
        String dataDir;
        try {
            if (context == null || context.getPackageManager() == null) {
                dataDir = Environment.getDataDirectory() + "/data/com.gpac.Osmo4/"; //$NON-NLS-1$
                Log.e(LOG_GPAC_CONFIG, "Cannot get context or PackageManager, using default directory=" + dataDir); //$NON-NLS-1$
            } else
                dataDir = context.getPackageManager().getApplicationInfo(context.getPackageName(), 0).dataDir;
        } catch (NameNotFoundException e) {
            Log.e(LOG_GPAC_CONFIG, "This is bad, we cannot find ourself : " + context.getPackageName(), e); //$NON-NLS-1$
            throw new RuntimeException("Cannot find package " + context.getPackageName(), e); //$NON-NLS-1$
        }
        gpacLibsDirectory = dataDir + "/lib/"; //$NON-NLS-1$
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacLibsDirectory + " for libraries"); //$NON-NLS-1$ //$NON-NLS-2$

    }

    /**
     * Ensures all directories are created
     * 
     * @return The {@link GpacConfig} instance itself
     */
    public GpacConfig ensureAllDirectoriesExist() {
        for (String s : new String[] { gpacConfigDirectory, gpacCacheDirectory }) {
            createDirIfNotExist(s);
        }
        return this;
    }

    private final String gpacConfigDirectory;

    /**
     * Default directory for GPAC configuration directory, ends with /
     * 
     * @return the gpacConfigDirectory
     */
    public String getGpacConfigDirectory() {
        return gpacConfigDirectory;
    }

    /**
     * Directory of Android containing all fonts
     * 
     * @return the gpacFontDirectory
     */
    public String getGpacFontDirectory() {
        return gpacFontDirectory;
    }

    /**
     * Default directory for GPAC modules directory, ends with /
     * 
     * @return the gpacModulesDirectory
     */
    public String getGpacModulesDirectory() {
        // return gpacModulesDirectory;
        return gpacLibsDirectory;
    }

    /**
     * @return the gpacLibsDirectory
     */
    public String getGpacLibsDirectory() {
        return gpacLibsDirectory;
    }

    /**
     * Default directory for cached files
     * 
     * @return the gpacCacheDirectory
     */
    public String getGpacCacheDirectory() {
        return gpacCacheDirectory;
    }

    private final String gpacFontDirectory = "/system/fonts/"; //$NON-NLS-1$

    // private final String gpacModulesDirectory;

    private final String gpacLibsDirectory;

    private final String gpacCacheDirectory;

    /**
     * Creates a given directory if it does not exist
     * 
     * @param path
     */
    private static boolean createDirIfNotExist(String path) {
        File f = new File(path);
        if (!f.exists()) {
            if (!f.mkdirs()) {
                Log.e(LOG_GPAC_CONFIG, "Failed to create directory " + path); //$NON-NLS-1$
                return false;
            } else {
                Log.i(LOG_GPAC_CONFIG, "Created directory " + path); //$NON-NLS-1$
            }
        }
        return true;
    }

    /**
     * Get the GPAC.cfg file
     * 
     * @return the file
     */
    public File getGpacConfigFile() {
        return new File(getGpacConfigDirectory(), "GPAC.cfg"); //$NON-NLS-1$
    }

    /**
     * Get the GPAC.cfg file
     * 
     * @return the file
     */
    public File getGpacLastRevFile() {
        return new File(getGpacConfigDirectory(), "lastRev.txt"); //$NON-NLS-1$
    }

    /**
     * Get the configuration as text
     * 
     * @return a String with newlines representing all the configuration
     */
    public String getConfigAsText() {
        StringBuilder sb = new StringBuilder();
        sb.append("GpacConfigDirectory=").append(getGpacConfigDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacModulesDirectory=").append(getGpacModulesDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacFontDirectory=").append(getGpacFontDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacCacheDirectory=").append(getGpacCacheDirectory()).append('\n'); //$NON-NLS-1$
        return sb.toString();
    }
}
