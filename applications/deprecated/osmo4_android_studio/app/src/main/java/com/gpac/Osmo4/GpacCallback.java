/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.gpac.Osmo4;

/**
 * Interface to implement by Java Objects to listen for callbacks from GPAC native code
 * 
 * @version $Revision$
 * 
 */
public interface GpacCallback {

    /**
     * GPAC Error codes
     * 
     * @version $Revision$
     * 
     */
    public enum GF_Err {

        /** Message from any scripting engine used in the presentation (ECMAScript, MPEG-J, ...) (Info). */
        GF_SCRIPT_INFO(3),
        /**
         * Indicates an data frame has several AU packed (not MPEG-4 compliant). This is used by decoders to force
         * multiple decoding of the same data frame (Info).
         */
        GF_PACKED_FRAMES(2),
        /** Indicates the end of a stream or of a file (Info). */
        GF_EOS(1),
        /*
         * ! \n\n
         */
        /** Operation success (no error). */
        GF_OK(0),
        /** \n */
        /** One of the input parameter is not correct or cannot be used in the current operating mode of the framework. */
        GF_BAD_PARAM(-1),
        /** Memory allocation failure. */
        GF_OUT_OF_MEM(-2),
        /** Input/Output failure (disk access, system call failures) */
        GF_IO_ERR(-3),
        /** The desired feature or operation is not supported by the framework */
        GF_NOT_SUPPORTED(-4),
        /** Input data has been corrupted */
        GF_CORRUPTED_DATA(-5),
        /** A modification was attempted on a scene node which could not be found */
        GF_SG_UNKNOWN_NODE(-6),
        /** The PROTO node interface does not match the nodes using it */
        GF_SG_INVALID_PROTO(-7),
        /** An error occured in the scripting engine */
        GF_SCRIPT_ERROR(-8),
        /**
         * Buffer is too small to contain decoded data. Decoders shall use this error whenever they need to resize their
         * output memory buffers
         */
        GF_BUFFER_TOO_SMALL(-9),
        /** Bitstream is not compliant to the specfication it refers to */
        GF_NON_COMPLIANT_BITSTREAM(-10),
        /** No decoders could be found to handle the desired media type */
        GF_CODEC_NOT_FOUND(-11),
        /** The URL is not properly formatted or cannot be found */
        GF_URL_ERROR(-12),
        /** An service error has occured at the local side */
        GF_SERVICE_ERROR(-13),
        /** A service error has occured at the remote (server) side */
        GF_REMOTE_SERVICE_ERROR(-14),
        /** The desired stream could not be found in the service */
        GF_STREAM_NOT_FOUND(-15),
        /** The IsoMedia file is not a valid one */
        GF_ISOM_INVALID_FILE(-20),
        /** The IsoMedia file is not complete. Either the file is being downloaded, or it has been truncated */
        GF_ISOM_INCOMPLETE_FILE(-21),
        /** The media in this IsoMedia track is not valid (usually due to a broken stream description) */
        GF_ISOM_INVALID_MEDIA(-22),
        /** The requested operation cannot happen in the current opening mode of the IsoMedia file */
        GF_ISOM_INVALID_MODE(-23),
        /** This IsoMedia track refers to media outside the file in an unknown way */
        GF_ISOM_UNKNOWN_DATA_REF(-24),

        /** An invalid MPEG-4 Object Descriptor was found */
        GF_ODF_INVALID_DESCRIPTOR(-30),
        /** An MPEG-4 Object Descriptor was found or added to a forbidden descriptor */
        GF_ODF_FORBIDDEN_DESCRIPTOR(-31),
        /** An invalid MPEG-4 BIFS command was detected */
        GF_ODF_INVALID_COMMAND(-32),
        /** The scene has been encoded using an unknown BIFS version */
        GF_BIFS_UNKNOWN_VERSION(-33),

        /** The remote IP address could not be solved */
        GF_IP_ADDRESS_NOT_FOUND(-40),
        /** The connection to the remote peer has failed */
        GF_IP_CONNECTION_FAILURE(-41),
        /** The network operation has failed */
        GF_IP_NETWORK_FAILURE(-42),
        /** The network connection has been closed */
        GF_IP_CONNECTION_CLOSED(-43),
        /** The network operation has failed because no data is available */
        GF_IP_NETWORK_EMPTY(-44),
        /** The network operation has been discarded because it would be a blocking one */
        GF_IP_SOCK_WOULD_BLOCK(-45),
        /**
         * UDP connection did not receive any data at all. Signaled by client services to reconfigure network if
         * possible
         */
        GF_IP_UDP_TIMEOUT(-46),

        /** Authentication with the remote host has failed */
        GF_AUTHENTICATION_FAILURE(-50),
        /** Script not ready for playback */
        GF_SCRIPT_NOT_READY(-51),

        /**
         * Unknown GPAC Error code
         */
        JAVA_UNKNOWN_ERROR(-100);

        int value;

        GF_Err(int x) {
            this.value = x;
        }

