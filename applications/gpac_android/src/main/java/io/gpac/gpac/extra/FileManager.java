package io.gpac.gpac.extra;

import android.os.Build;
import android.support.annotation.Nullable;
import android.util.Log;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;


public class FileManager {

    public static final String SORT_ORDER_ASC = "ASC";
    public static final String SORT_ORDER_DESC = "DESC";

    public static final String SORT_BY_NAME = "NAME";
    public static final String SORT_BY_SIZE = "SIZE";
    public static final String SORT_BY_LAST_MODIFIED = "LAST_MODIFIED";

    public static String getExtension(String url) {

        if (url.contains("?")) {
            url = url.substring(0, url.indexOf("?"));
        }
        if (url.lastIndexOf(".") == -1) {
            return null;
        } else {
            String ext = url.substring(url.lastIndexOf("."));
            if (ext.contains("%")) {
                ext = ext.substring(0, ext.indexOf("%"));
            }
            if (ext.contains("/")) {
                ext = ext.substring(0, ext.indexOf("/"));
            }
            return ext.toLowerCase();
        }
    }

    public static ArrayList<File> sort(ArrayList<File> files, String sortOrder, String sortBy, boolean folderFirst) {

        switch (sortOrder) {

            case SORT_ORDER_ASC:
                switch (sortBy) {
                    case SORT_BY_SIZE:
                        return sortBySize(files, SORT_ORDER_ASC, folderFirst);

                    case SORT_BY_LAST_MODIFIED:
                        return sortByLastModified(files, SORT_ORDER_ASC, folderFirst);

                    case SORT_BY_NAME:
                    default:
                        return sortByName(files, SORT_ORDER_ASC, folderFirst);
                }

            case SORT_ORDER_DESC:
                switch (sortBy) {
                    case SORT_BY_NAME:
                        return sortByName(files, SORT_ORDER_DESC, folderFirst);

                    case SORT_BY_LAST_MODIFIED:
                        return sortByLastModified(files, SORT_ORDER_DESC, folderFirst);

                    case SORT_BY_SIZE:
                    default:
                        return sortBySize(files, SORT_ORDER_DESC, folderFirst);
                }

            default:
                return sortByName(files, SORT_ORDER_ASC, folderFirst);
        }

    }

    private static ArrayList<File> sortByName(final ArrayList<File> files, final String sortOrder, final boolean folderFirst) {

        Collections.sort(files, new Comparator<File>() {
            @Override
            public int compare(File lhs, File rhs) {
                if(folderFirst) {
                    if (lhs.isDirectory() && !rhs.isDirectory()) return -1;
                    if (!lhs.isDirectory() && rhs.isDirectory()) return 1;
                }
                switch (sortOrder) {
                    case SORT_ORDER_DESC:
                        return rhs.getName().compareToIgnoreCase(lhs.getName());

                    case SORT_ORDER_ASC:
                    default:
                        return lhs.getName().compareToIgnoreCase(rhs.getName());
                }
            }
        });
        return files;
    }

    private static ArrayList<File> sortBySize(ArrayList<File> files, final String sortOrder, final boolean folderFirst) {

        Collections.sort(files, new Comparator<File>() {
            @Override
            public int compare(File lhs, File rhs) {

                if(folderFirst) {
                    if (lhs.isDirectory() && !rhs.isDirectory()) return -1;
                    if (!lhs.isDirectory() && rhs.isDirectory()) return 1;
                }

                long lhsLength = 0, rhsLength = 0, diff;

                if(lhs.isDirectory()) lhsLength = getFolderSize(lhs);
                if(lhs.isFile()) lhsLength = lhs.length();
                if(rhs.isDirectory()) rhsLength = getFolderSize(rhs);
                if(rhs.isFile()) rhsLength = rhs.length();

                switch (sortOrder) {
                    case SORT_ORDER_DESC:
                        diff = rhsLength - lhsLength;
                        break;
                    case SORT_ORDER_ASC:
                    default:
                        diff = lhsLength - rhsLength;
                        break;
                }

                if (diff < 0) return -1;
                else if (diff > 0) return 1;
                else return 0;
            }
        });
        return files;
    }

    private static ArrayList<File> sortByLastModified(ArrayList<File> files, final String sortOrder, final boolean foldersFirst) {

        Collections.sort(files, new Comparator<File>() {
            @Override
            public int compare(File lhs, File rhs) {

                if (foldersFirst){
                    if(lhs.isDirectory() && !rhs.isDirectory())  return -1;
                    if(!lhs.isDirectory() && rhs.isDirectory()) return 1;
                }

                long diff;
                switch (sortOrder) {
                    case SORT_ORDER_DESC:
                        diff = rhs.lastModified() - lhs.lastModified();
                        break;

                    case SORT_ORDER_ASC:
                    default:
                        diff = lhs.lastModified() - rhs.lastModified();
                        break;
                }

                if (diff < 0) return -1;
                else if (diff > 0) return 1;
                else return 0;
            }
        });
        return files;
    }

    public static long getFolderSize(File file) {

        long size = 0;
        for (File f : file.listFiles()) {
            if (f.isFile()) size += f.length();
            else size += getFolderSize(f);
        }
        return size;
    }

    public static boolean isHiddenFile(File file) {
        return file.getName().startsWith(".");
    }

    public static ArrayList<File> removeHiddenFiles(ArrayList<File> files) {
        ArrayList<File> list = new ArrayList<>();

        for (File file : files) {
            if (!isHiddenFile(file))
                list.add(file);
        }
        return list;
    }
}
