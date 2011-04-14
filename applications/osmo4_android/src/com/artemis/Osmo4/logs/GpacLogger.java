/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.artemis.Osmo4.logs;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.Date;
import java.util.SortedSet;
import java.util.TreeSet;
import android.util.Log;
import com.artemis.Osmo4.Osmo4Renderer;
import com.artemis.Osmo4.GpacCallback.GF_Log_Module;

/**
 * This class handles logging.
 * 
 * It will be extended soon to be configurable and enable saving files on disk
 * 
 * @version $Revision$
 * 
 */
public class GpacLogger {

    /**
     * Default Constructor
     */
    public GpacLogger() {
        loggedModules.add(GF_Log_Module.GF_LOG_AUDIO);
        loggedModules.add(GF_Log_Module.GF_LOG_MEDIA);
        loggedModules.add(GF_Log_Module.GF_LOG_MODULE);
        loggedModules.add(GF_Log_Module.GF_LOG_CORE);

    }

    private boolean enableLogOnDisk = false;

    /**
     * @return the enableLogOnDisk
     */
    public synchronized boolean isEnableLogOnDisk() {
        return enableLogOnDisk;
    }

    /**
     * @param enableLogOnDisk the enableLogOnDisk to set
     */
    public synchronized void setEnableLogOnDisk(boolean enableLogOnDisk) {
        this.enableLogOnDisk = enableLogOnDisk;
    }

    private final File logger = new File(Osmo4Renderer.GPAC_CACHE_DIR, "gpac.log"); //$NON-NLS-1$

    /**
     * Called when creating logger
     */
    public void onCreate() {
        if (!enableLogOnDisk)
            return;
        if (writer == null) {
            try {
                writer = new PrintStream(new BufferedOutputStream(new FileOutputStream(logger), 128), true, "UTF-8"); //$NON-NLS-1$
            } catch (Exception e) {
                Log.e(GpacLogger.class.getSimpleName(), "Failed to create writer", e); //$NON-NLS-1$
            }
        }
        if (writer != null)
            writer.println("New log $Revision$ at " + new Date()); //$NON-NLS-1$
    }

    /**
     * Called when stopping logger
     */
    public void onDestroy() {
        if (!enableLogOnDisk)
            return;
        PrintStream w = this.writer;
        writer = null;
        if (w != null) {
            w.println("Closing log file at " + new Date()); //$NON-NLS-1$
            w.close();
        }
    }

    /**
     * Logs from C-code
     * 
     * @param level
     * @param module
     * @param message
     */
    public void onLog(int level, int module, String message) {
        GF_Log_Module gModule = GF_Log_Module.getModule(module);
        if (loggedModules.contains(gModule)) {
            if (loggedLevel <= level)
                doLog(gModule, level, message);
        } else if (defaultLoggedLevel <= level) {
            doLog(gModule, level, message);
        }
    }

    private void doLog(GF_Log_Module module, int level, String message) {
        Log.println(level, module.name(), message);
        if (enableLogOnDisk) {
            PrintStream s = writer;
            if (s != null) {
                s.println(module.name() + "\t" + message); //$NON-NLS-1$
                s.flush();
            }
        }
    }

    private final SortedSet<GF_Log_Module> loggedModules = new TreeSet<GF_Log_Module>();

    // The log level used by GPAC modules that are part of loggedModules collection
    private int loggedLevel = Log.DEBUG;

    // The log level used by GPAC modules not part of loggedModules collection
    private int defaultLoggedLevel = Log.INFO;

    /**
     * @return the loggedLevel
     */
    public synchronized int getLoggedLevel() {
        return loggedLevel;
    }

    /**
     * @param loggedLevel the loggedLevel to set
     */
    public synchronized void setLoggedLevel(int loggedLevel) {
        this.loggedLevel = loggedLevel;
    }

    /**
     * @return the defaultLoggedLevel
     */
    public synchronized int getDefaultLoggedLevel() {
        return defaultLoggedLevel;
    }

    /**
     * @param defaultLoggedLevel the defaultLoggedLevel to set
     */
    public synchronized void setDefaultLoggedLevel(int defaultLoggedLevel) {
        this.defaultLoggedLevel = defaultLoggedLevel;
    }

    private volatile PrintStream writer;

};
