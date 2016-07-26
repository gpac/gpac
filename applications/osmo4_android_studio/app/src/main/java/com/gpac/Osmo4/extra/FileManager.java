package com.gpac.Osmo4.extra;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;


public class FileManager {

    public static final String SORT_ORDER_ASC = "ASC";
    public static final String SORT_ORDER_DESC = "DESC";
    public static final String SORT_ORDER_FOLDERS_FIRST = "NONE";

    public static final String SORT_BY_NAME = "NAME";
    public static final String SORT_BY_SIZE = "SIZE";

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

    public static ArrayList<File> sort(ArrayList<File> files, String sortOrder, String sortBy) {

        switch (sortOrder) {

            case SORT_ORDER_ASC:
                switch (sortBy) {
                    case SORT_BY_NAME:
                        return sortByName(files, SORT_ORDER_ASC);
                    case SORT_BY_SIZE:
                        return sortBySize(files, SORT_ORDER_ASC);
                    default:
                        return sortByName(files, SORT_ORDER_ASC);
                }

            case SORT_ORDER_DESC:
                switch (sortBy) {
                    case SORT_BY_NAME:
                        return sortByName(files, SORT_ORDER_DESC);
                    case SORT_BY_SIZE:
                        return sortBySize(files, SORT_ORDER_DESC);
                    default:
                        return sortBySize(files, SORT_ORDER_ASC);
                }

            case SORT_ORDER_FOLDERS_FIRST:
                return sortFoldersFirst(files, SORT_BY_NAME);
            default:
                return sortByName(files, SORT_ORDER_ASC);
        }

    }

    private static ArrayList<File> sortByName(ArrayList<File> files, final String sortOrder) {

        Collections.sort(files, new Comparator<File>() {
            @Override
            public int compare(File lhs, File rhs) {

                if (sortOrder.equals(SORT_ORDER_ASC))
                    return lhs.getName().compareToIgnoreCase(rhs.getName());
                else return rhs.getName().compareToIgnoreCase(lhs.getName());
            }
        });
        return files;
    }

    private static ArrayList<File> sortBySize(ArrayList<File> files, final String sortOrder) {

        Collections.sort(files, new Comparator<File>() {
            @Override
            public int compare(File lhs, File rhs) {
                long diff;
                if (sortOrder.equals(SORT_ORDER_ASC)) {
                    diff = lhs.length() - rhs.length();
                } else diff = rhs.length() - lhs.length();

                if (diff < 0)
                    return -1;
                else if (diff > 0)
                    return 1;
                else return 0;

            }
        });
        return files;
    }

    private static ArrayList<File> sortFoldersFirst(ArrayList<File> fileList, String sortBy) {
        ArrayList<File> dirs = new ArrayList<>();
        ArrayList<File> files = new ArrayList<>();

        for (File f : fileList) {
            if (f.isDirectory())
                dirs.add(f);
            else files.add(f);
        }
        dirs = sortBy.equals(SORT_BY_SIZE) ? sortBySize(dirs, SORT_ORDER_ASC) : sortByName(dirs, SORT_ORDER_ASC);
        files = sortBy.equals(SORT_BY_SIZE) ? sortBySize(files, SORT_ORDER_ASC) : sortByName(files, SORT_ORDER_ASC);
        dirs.addAll(files);
        return dirs;
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