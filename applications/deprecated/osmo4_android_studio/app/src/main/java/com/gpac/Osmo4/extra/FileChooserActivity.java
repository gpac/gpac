/**
 * $URL$
 * <p/>
 * $LastChangedBy$ - $LastChangedDate$
 */
package com.gpac.Osmo4.extra;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.FragmentManager;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.view.Menu;
import android.view.MenuItem;

import com.gpac.Osmo4.R;


public class FileChooserActivity extends Activity {

    final String FILE_CHOOSER_FRAGMENT = "fileChooserFragment";
    public final static String TITLE_PARAMETER = "org.openintents.extra.TITLE";

    private FragmentManager fm;
    private FileChooserFragment fileChooserFragment;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_file_chooser);

        requestForPermission();
        fm = getFragmentManager();

        if (fm.findFragmentById(R.id.fileChooserLayout) == null) {
            fileChooserFragment = new FileChooserFragment();
            fm.beginTransaction()
                    .add(R.id.fileChooserLayout, fileChooserFragment, FILE_CHOOSER_FRAGMENT)
                    .addToBackStack(FILE_CHOOSER_FRAGMENT)
                    .commit();
        }
    }

    private void requestForPermission() {

        if (ContextCompat.checkSelfPermission(FileChooserActivity.this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {

            if (ActivityCompat.shouldShowRequestPermissionRationale(FileChooserActivity.this,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE)) {

                promptForPermissionsDialog(getString(R.string.requestPermissionStorage), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        ActivityCompat.requestPermissions(FileChooserActivity.this,
                                new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                                100);
                    }
                });
            } else {
                ActivityCompat.requestPermissions(FileChooserActivity.this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        100);
            }
        }
    }

    private void promptForPermissionsDialog(String message, DialogInterface.OnClickListener onClickListener) {
        new AlertDialog.Builder(FileChooserActivity.this)
                .setMessage(message)
                .setPositiveButton(getString(R.string.yes), onClickListener)
                .setNegativeButton(getString(R.string.no), null)
                .create()
                .show();
    }

    @Override
    public void onBackPressed() {
        fileChooserFragment.backPressed();
    }


    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return super.onOptionsItemSelected(item);
    }

}