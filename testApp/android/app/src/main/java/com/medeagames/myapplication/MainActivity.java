package com.medeagames.myapplication;

import android.os.Bundle;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;

import java.util.ArrayList;

import org.json.*;

public class MainActivity extends AppCompatActivity {
    // references to view objects
    private EditText mTimesliceTextWidget;
    private CheckBox mRepeatCheckboxWidget;
    private LinearLayout mPatternGridWidget;

    public static final int MAX_CHANNELS = 8;

    // internal backing
    /* or not...
    private boolean mRepeat    = false;
    */
    private int mTimesliceMs   = 1000; // XXX get from value in resources for default timeslicetext
    private int mControllerId  = 0;    // NB - not currently controllable...
    private boolean mIsRunning = false; // XXX add some sort of UI that says that this is running
    private int mNumChannels   = 2;    // make this selectable

    // service to talk to the serial cable
    static private SerialService serialInterface = null;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        // XXX - not sure what this is or what I want it to do
        FloatingActionButton fab = (FloatingActionButton) findViewById(R.id.fab);
        fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                        .setAction("Action", null).show();
            }
        });

        if (serialInterface == null ){
            serialInterface = new SerialService(this.getApplicationContext());
        }
        mTimesliceTextWidget  = (EditText)findViewById(R.id.speed_ms);
        mRepeatCheckboxWidget = (CheckBox)findViewById(R.id.repeatCheckbox);
        mPatternGridWidget    = (LinearLayout)findViewById(R.id.patternGrid);

        /*
        mTimesliceTextWidget.addTextChangedListener(new TextWatcher(){
            public void afterTextChanged(Editable s) {

                // check - is the text an integer? If not, revert to previous text
                String newText = s.toString();
                try {
                    Integer intText = Integer.parseInt(newText);
                    if (intText != null) {
                        mTimesliceMs = intText.intValue();
                    } else {
                        s.clear();
                        s.append(String.valueOf(mTimesliceMs));
                    }
                } catch (NumberFormatException e) {
                    s.clear();
                    s.append(String.valueOf(mTimesliceMs));
                }
            }
            public void beforeTextChanged(CharSequence s, int start, int count, int after){}
            public void onTextChanged(CharSequence s, int start, int before, int count){}
        });
        */

        updateViewFromBundle(savedInstanceState);
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
        SignalRequest request = new SignalRequest(false, "00", mTimesliceMs, repeat);

        // go through all the rows, adding the signals
        for (int i = 0; i< mPatternGridWidget.getChildCount(); i++) {
            View child1 = mPatternGridWidget.getChildAt(i);
            if (child1.getClass() == TimesliceRow.class) {
                ArrayList<Boolean> signals = new ArrayList<>();
                LinearLayout patternRow = (LinearLayout)((TimesliceRow)child1).getChildAt(0);  // XXX this will not be necessary when I fix the inflate
                // now iterate through these children, looking for SingleColorViews
                int maxRows = Math.max(patternRow.getChildCount(), mNumChannels);
                for (int j=0; j<maxRows; j++ ){
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
        serialInterface.sendSignalRequest(request);
    }

    public void stopPattern(View view) {
        boolean repeat = mRepeatCheckboxWidget.isChecked();
        SignalRequest request = new SignalRequest(true, "00", mTimesliceMs, repeat);
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
        savedInstanceState.putInt("Timeslice_ms", mTimesliceMs);

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

        savedInstanceState.putString("PatternData", patternDataJson.toString());
    }

    @Override
    public void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        updateViewFromBundle(savedInstanceState);
    }

    private void updateViewFromBundle(Bundle savedInstanceState) {

        ArrayList<ArrayList<Boolean>> patternData = null;

        boolean repeat = false;

        if (savedInstanceState != null) {
            repeat = savedInstanceState.getBoolean("RepeatValue", repeat);
            mControllerId = savedInstanceState.getInt("ControllerId", 0);   // XXX create static default
            mNumChannels = savedInstanceState.getInt("NumberChannels", 4);  // XXX create static default
            mTimesliceMs = savedInstanceState.getInt("TimesliceMs", mTimesliceMs);

            String patternDataString = savedInstanceState.getString("PatternData", null);
            patternData = new ArrayList<>();

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

}
