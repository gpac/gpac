/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.gpac.Osmo4.extra;

import java.io.File;

/**
 * @version $Revision$
 * 
 */
public class FileEntry implements Comparable<FileEntry> {

    private final File file;

    private final String name;

    /**
     * Constructor
     * 
     * @param f
     */
    public FileEntry(File f) {
        this.file = f;
        this.name = f.getName();
    }

    /**
     * Constructor
     * 
     * @param f
     * @param name The name to use
     */
    public FileEntry(File f, String name) {
        this.file = f.getAbsoluteFile();
        this.name = name;
    }

    /**
     * Get the name of option
     * 
     * @return The name of option
     */
    public String getName() {
        return name;
    }

    @Override
    public int compareTo(FileEntry o) {
        return getName().toLowerCase().compareTo(o.getName());
    }

    /**
     * @return the file
     */
    public File getFile() {
        return file;
    }
}