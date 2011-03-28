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
        };

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
     * Display a message
     * 
     * @param message
     * @param title
     * @param errorCode
     */
    public void displayMessage(String message, String title, int errorCode);

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
