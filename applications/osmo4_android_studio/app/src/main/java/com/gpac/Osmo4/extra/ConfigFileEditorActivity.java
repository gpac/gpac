package com.gpac.Osmo4.extra;

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

import com.gpac.Osmo4.R;


public class ConfigFileEditorActivity extends Activity {

    private String TAG = "ConfigFileEditorActivity";

    private AutoCompleteTextView optionsTextView;
    private TextView typeTextView;
    private TextView dfaultTextView;
    private TextView usageTextView;
    private TextView descriptionTextView;
    private TextView currentValueTextView;
    private EditText newValueEditText;
    private Button saveButton;
    private String currentValue;

    private ConfigFileEditor editor;
    private ConfigFileEditor.Attribute attribute;
    private String currentName;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_config_file_editor);

        optionsTextView = (AutoCompleteTextView) findViewById(R.id.optionsTextView);
        typeTextView = (TextView) findViewById(R.id.typeTextView);
        dfaultTextView = (TextView) findViewById(R.id.dfaultTextView);
        usageTextView = (TextView) findViewById(R.id.usageTextView);
        descriptionTextView = (TextView) findViewById(R.id.descriptionTextView);
        currentValueTextView = (TextView) findViewById(R.id.currentValueEditText);
        newValueEditText = (EditText) findViewById(R.id.newValueEditText);
        saveButton = (Button) findViewById(R.id.saveButton);

        descriptionTextView.setMovementMethod(new ScrollingMovementMethod());
        editor = new ConfigFileEditor(this);
        final String[]keys = editor.getAllKeys();
        final ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                android.R.layout.simple_dropdown_item_1line, keys);
        optionsTextView.setAdapter(adapter);

        optionsTextView.setOnClickListener(onOptionTextViewClick);
        optionsTextView.setOnItemClickListener(onItemClick);
        optionsTextView.setOnKeyListener(keyListener);
        saveButton.setOnClickListener(onSaveButtonClick);
    }

    private View.OnClickListener onOptionTextViewClick = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            optionsTextView.showDropDown();
        }
    };

    private AdapterView.OnItemClickListener onItemClick = new AdapterView.OnItemClickListener() {
        @Override
        public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            setAllFields((String) parent.getItemAtPosition(position));
        }
    };

    private View.OnKeyListener keyListener = new View.OnKeyListener() {
        @Override
        public boolean onKey(View v, int keyCode, KeyEvent event) {
            if(event.getAction() == KeyEvent.ACTION_DOWN &&
                    keyCode == KeyEvent.KEYCODE_ENTER) {
                currentName = optionsTextView.getText().toString().trim();
                if (currentName == null || currentName.equals("")) {
                    return false;
                } else {
                    optionsTextView.dismissDropDown();
                    setAllFields(currentName);
                    return true;
                }
            }
            return false;
        }
    };

    private View.OnClickListener onSaveButtonClick = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            String newValue = newValueEditText.getText().toString().trim();
            if(newValue == null || newValue.equals("")) {
                Toast.makeText(ConfigFileEditorActivity.this, "New value is empty", Toast.LENGTH_LONG).show();
                return;
            }

            if(currentValue == null){
                Log.e(TAG, getString(R.string.noKey));
                return;
            }

            if (newValue.equals(currentValue)){
                Toast.makeText(ConfigFileEditorActivity.this, getString(R.string.noChange), Toast.LENGTH_LONG).show();
                return;
            }

            if(editor.isValueValid(currentName, newValue)){
                editor.setValue(currentName, newValue);
                setAllFields(currentName);
                Toast.makeText(ConfigFileEditorActivity.this, getString(R.string.valueUpdated), Toast.LENGTH_LONG).show();
                newValueEditText.setText("");
            } else {
                Toast.makeText(ConfigFileEditorActivity.this, getString(R.string.valueNotValid), Toast.LENGTH_LONG).show();
                Log.e(TAG, getString(R.string.valueNotValid));
            }

        }
    };

    private void setAllFields(String key) {
        currentName = key;
        attribute = editor.getAllAttributesForKey(key);
        if(attribute == null) {
            Log.v(TAG, "no attribute found for the given key in the option file : " + key);
            typeTextView.setText(getString(R.string.noAttribute));
            dfaultTextView.setText(getString(R.string.noAttribute));
            usageTextView.setText(getString(R.string.noAttribute));
            descriptionTextView.setText(getString(R.string.noAttribute));
        }
         else {
            typeTextView.setText(attribute.getType());
            dfaultTextView.setText(attribute.getDefault());
            usageTextView.setText(attribute.getUsage());
            descriptionTextView.setText(Html.fromHtml(attribute.getDesciption(), null, new XMLTagHandler()));
            Log.v(TAG, "description " + attribute.getDesciption());
        }
        currentValue = editor.getValue(key);
        if (currentValue == null) currentValueTextView.setText(getString(R.string.noKey)); else currentValueTextView.setText(currentValue);
    }
}