package io.gpac.gpac.extra;

import android.app.Activity;
import android.os.Bundle;
import android.text.Html;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import android.view.ViewGroup;
import android.content.Intent;

import io.gpac.gpac.R;


public class URLAuthenticate extends Activity {

    private String TAG = "URLAuthenticate";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_url_authenticate);
        getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        this.setFinishOnTouchOutside(false);

		TextView txt_url = (TextView) findViewById(R.id.authURL);

		Intent intent = getIntent();
		txt_url.setText("URL " + intent.getStringExtra("URL"));

		Button b_ok = (Button) findViewById(R.id.ok);
		b_ok.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				Intent intent=new Intent();

				EditText ed_txt = (EditText) findViewById(R.id.userName);
				intent.putExtra("USER", ed_txt.getText().toString() );

				ed_txt = (EditText) findViewById(R.id.password);
				intent.putExtra("PASSWD", ed_txt.getText().toString() );

				setResult(RESULT_OK, intent);
				finish();
			}
		});

		Button b_cancel = (Button) findViewById(R.id.cancel);
		b_cancel.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				setResult(RESULT_CANCELED);
				finish();
			}
		});
    }
}
