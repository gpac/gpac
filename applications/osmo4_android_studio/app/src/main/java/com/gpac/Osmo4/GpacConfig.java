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

/**
 * NOTE FOR DEVELOPERS
 * Whenever you add or change a path here, you MUST verify also the corresponding path in src/utils/os_config_init.c
 */
public class GpacConfig {

    private final static String LOG_GPAC_CONFIG = GpacConfig.class.getSimpleName();

    /**
     * Default Constructor
     *
     * @param context
     */
    public GpacConfig(Context context) {
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
        gpacAppDirectory = dataDir + '/';
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacAppDirectory + " for osmo"); //$NON-NLS-1$ //$NON-NLS-2$
        gpacCacheDirectory = context.getCacheDir().getAbsolutePath();
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacCacheDirectory + " for cache"); //$NON-NLS-1$ //$NON-NLS-2$
        //
        //Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacModulesDirectory + " for modules"); //$NON-NLS-1$ //$NON-NLS-2$
        gpacLibsDirectory = dataDir + "/lib/"; //$NON-NLS-1$
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacLibsDirectory + " for libraries"); //$NON-NLS-1$ //$NON-NLS-2$

        gpacGuiDirectory = dataDir + "/share/gui/";
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacGuiDirectory + " for GUI"); //$NON-NLS-1$ //$NON-NLS-2$

        gpacShaderDirectory = dataDir + "/share/shaders/";
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacShaderDirectory + " for shader files"); //$NON-NLS-1$ //$NON-NLS-2$

        File osmo = new File(Environment.getExternalStorageDirectory(), "osmo"); //$NON-NLS-1$
        gpacLogDirectory = osmo.getAbsolutePath() + "/log/";
        Log.v(LOG_GPAC_CONFIG, "Using directory " + gpacLogDirectory + " for log files"); //$NON-NLS-1$ //$NON-NLS-2$

        //check if GPAC.cfg exists in /sdcard/osmo
        File gpac_cfg = new File(osmo.getAbsolutePath(), "GPAC.cfg");
		if (gpac_cfg.exists())
			gpacConfigDirectory = osmo.getAbsolutePath() + "/";
		else
			gpacConfigDirectory = null;
    }

    /**
     * Ensures all directories are created
     *
     * @return The {@link GpacConfig} instance itself
     */
    public GpacConfig ensureAllDirectoriesExist() {
        for (String s : new String[] { gpacAppDirectory, gpacCacheDirectory, gpacShaderDirectory, gpacLogDirectory }) {
            createDirIfNotExist(s);
        }
        return this;
    }

    /**
     * Default directory for GPAC configuration directory, ends with /
     *
     * @return the gpacAppDirectory
     */
    public String getGpacAppDirectory() {
        return gpacAppDirectory;
    }


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

    /**
     * Default directory for GUI files
     *
     * @return the gpacGuiDirectory
     */
    public String getGpacGuiDirectory() {
        return gpacGuiDirectory;
    }

    /**
     * Default directory for shader files
     *
     * @return the gpacShaderDirectory
     */
    public String getGpacShaderDirectory() {
        return gpacShaderDirectory;
    }

    /**
     * Default directory for log files
     *
     * @return the gpacLogDirectory
     */
    public String getGpacLogDirectory() {
        return gpacLogDirectory;
    }

    private final String gpacAppDirectory;

    private final String gpacConfigDirectory;

    private final String gpacFontDirectory = "/system/fonts/"; //$NON-NLS-1$

    // private final String gpacModulesDirectory;

    private final String gpacLibsDirectory;

    private final String gpacCacheDirectory;

    private final String gpacGuiDirectory;

    private final String gpacShaderDirectory;

    private final String gpacLogDirectory;

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
        sb.append("GpacAppDirectory=").append(getGpacAppDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacModulesDirectory=").append(getGpacModulesDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacFontDirectory=").append(getGpacFontDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacCacheDirectory=").append(getGpacCacheDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacGuiDirectory=").append(getGpacGuiDirectory()).append('\n'); //$NON-NLS-1$
        sb.append("GpacShaderDirectory=").append(getGpacShaderDirectory()).append('\n'); //$NON-NLS-1$
        return sb.toString();
    }
}
