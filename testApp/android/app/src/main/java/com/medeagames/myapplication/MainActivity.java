package com.medeagames.myapplication;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import org.json.*;
import org.w3c.dom.Text;

public class MainActivity extends AppCompatActivity {
    // references to view objects
    private EditText mTimesliceTextWidget;
    private CheckBox mRepeatCheckboxWidget;
    private LinearLayout mPatternGridWidget;

    public static final int MAX_CHANNELS = 8;

    private int mTimesliceMs   = 500; // XXX get from value in resources for default timeslicetext
    private int mControllerId  = 0;    // NB - not currently controllable...
    private boolean mIsRunning = false; // XXX add some sort of UI that says that this is running
    private int mNumChannels   = 3;    // XXX make this selectable
    private UsbBroadcastReceiver usbReceiver = null;
    private File mDebugDir     = null;


    // service to talk to the serial cable
    static private SerialService serialInterface = null;

    @Override
    protected void onPause() {
        super.onPause();
        /*
        unregisterReceiver(usbReceiver);
        */
    }

    @Override
    protected void onResume() {
        super.onResume();
        /*
        Intent attachIntent = this.registerReceiver(usbReceiver, new IntentFilter("android.hardware.usb.action.USB_DEVICE_ATTACHED"));
        Intent detachIntent = this.registerReceiver(usbReceiver, new IntentFilter("android.hardware.usb.action.USB_DEVICE_DETACHED"));
        */
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        Log.i("FLG_POOFER", "Poofer start");
        setupDebugDirs();

        usbReceiver = new UsbBroadcastReceiver();

        writeToFile("Creating");

        if (serialInterface == null ){
            serialInterface = SerialService.getSerialService(this.getApplicationContext());
        }

        mTimesliceTextWidget  = (EditText)findViewById(R.id.speed_ms);
        mRepeatCheckboxWidget = (CheckBox)findViewById(R.id.repeatCheckbox);
        mPatternGridWidget    = (LinearLayout)findViewById(R.id.patternGrid);

        mTimesliceTextWidget.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    Log.e("FLG_POOFER", "After text changed");
                    int newTimeslice = -1;
                    String newTimesliceStr = view.getText().toString();

                    // check old vs new values to avoid infinite recursion.
                    if (!String.valueOf(mTimesliceMs).equals(newTimesliceStr)) {
                        try {
                            newTimeslice = Integer.parseInt(newTimesliceStr);
                        } catch (NumberFormatException e) {
                            Log.e("FLG_POOFER", "Invalid value in timeslice");
                        }

                        if (newTimeslice < 10 || newTimeslice > 20000) {
                            Log.e("FLG_POOFER", "Timeslice number out of bounds");
                        } else {
                            mTimesliceMs = newTimeslice;
                        }

                        mTimesliceTextWidget.setText(Integer.toString(mTimesliceMs));
                    }

                    // set focus to parent rather than this text entry - should prevent keyboard
                    // from popping up if the screen is rotated
                    ((View)view.getParent()).requestFocus();

                }
                return false;
            }
        });

        if (savedInstanceState != null) {
            updateViewFromBundle(savedInstanceState);
        } else {
            SharedPreferences prefs = getPreferences(MODE_PRIVATE);
            updateViewFromPreferences(prefs);
        }


        if (!(Thread.getDefaultUncaughtExceptionHandler() instanceof CustomExceptionHandler)) {
            Thread.setDefaultUncaughtExceptionHandler(new CustomExceptionHandler(
                    mDebugDir + "flg_main"));
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    // Send pattern to serial interface
    public void startPattern(View view){
        boolean repeat = mRepeatCheckboxWidget.isChecked();
        SignalRequest request = new SignalRequest(false, mControllerId, mTimesliceMs, repeat);

        // go through all the rows, adding the signals
        for (int i = 0; i< mPatternGridWidget.getChildCount(); i++) {
            View child1 = mPatternGridWidget.getChildAt(i);
            if (child1.getClass() == TimesliceRow.class) {
                ArrayList<Boolean> signals = new ArrayList<>();
                LinearLayout patternRow = (LinearLayout)((TimesliceRow)child1).getChildAt(0);  // XXX this will not be necessary when I fix the inflate
                // now iterate through these children, looking for SingleColorViews
                int maxColumns = Math.min(patternRow.getChildCount(), mNumChannels);
                for (int j=0; j<maxColumns; j++ ){
                    View child2 = patternRow.getChildAt(j);
                    Class classChild2 = child2.getClass();
                    if (classChild2 == SingleColorView.class) {
                        SingleColorView patternCell = (SingleColorView)child2;
                        signals.add(patternCell.isOn());
                    }
                }
                request.addSignals(signals);
            }
        }
        savePreferences(); // ideally I should do this every time data changes XXX
        serialInterface.sendSignalRequest(request);
    }

    public void stopPattern(View view) {
        boolean repeat = mRepeatCheckboxWidget.isChecked();
        SignalRequest request = new SignalRequest(true, 0, mTimesliceMs, repeat);
        serialInterface.sendSignalRequest(request);
    }

    public void addRow(View view) {
        // climb up to row view
        TimesliceRow currentRow = getCurrentRow(view);
        TimesliceRow newRow = new TimesliceRow(this, mNumChannels, true);
        ViewGroup parent = (ViewGroup)currentRow.getParent();
        if (parent != null) {
            int currentIdx = parent.indexOfChild(currentRow);
            parent.addView(newRow, currentIdx+1);
        }

    }

    public void deleteRow(View view) {
        // climb up to row view... which in this case is a linear layout, but I should subclass...
        // Or do something with the id to let me know it's the *right* linear layout. XXX
        TimesliceRow row = getCurrentRow(view);
        if (row != null) {
            ((ViewGroup) row.getParent()).removeView(row);
        }
      }

    private TimesliceRow getCurrentRow(View view) {
        ViewParent parentView = view.getParent();
        View child = view;
        while (parentView != null ){
            Class parentClass = parentView.getClass();
            if (parentClass == TimesliceRow.class) {
                return (TimesliceRow) parentView;
            }
            child = (View)parentView;
            parentView = child.getParent();
        }
        return null;
    }


    // brute force method of getting pattern data... This does not
    // scale well and something better should probably be done. XXX
    private ArrayList<ArrayList<Boolean>> getPatternData(){
        ArrayList<ArrayList<Boolean>> pattern = new ArrayList<>();
        for (int i = 0; i< mPatternGridWidget.getChildCount(); i++) {
            View child1 = mPatternGridWidget.getChildAt(i);
            if (child1 != null && child1.getClass() == TimesliceRow.class) {
                ArrayList<Boolean> signals = new ArrayList<>();
                child1 = ((TimesliceRow)child1).getChildAt(0); // this should not be necessary when I inflate the timeslice row correctly
                LinearLayout patternRow = (LinearLayout) child1;
                // now iterate through these children, looking for SingleColorViews
                for (int j = 0; j < patternRow.getChildCount(); j++) {
                    View child2 = patternRow.getChildAt(j);
                    Class classChild2 = child2.getClass();
                    if (classChild2 == SingleColorView.class) {
                        SingleColorView patternCell = (SingleColorView) child2;
                        signals.add(patternCell.isOn());
                    }
                }
                pattern.add(signals);
            }
        }
        return pattern;
    }


    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        super.onSaveInstanceState(savedInstanceState);

        if (savedInstanceState == null) {
            return;
        }

        // easy values first...
        savedInstanceState.putBoolean("RepeatValue", mRepeatCheckboxWidget.isChecked());
        savedInstanceState.putInt("ControllerId", mControllerId);
        savedInstanceState.putInt("NumberChannels", mNumChannels);
        savedInstanceState.putInt("TimesliceMs", mTimesliceMs);

        // now the pattern data
        ArrayList<ArrayList<Boolean>> patternData = getPatternData();
        JSONArray patternDataJson = new JSONArray();
        for (ArrayList<Boolean> timeslice : patternData) {
            JSONArray timesliceJson = new JSONArray();
            for (Boolean onOff : timeslice) {
                timesliceJson.put(onOff);
            }
            patternDataJson.put(timesliceJson);
        }
        Log.e("FLG_POOFER", "PatternData stored is " + patternDataJson.toString());

        savedInstanceState.putString("PatternData", patternDataJson.toString());
    }

    @Override
    public void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        updateViewFromBundle(savedInstanceState);
    }

    private void savePreferences()
    {
        SharedPreferences prefs = getPreferences(MODE_PRIVATE);
        if (prefs == null) {
            return;
        }

        SharedPreferences.Editor editor = prefs.edit();

        // easy values first...
        editor.putBoolean("RepeatValue", mRepeatCheckboxWidget.isChecked());
        editor.putInt("ControllerId", mControllerId);
        editor.putInt("NumberChannels", mNumChannels);
        editor.putInt("TimesliceMs", mTimesliceMs);

        // now the pattern data
        ArrayList<ArrayList<Boolean>> patternData = getPatternData();
        JSONArray patternDataJson = new JSONArray();
        for (ArrayList<Boolean> timeslice : patternData) {
            JSONArray timesliceJson = new JSONArray();
            for (Boolean onOff : timeslice) {
                timesliceJson.put(onOff);
            }
            patternDataJson.put(timesliceJson);
        }
        Log.e("FLG_POOFER", "PatternData stored is " + patternDataJson.toString());

        editor.putString("PatternData", patternDataJson.toString());
        editor.commit();

    }

    private void updateViewFromPreferences(SharedPreferences prefs){
        if (prefs != null) {
            boolean repeat = prefs.getBoolean("RepeatValue", false);
            mControllerId = prefs.getInt("ControllerId", 0);
            mNumChannels = prefs.getInt("NumberChannels", 4);
            mTimesliceMs = prefs.getInt("TimesliceMs", mTimesliceMs);
            String patternDataString = prefs.getString("PatternData", null);

            restoreState(repeat, patternDataString);
        }
    }

    private void updateViewFromBundle(Bundle savedInstanceState) {

        boolean repeat = false;
        String patternDataString = null;

        if (savedInstanceState != null) {
            repeat = savedInstanceState.getBoolean("RepeatValue", repeat);
            mControllerId = savedInstanceState.getInt("ControllerId", 0);   // XXX create static default
            mNumChannels = savedInstanceState.getInt("NumberChannels", 4);  // XXX create static default
            mTimesliceMs = savedInstanceState.getInt("TimesliceMs", mTimesliceMs);

            patternDataString = savedInstanceState.getString("PatternData", null);
            Log.e("FLG_POOFER", "PatternData retrieved is " + patternDataString);
            Log.e("FLG_POOFER", "num channels retrieved is " + mNumChannels);
         }

        restoreState(repeat, patternDataString);
    }

    private void restoreState(boolean repeat, String patternDataString)
    {
        ArrayList<ArrayList<Boolean>> patternData = null;

        if (patternDataString != null) {
            patternData = new ArrayList<ArrayList<Boolean>>();
            try {
                JSONArray timeslices = new JSONArray(patternDataString);
                for (int i = 0; i < timeslices.length(); i++) {
                    JSONArray timeslice = timeslices.getJSONArray(i);
                    ArrayList<Boolean> timesliceData = new ArrayList<>();
                    for (int j = 0; j < timeslice.length(); j++) {
                        Boolean onOff = timeslice.getBoolean(j);
                        timesliceData.add(onOff);
                    }
                    patternData.add(timesliceData);
                }
            } catch (org.json.JSONException jsonException) {
                /// XXX
            }
        }

        mRepeatCheckboxWidget.setChecked(repeat);
        mTimesliceTextWidget.setText(String.valueOf(mTimesliceMs));

        // hide all channel headers for unused channels
        if (mNumChannels <= 0) mNumChannels = 1;
        for (int i=MAX_CHANNELS; i>mNumChannels; i--) {
            int headerId = getResources().getIdentifier("ch" + i + "Header", "id", this.getPackageName());
            View header = findViewById(headerId);
            if (header != null) {
                header.setVisibility(View.GONE);
            }
        }

        // add pattern data... Remove any left over (XXX - this is necessary because I'm getting calls to both OnCreate and OnRestore when the user changes the perspective.
        // There's probably a better way to handle it.
        if (mPatternGridWidget.getChildCount() > 0) {
            mPatternGridWidget.removeAllViews();
        }
        if (patternData == null || patternData.size() <= 0) {
            TimesliceRow row = new TimesliceRow(this, mNumChannels, false);
            // XXX something about setting layout parameters?
            mPatternGridWidget.addView(row);
        } else {
            // walk pattern data. First one is special
            boolean first = true;
            for (ArrayList<Boolean> timeslice : patternData) {
                TimesliceRow row = new TimesliceRow(this, mNumChannels, !first);
                row.setPattern(timeslice);
                mPatternGridWidget.addView(row);
            }
        }
    }

    private void setupDebugDirs()
    {
        File appStorage = new File("/mnt/extsd/");
        if (appStorage == null || ! appStorage.exists()) {
            appStorage = this.getApplicationContext().getExternalFilesDir(null);
        }
        if (appStorage != null) {
            mDebugDir = new File(appStorage.getAbsolutePath() + "/flg");
            if (!mDebugDir.exists()) {
                boolean created = mDebugDir.mkdir();
                if (!created) {
                    Toast.makeText(this.getApplicationContext(), "Could not create debug dir", Toast.LENGTH_LONG).show();
                    mDebugDir = null;
                } else {
                    //Toast.makeText(this.getApplicationContext(), "Created debug dir", Toast.LENGTH_LONG).show();
                }
            } else if (!mDebugDir.canWrite()) {
                Toast.makeText(this.getApplicationContext(), "Cannot write to debug dir", Toast.LENGTH_LONG).show();
                mDebugDir = null;
            } else {
                //Toast.makeText(this.getApplicationContext(), "Debug dir exists, is writeable", Toast.LENGTH_LONG).show();
            }
        }
    }


    private void writeToFile(String bitsToWrite) {
        if (mDebugDir != null) {
            try {
                File file = new File(mDebugDir, "dbg");
                BufferedWriter bos = new BufferedWriter(new FileWriter(file));
                bos.write(bitsToWrite);
                bos.flush();
                bos.close();
            } catch (Exception e) {
                Log.e("FLG_POOFER", "Could not write to file");
                e.printStackTrace();
            }
        }
    }

}
