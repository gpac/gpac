/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.artemis.Osmo4.logs;

import java.util.SortedSet;
import java.util.TreeSet;
import android.util.Log;
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
    }

    private final SortedSet<GF_Log_Module> loggedModules = new TreeSet<GF_Log_Module>();

    private final int loggedLevel = Log.DEBUG;

    private final int defaultLoggedLevel = Log.INFO;
};
