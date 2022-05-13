package io.gpac.gpac.extra;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.support.v4.content.ContextCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.widget.CardView;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import io.gpac.gpac.R;

import java.io.File;
import java.util.ArrayList;
import java.util.Date;


public class FileArrayAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

    public interface OnItemClickListener {
        void onItemClick(View view, int position);
    }

    private ArrayList<File> files;
    private OnItemClickListener onItemClickListener;
    private Context context;

    public FileArrayAdapter(ArrayList<File> files, OnItemClickListener onItemClickListener, Context context) {
        this.files = files;
        this.onItemClickListener = onItemClickListener;
        this.context = context;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        return new ListItemViewHolder(
                        LayoutInflater
                                .from(parent.getContext())
                                .inflate(R.layout.list_item_file_chooser, parent, false));
    }

    @Override
    public void onBindViewHolder(final RecyclerView.ViewHolder viewHolder, final int position) {

        ListItemViewHolder holder = (ListItemViewHolder) viewHolder;
        File file = getItem(position);
        holder.title.setText(file.getName());
        holder.lastModified.setText(new Date(file.lastModified()).toString());
        setIcon(file, holder);
        holder.cardView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onItemClickListener.onItemClick(v, position);
            }
        });
    }

    public void setIcon(File file, ListItemViewHolder holder) {

        String extension;
        Drawable drawable = null;

        try {
            extension = FileManager.getExtension(file.getAbsolutePath());

            if (file.isFile()) {
                switch (extension) {

                    case ".c":  case ".cpp":    case ".doc":    case ".docx":   case ".exe":
                    case ".h":  case ".html":   case ".java":   case ".log":    case ".txt":
                    case ".pdf":    case ".ppt":    case ".xls":
                        drawable = ContextCompat.getDrawable(context, R.drawable.ic_file);
                        break;

                    case ".3ga":    case ".aac":    case ".mp3":    case ".m4a":    case ".ogg":
                    case ".wav":    case ".wma":
                        drawable = ContextCompat.getDrawable(context, R.drawable.ic_audio);
                        break;

                    case ".3gp":    case ".avi":    case ".mpg":    case ".mpeg":   case ".mp4":
                    case ".mkv":    case ".webm":   case ".wmv":    case ".vob":
                        drawable = ContextCompat.getDrawable(context, R.drawable.ic_video);
                        break;

                    case ".ai": case ".bmp":    case ".exif":   case ".gif":    case ".jpg":
                    case ".jpeg":   case ".png":    case ".svg":
                        drawable = ContextCompat.getDrawable(context, R.drawable.ic_image);
                        break;

                    case ".rar":    case ".zip":    case ".ZIP":
                        drawable = ContextCompat.getDrawable(context, R.drawable.ic_compressed);
                        break;

                    default:
                        drawable = ContextCompat.getDrawable(context, R.drawable.ic_error);
                        break;
                }
            } else if (file.isDirectory()) {
                drawable = ContextCompat.getDrawable(context, R.drawable.ic_folder);
            } else drawable = ContextCompat.getDrawable(context, R.drawable.ic_error);

        } catch (Exception e) {
            drawable = ContextCompat.getDrawable(context, R.drawable.ic_error);
        }
        drawable = DrawableCompat.wrap(drawable);
        holder.icon.setImageDrawable(drawable);
    }

    @Override
    public int getItemCount() {
        return files.size();
    }

    public File getItem(int position) {
        return files.get(position);
    }

    static class ListItemViewHolder extends RecyclerView.ViewHolder {

        CardView cardView;
        TextView title;
        TextView lastModified;
        ImageView icon;
        LinearLayout linearLayout;

        public ListItemViewHolder(View itemView) {
            super(itemView);
            cardView = (CardView) itemView.findViewById(R.id.cardView);
            title = (TextView) itemView.findViewById(R.id.title);
            icon = (ImageView) itemView.findViewById(R.id.icon);
            linearLayout = (LinearLayout) itemView.findViewById(R.id.linearLayout);
            lastModified = (TextView) itemView.findViewById(R.id.lastModified);
        }
    }

    public ArrayList<File> getFiles() {
        return files;
    }
}