        /**
         * Get a GPAC Error code from its value
         * 
         * @param status The int status corresponding to the error
         * @return A {@link GF_Err} object
         */
        public static GF_Err getError(int status) {
            for (GF_Err x : values()) {
                if (x.value == status)
                    return x;
            }
            return JAVA_UNKNOWN_ERROR;
        }
    }

    /**
     * Mapping between GPAC Log Modules and Java symbols
     * 
     * @version $Revision$
     * 
     */
    public enum GF_Log_Module implements Comparable<GF_Log_Module> {
        /** Log message from the core library (init, threads, network calls, etc) */
        GF_LOG_CORE(0),
        /** Log message from a raw media parser (BIFS, LASeR, A/V formats) */
        GF_LOG_CODING(1),
        /** Log message from a bitstream parser (IsoMedia, MPEG-2 TS, OGG, ...) */
        GF_LOG_CONTAINER(2),
        /** Log message from the network/service stack (messages & co) */
        GF_LOG_NETWORK(3),
        /** Log message from the network/service stack (messages & co) */
        GF_LOG_HTTP(4),
        /** Log message from the RTP/RTCP stack (TS info) and packet structure & hinting (debug) */
        GF_LOG_RTP(5),
        /** Log message from a codec */
        GF_LOG_CODEC(6),
        /** Log message from any XML parser (context loading, etc) */
        GF_LOG_PARSER(7),
        /** Log message from the terminal/compositor, indicating media object state */
        GF_LOG_MEDIA(8),
        /** Log message from the scene graph/scene manager (handling of nodes and attribute modif, DOM core) */
        GF_LOG_SCENE(9),
        /** Log message from the scripting engine */
        GF_LOG_SCRIPT(10),
        /** Log message from event handling */
        GF_LOG_INTERACT(11),
        /** Log message from compositor */
        GF_LOG_COMPOSE(12),
        /** Log message from compositor */
        GF_LOG_COMPTIME(13),
        /** Log for video object cache */
        GF_LOG_CACHE(14),
        /** Log message from multimedia I/O devices (audio/video input/output, ...) */
        GF_LOG_MMIO(15),
        /** Log for runtime info (times, memory, CPU usage) */
        GF_LOG_RTI(16),
        /** Log for memory tracker */
        GF_LOG_MEMORY(17),
        /** Log for audio compositor */
        GF_LOG_AUDIO(18),
        /** generic Log for modules */
        GF_LOG_MODULE(19),
        /** Log for GPAC mutexes and threads (Very verbose at DEBUG)*/
        GF_LOG_MUTEX(20),
        /*! Log for threads and condition */
		GF_LOG_CONDITION(21),
		/*! Log for all HTTP streaming */
		GF_LOG_DASH(22),
		/*! Log for all filter engine */
		GF_LOG_FILTER(23),
		/*! Log for all filter scheduler */
		GF_LOG_SCHEDULER(24),
		/*! Log for ROUTE demux */
		GF_LOG_ROUTE(25),
		/*! Log for all messages coming from GF_Terminal or script alert()*/
		GF_LOG_CONSOLE(26),
		/*! Log for all messages coming the application, not used by libgpac or the modules*/
		GF_LOG_APP(27),
        /**
         * Unknown Log subsystem
         */
        GF_LOG_UNKNOWN(28);

        int value;

        /**
         * Private Constructor
         */
        private GF_Log_Module(int x) {
            this.value = x;
        }

        /**
         * Finds a module from its module_code
         * 
         * @param module_code
         * @return The Module found (never null)
         */
        public static GF_Log_Module getModule(int module_code) {
            for (GF_Log_Module x : values()) {
                if (x.value == module_code)
                    return x;
            }
            return GF_LOG_UNKNOWN;
        }
    }

    /**
     * Display a message
     * 
     * @param message
     * @param title
     * @param errorCode
     */
    public void displayMessage(String message, String title, int errorCode);
    
    
    /**
     * Called when setup log file
     * 
     * @param path
     */
    public void setLogFile(String path);

    /**
     * Called when logging
     * 
     * @param level
     * @param module
     * @param message
     */
    public void onLog(int level, int module, String message);

    /**
     * Called when progress is set
     * 
     * @param msg
     * @param done
     * @param total
     */
    public void onProgress(String msg, int done, int total);

    /**
     * Called when GPAC is ready
     */
    public void onGPACReady();

    /**
     * Called when GPAC initialization fails
     * 
     * @param e
     */
    public void onGPACError(Throwable e);

    // /**
    // * Returns UserName:Password for given site
    // *
    // * @param siteURL The URL of site requiring authorization
    // * @param userName The User Name (may be null if never set)
    // * @param password The password (may be null if never set)
    // * @return A String formatted as username:password
    // */
    // public String onGPACAuthorization(String siteURL, String userName, String password);

    /**
     * Set the new caption for the application
     * 
     * @param newCaption The new caption to set for application
     */
    public void setCaption(String newCaption);

    /**
     * Show the virtual keyboard
     * 
     * @param showKeyboard
     */
    public void showKeyboard(boolean showKeyboard);

    /**
     * Toggle Sensors on/off
     * 
     * @param active sensor (true/false)
     */
    public void sensorSwitch(boolean active);
    

}
