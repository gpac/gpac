/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.artemis.Osmo4;

/**
 * @copyright RTL Group 2008
 * @author RTL Group DTIT software development team (last changed by $LastChangedBy$)
 * @version $Revision$
 * 
 */
public interface GPACInstanceInterface {

    /**
     * @version $Revision$
     * 
     */
    public static class GpacInstanceException extends Exception {

        /**
         * 
         */
        private static final long serialVersionUID = 3207851655866335152L;

        /**
         * Constructor
         * 
         * @param msg
         */
        public GpacInstanceException(String msg) {
            super(msg);
        }

    };

    /**
     * Call this method to disconnect
     */
    public void disconnect();

    /**
     * Call this method to connect to a given URL
     * 
     * @param url The URL to connect to
     */
    public void connect(String url);

    /**
     * Destroys the current instance, you won't be able to use it anymore...
     */
    public void destroy();

}
