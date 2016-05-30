/**
 * $URL$
 *
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.gpac.Osmo4.extra;

import java.io.File;
import java.util.List;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.TextView;
import com.gpac.Osmo4.R;

/**
 * Class that contains the list of file for given directory
 * 
 * @version $Revision$
 * 
 */
public class FileArrayAdapter extends ArrayAdapter<FileEntry> {

    private final Context context;

    private final int id;

    private final List<FileEntry> items;

    /**
     * Constructor
     * 
     * @param context
     * @param textViewResourceId
     * @param objects
     */
    public FileArrayAdapter(Context context, int textViewResourceId, List<FileEntry> objects) {
        super(context, textViewResourceId, objects);
        this.context = context;
        id = textViewResourceId;
        items = objects;
    }

    @Override
    public FileEntry getItem(int i) {
        return items.get(i);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View v = convertView;
        if (v == null) {
            LayoutInflater vi = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            v = vi.inflate(id, null);
        }
        final FileEntry o = items.get(position);
        if (o != null) {
            TextView t1 = (TextView) v.findViewById(R.id.TextView01);
            TextView t2 = (TextView) v.findViewById(R.id.TextView02);

            if (t1 != null)
                t1.setText(o.getName());
            if (t2 != null) {
                File f = o.getFile();
                if (f.isDirectory())
                    t2.setText(context.getString(R.string.directory));
                else
                    t2.setText(context.getString(R.string.fileSize, f.length()));
            }
        }
        return v;
    }

}
