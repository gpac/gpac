package com.gpac.Osmo4.extra;

import android.Manifest;
import android.app.ActionBar;
import android.app.Activity;
import android.app.Fragment;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.annotation.Nullable;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import com.gpac.Osmo4.R;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;

public class FileChooserFragment extends Fragment {

    private final String PREF_SHOW_HIDDEN_FILES = "SHOW_HIDDEN_FILES";
    private final String PREF_SORT_ORDER = "SORT_ORDER";
    private final String PREF_SORT_BY = "SORT_BY";
    private final String PREF_FOLDERS_FIRST = "FOLDERS_FIRST";

    private File currDir;
    private ArrayList<File> files;
    private FileArrayAdapter adapter;
    private SharedPreferences prefs;
    private ActionBar actionBar;

    private boolean showHiddenFiles;
    private String sortOrder;
    private String sortBy;
    private boolean foldersFirst;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (ContextCompat.checkSelfPermission(getActivity(), Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED)
            currDir = new File("/");

        else currDir = Environment.getExternalStorageDirectory();
    }

    @Nullable
    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.fragment_file_chooser, container, false);
        setRetainInstance(true);

        RecyclerView recyclerView = (RecyclerView) view.findViewById(R.id.recyclerView);
        GridLayoutManager gridLayoutManager = new GridLayoutManager(getActivity(), 2);
        gridLayoutManager.setOrientation(GridLayoutManager.VERTICAL);
        files = new ArrayList<>();
        actionBar = getActivity().getActionBar();
        adapter = new FileArrayAdapter(files, onItemClickListenerCallback, getActivity());
        prefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
        setPreferences();
        recyclerView.setLayoutManager(gridLayoutManager);
        populateList(currDir);
        recyclerView.setAdapter(adapter);
        return view;
    }

    @Override
    public void onResume() {
        super.onResume();
        setPreferences();
    }

    private void setPreferences() {
        showHiddenFiles = prefs.getBoolean(PREF_SHOW_HIDDEN_FILES, true);
        sortOrder = prefs.getString(PREF_SORT_ORDER, FileManager.SORT_ORDER_ASC);
        sortBy = prefs.getString(PREF_SORT_BY, FileManager.SORT_BY_NAME);
        foldersFirst = prefs.getBoolean(PREF_FOLDERS_FIRST, true);
    }

    public void backPressed() {
        if (currDir.getAbsolutePath().equals("/") || !currDir.getParentFile().canRead())
            getActivity().finish();
        else populateList(currDir.getParentFile());
    }

    private FileArrayAdapter.OnItemClickListener onItemClickListenerCallback = new FileArrayAdapter.OnItemClickListener() {
        @Override
        public void onItemClick(View view, int position) {
            File f = adapter.getItem(position);
            if (f.isDirectory())
                onDirectoryClick(f);
            else if (f.isFile())
                onFileClick(f);
        }
    };

    private void onFileClick(File f) {
        Intent data = new Intent();
        data.setData(Uri.fromFile(f));
        getActivity().setResult(Activity.RESULT_OK, data);
        getActivity().finish();
    }

    private void onDirectoryClick(File f) {
        if (f.canRead())
            populateList(f);
        else Toast.makeText(getActivity(), "Cannot read " + f, Toast.LENGTH_LONG).show();
    }

    public void populateList(File file) {
        new BackgroundWork(file).execute();
    }

    class BackgroundWork extends AsyncTask<File, String, Boolean> {

        private File file;
        public BackgroundWork(File file) {
            this.file = file;
        }

        @Override
        protected Boolean doInBackground(File... params) {

            if (!file.canRead()) {
                Toast.makeText(getActivity(), "Directory cannot be read", Toast.LENGTH_LONG).show();
                return false;
            }
            files.clear();
            ArrayList<File> list = new ArrayList<>(Arrays.asList(file.listFiles()));

            files.addAll(
                    FileManager.sort(
                            showHiddenFiles? list : FileManager.removeHiddenFiles(list),
                            sortOrder,
                            sortBy,
                            foldersFirst
                    ));
            currDir = file;
            return true;
        }

        @Override
        protected void onPostExecute(Boolean aVoid) {
            actionBar.setTitle(file.getAbsolutePath());
            adapter.notifyDataSetChanged();
        }
    }
}