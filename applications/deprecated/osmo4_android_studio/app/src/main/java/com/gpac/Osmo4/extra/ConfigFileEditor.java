package com.gpac.Osmo4.extra;

import android.content.Context;
import android.util.Log;

import com.gpac.Osmo4.GpacConfig;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;


public class ConfigFileEditor {

    private String TAG = "ConfigFileEditor";

    private String ATTRIBUTE_NAME = "name";
    private String ATTRIBUTE_TYPE = "type";
    private String ATTRIBUTE_DEFAULT = "default";
    private String ATTRIBUTE_USAGE = "usage";

    private final String TYPE_PATH = "path";
    private final String TYPE_FILENAME = "filename";
    private final String TYPE_STRING = "string";
    private final String TYPE_BOOLEAN = "boolean";
    private final String TYPE_UINT = "uint";

    private File configFile;

    private XmlPullParserFactory xmlFactoryObject;
    private XmlPullParser parser;
    private GpacConfig config;
    private Context context;

    public ConfigFileEditor(Context context) {
        this.context = context;
        config = new GpacConfig(context);
        configFile = new File(config.getGpacConfigDirectory(), "GPAC.cfg");

        try {
            xmlFactoryObject = XmlPullParserFactory.newInstance();
        } catch (XmlPullParserException e) {
            Log.v(TAG, "Cannot create xml parser new instance");
            e.printStackTrace();
        }
    }

    private void start() {
        InputStream inputStream = null;
        try {
            inputStream  = context.getAssets().open("configuration.xml");
            parser = xmlFactoryObject.newPullParser();
            parser.setInput(inputStream, null);
        } catch (XmlPullParserException | IOException e) {
            e.printStackTrace();
        }
    }

    /**
     *  get all the keys from the xml file
     *
     * @return array of all keys
     */
    public String[] getAllKeys() {
        ArrayList<String> list = new ArrayList<>();
        int event = 0;
        try {
            start();
            event = parser.getEventType();

            while (event != XmlPullParser.END_DOCUMENT) {
                String name = parser.getName();
                switch (event) {
                    case XmlPullParser.START_TAG:
                        if (name.equals("key")) {
                            String attrName = parser.getAttributeValue(null, ATTRIBUTE_NAME).trim();
                            list.add(attrName);
                        }
                        break;

                    case XmlPullParser.END_TAG:
                        break;
                }
                event = parser.next();
            }
        } catch (XmlPullParserException e) {
            Log.e(TAG, "XML Exception");
            e.printStackTrace();
        } catch (IOException e) {
            Log.e(TAG, "IOException");
            e.printStackTrace();
        }
        return list.toArray(new String[0]);
    }

    /**
     *  get all the attributes for a single key
     *
     * @param key key to find attributes for
     * @return Attribute object with all attributes set. If key not found, returns null
     */
    public Attribute getAllAttributesForKey(String key) {
        Attribute attribute = null;
        String section = null;
        String attrName = null;
        String attrType = null;
        String attrDfault = null;
        String attrUsage = null;
        StringBuilder attrDescription = new StringBuilder();

        boolean keyFound = false;
        boolean exit = false;
        int depth = 0;
        int event;
        try {
            start();                    //start from beginning
            event = parser.getEventType();
            while (event != XmlPullParser.END_DOCUMENT && !exit) {
                String name = parser.getName();
                switch (event) {
                    case XmlPullParser.START_TAG:

                        if(name.equals("section")){
                            section = parser.getAttributeValue(null, ATTRIBUTE_NAME);
                        }
                        if (name.equals("key") && parser.getAttributeValue(null, ATTRIBUTE_NAME).equals(key)) {
                            attrName = parser.getAttributeValue(null, ATTRIBUTE_NAME);
                            attrType = parser.getAttributeValue(null, ATTRIBUTE_TYPE);
                            attrDfault = parser.getAttributeValue(null, ATTRIBUTE_DEFAULT);
                            attrUsage = parser.getAttributeValue(null, ATTRIBUTE_USAGE);
                            keyFound = true;
                        }
                        if(keyFound)
                            ++depth;
                        if (keyFound && depth > 1){
                            attrDescription.append("<").append(parser.getName()).append(">");
                        }
                        break;

                    case XmlPullParser.TEXT:
                        if(keyFound) {
                            attrDescription.append(parser.getText().trim());
                        }
                        break;

                    case XmlPullParser.END_TAG:
                        if(keyFound)
                            --depth;

                        if (keyFound && depth > 0){
                            attrDescription.append("</").append(parser.getName()).append(">");
                        }

                        if(name.equals("key") && keyFound){
                            attribute = new Attribute(section, attrName, attrType, attrDfault, attrUsage, attrDescription.toString());
                            keyFound = false;
                            exit = true;
                        }
                        break;
                }
                event = parser.next();
            }
        } catch (XmlPullParserException e) {
            Log.e(TAG, "XML Exception");
            e.printStackTrace();
        } catch (IOException e) {
            Log.e(TAG, "IOException");
            e.printStackTrace();
        }
        return attribute;
    }


