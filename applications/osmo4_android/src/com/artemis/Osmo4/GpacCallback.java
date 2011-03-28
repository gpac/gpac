/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.artemis.Osmo4;

/**
 * Interface to implement by Java Objects to listen for callbacks from GPAC native code
 * 
 * @version $Revision$
 * 
 */
public interface GpacCallback {

    /**
     * Display a message
     * 
     * @param message
     * @param title
     */
    public void displayMessage(String message, String title);

    /**
     * Called when logging
     * 
     * @param level
     * @param module
     * @param message
     * @param arguments
     */
    public void log(int level, int module, String message, Object... arguments);

    /**
     * Called when progress is set
     * 
     * @param msg
     * @param done
     * @param total
     */
    public void onProgress(String msg, int done, int total);
}
