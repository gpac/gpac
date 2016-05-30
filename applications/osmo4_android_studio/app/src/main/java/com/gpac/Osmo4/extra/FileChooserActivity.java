/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.gpac.Osmo4.extra;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import android.app.ListActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.ListView;
import com.gpac.Osmo4.R;

/**
 * @version $Revision$
 * 
 */
public class FileChooserActivity extends ListActivity {

    private File currentDir;

    private FileArrayAdapter adapter;

    /**
     * The parameter name to use to search for title
     */
    public final static String TITLE_PARAMETER = "org.openintents.extra.TITLE"; //$NON-NLS-1$

    private String customTitle;

    private void updateTitle(File currentPath) {
        setTitle(getResources().getString(R.string.selectFileTitlePattern, customTitle, currentPath.getAbsolutePath()));
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent intent = getIntent();
        currentDir = new File("/"); //$NON-NLS-1$
        this.customTitle = getResources().getString(R.string.selectFileDefaultTitle);
        if (intent != null) {
            if (intent.getData() != null) {
                File f = new File(intent.getData().getPath());
                if (f.exists() && f.isDirectory() && f.canRead())
                    currentDir = f;
            }
            String title = intent.getExtras().getString(TITLE_PARAMETER);
            if (title != null)
                this.customTitle = title;
        }
        fillList(currentDir);
    }

    private void fillList(File f) {
        File[] dirs = f.listFiles();
        updateTitle(f);
        List<FileEntry> dir = new ArrayList<FileEntry>();
        List<FileEntry> fls = new ArrayList<FileEntry>();
        if (dirs != null) {
            for (File ff : dirs) {
                dir.add(new FileEntry(ff.getAbsoluteFile()));
            }
            Collections.sort(dir);
            Collections.sort(fls);
            dir.addAll(fls);
        }
        if (f.getParentFile() != null)
            dir.add(0, new FileEntry(f.getParentFile(), getResources().getString(R.string.parentDirectory)));
        adapter = new FileArrayAdapter(this, R.layout.file_view, dir);
        this.setListAdapter(adapter);
    }

    @Override
    protected void onListItemClick(ListView l, View v, int position, long id) {
        super.onListItemClick(l, v, position, id);
        FileEntry o = adapter.getItem(position);
        if (o.getFile().isDirectory()) {
            fillList(o.getFile());
        } else {
            onFileClick(o);
        }
    }

    private void onFileClick(FileEntry o) {
        Intent data = new Intent();
        data.setData(Uri.fromFile(o.getFile()));
        setResult(RESULT_OK, data);
        finish();
    }

}