    /**
     * get value for key from the config file
     *
     * @param key key to find
     * @return the value from the config file, if key not found, returns null.
     */
    public String getValue(String key) {
        String line;
        try {
            BufferedReader br = new BufferedReader(new FileReader(configFile));

            while ((line = br.readLine()) != null) {
                String[] parts = line.split("=");
                if(parts[0].trim().equals(key)){
                    return parts[1];
                }
            }
            br.close();
        } catch (FileNotFoundException e) {
            Log.e(TAG, "GPAC.cfg not found");
            e.printStackTrace();
        } catch (IOException e) {
            Log.e(TAG, "IO Exception while parsing GPAC.cfg");
            e.printStackTrace();
        }
        return null;
    }

    /**
     *  checks if the new value is valid or not.
     * @param type type of newValue
     * @param newValue value to check
     * @return true if valid, otherwise false
     */
    public boolean isValueValid(String type, String newValue) {

        switch (type) {
          /*  case TYPE_PATH:
            case TYPE_FILENAME:
                File file = new File(newValue);
                if(!file.exists()){
                    Log.e(TAG, "The path does not exist : " + newValue);
                    return false;
                }
                break;
          */

            case TYPE_BOOLEAN:
                return newValue.equals("yes") || newValue.equals("no");

            case TYPE_UINT:
                try {
                    int num = Integer.parseInt(newValue);
                    return num >= 0;
                } catch(Exception e) {
                    Log.e(TAG, "The type is uint but value provided: "+ newValue);
                    return false;
                }

            case TYPE_STRING:
            default:
                return true;

        }
    }

    /**
     *   set the value in the config file
     * @param key key for newValue
     * @param newValue
     */
    public void setValue(String key, String newValue) {

        String line = null;
        try {
            File tempFile = new File(config.getGpacConfigDirectory(), "temp.cfg");
            BufferedReader br = new BufferedReader(new FileReader(configFile));
            BufferedWriter bw = new BufferedWriter(new FileWriter(tempFile));

            while ((line = br.readLine()) != null) {
                String parts[] = line.split("=");
                if(parts[0].equals(key)){
                    line = key + "=" + newValue;
                }
                bw.write(line);
                bw.newLine();
            }
            br.close();
            bw.close();

            if(!configFile.delete()) {
                Log.e(TAG, "Cannot delete config file");
            }

            if(!tempFile.renameTo(configFile)) {
                Log.e(TAG, "Cannot rename temp file to GPAC.cfg");
            }

        } catch (IOException e) {
            Log.e(TAG, "IOException ");
            e.printStackTrace();
        }
    }

    class Attribute {
        private String section;
        private String name;
        private String type;
        private String dfault;
        private String usage;
        private String desciption;

        public Attribute(String section, String name, String type, String dfault, String usage, String desciption) {
            this.section = section;
            this.name = name;
            this.type = type;
            this.dfault = dfault;
            this.usage = usage;
            this.desciption = desciption;
        }

        public String getName() {
            return name;
        }

        public String getType() {
            return type;
        }

        public String getDefault() {
            return dfault;
        }

        public String getUsage() {
            return usage;
        }

        public String getDesciption() {
            return desciption;
        }
    }

}
