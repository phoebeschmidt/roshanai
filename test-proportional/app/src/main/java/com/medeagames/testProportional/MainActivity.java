package com.medeagames.testProportional;

import android.content.SharedPreferences;
import android.graphics.PointF;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
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
import android.widget.TextView;

import java.util.ArrayList;
import java.util.List;

import org.json.*;

public class MainActivity extends AppCompatActivity {
    // references to view objects
    private EditText mPatternTimeTextWidget;
    private CheckBox mRepeatCheckboxWidget;
    private ProportionalView mProportionalView;

    public static final int MAX_CHANNELS = 8;

    private int mPatternTimeSec= 5;     // XXX get from value in resources for default timeslicetext
    private int mControllerId  = 0;     // NB - not currently controllable...
    private boolean mIsRunning = false; // XXX add some sort of UI that says that this is running
    private int mNumChannels   = 1;     // XXX make this selectable

    // XXX - need some mode thing to switch between modes. A button, or a selector, or something. Radio buttons?

    // service to talk to the serial cable
    static private SerialService serialInterface = null;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        Log.i("FLG_POOFER", "Poofer start");

        if (serialInterface == null ){
            serialInterface = new SerialService(this.getApplicationContext());
        }
        mPatternTimeTextWidget  = (EditText)findViewById(R.id.pattern_time_sec);
        mRepeatCheckboxWidget = (CheckBox)findViewById(R.id.repeatCheckbox);
        mProportionalView     = (ProportionalView)findViewById(R.id.proportional_view);

        mPatternTimeTextWidget.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    Log.e("FLG_POOFER", "After text changed");
                    int newPatternTime = -1;
                    String newPatternTimeStr = view.getText().toString();

                    // check old vs new values to avoid infinite recursion.
                    if (!String.valueOf(mPatternTimeTextWidget).equals(newPatternTimeStr)) {
                        try {
                            newPatternTime = Integer.parseInt(newPatternTimeStr);
                        } catch (NumberFormatException e) {
                            Log.e("FLG_POOFER", "Invalid value for pattern time");
                        }

                        if (newPatternTime < 1 || newPatternTime > 30) {
                            Log.e("FLG_POOFER", "Pattern time number out of bounds");
                        } else {
                            mPatternTimeSec = newPatternTime;
                        }

                        mPatternTimeTextWidget.setText(Integer.toString(mPatternTimeSec));
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
                    "/sdcard/flg_main"));
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

    // Send pattern to serial interface. Called when Start button is pressed
    public void startPattern(View view){

        boolean repeat = mRepeatCheckboxWidget.isChecked();
        SignalRequest request = new SignalRequest(0, mControllerId, 100, repeat);

        // Add the signals
        List<PointF> points = mProportionalView.getPoints();
        if (points == null || points.size() < 2) {
            Log.e("FLG", "Fewer than 2 points. Cannot parse point data");
            return;
        }

        int nextPointIdx = 1;
//        Float xIncr = (float)(mProportionalView.getWidth())/(mPatternTimeSec*10); // number of pixels per timeslice
        Float xIncr = (float)(1.0f/(mPatternTimeSec*10)); // number of pixels per timeslice
        PointF prevPoint = points.get(0);
        PointF nextPoint = points.get(1);
        Float curX = prevPoint.x;
        while (curX <= 1.0f) { // XXX want the view of the main thingy!
            if (nextPoint.x <= curX ) {
                if (nextPointIdx + 1 < points.size()) { // Check for overflow
                    break;
                }
                prevPoint = nextPoint;
                nextPoint = points.get(nextPointIdx++);
            }
            Float dydx = (nextPoint.y - prevPoint.y)/(nextPoint.x - prevPoint.x);
            Float deltaX = curX - prevPoint.x;
            Float value = 1.0f - (prevPoint.y + deltaX*dydx);  // complementary, because in screen coordinates (where the original y is from) the origin is top right
            //values.add(value);
            curX += xIncr;
            ArrayList<Float> simpleArrayList = new ArrayList<>();
            simpleArrayList.add(value);
            request.addSignals(new ArrayList<>(simpleArrayList));
        }

        savePreferences(); // ideally I should do this every time data changes XXX
        serialInterface.sendSignalRequest(request);
    }

    public void stopPattern(View view) {
        boolean repeat = mRepeatCheckboxWidget.isChecked();
        SignalRequest request = new SignalRequest(SignalRequest.STOP, 0, 10, repeat);
        serialInterface.sendSignalRequest(request);
    }

    public void clearPattern(View view) {
        mProportionalView.clear();
    }

    public void undoPattern(View view) {
        mProportionalView.undo();
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
        savedInstanceState.putInt("PatternTimeSec", mPatternTimeSec);

        // now the pattern data
        JSONArray patternDataJson = getPatternData();

        Log.e("FLG_POOFER", "PatternData stored is " + patternDataJson.toString());

        savedInstanceState.putString("PatternData", patternDataJson.toString());
    }

    @Override
    public void onRestoreInstanceState(Bundle savedInstanceState) {
        super.onRestoreInstanceState(savedInstanceState);
        updateViewFromBundle(savedInstanceState);
    }

    private JSONArray getPatternData()
    {
        ArrayList<List<PointF>> patternData = new ArrayList<>();
        patternData.add(mProportionalView.getPoints());
        JSONArray patternDataJson = new JSONArray();
        for (List<PointF> channelData : patternData) {
            JSONArray channelJson = new JSONArray();
            for (PointF value : channelData) {
                JSONObject point = new JSONObject();
                try {
                    point.put("X", (double) value.x);
                    point.put("Y", (double) value.y);
                    channelJson.put(point); // want to put x and y values... XXX
                } catch (org.json.JSONException e) {
                    Log.e("FLG", e.toString());
                }
            }
            patternDataJson.put(channelJson);
        }
        return patternDataJson;
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
        editor.putInt("PatternTimeSec", mPatternTimeSec);

        // now the pattern data
        JSONArray patternDataJson = getPatternData();

        Log.e("FLG_POOFER", "PatternData stored is " + patternDataJson.toString());

        editor.putString("PatternData", patternDataJson.toString());
        editor.commit();
    }

    private void updateViewFromPreferences(SharedPreferences prefs){
        if (prefs != null) {
            boolean repeat = prefs.getBoolean("RepeatValue", false);
            mControllerId = prefs.getInt("ControllerId", 0);
            mNumChannels = prefs.getInt("NumberChannels", 4);
            mPatternTimeSec = prefs.getInt("PatternTimeSec", mPatternTimeSec);
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
            mPatternTimeSec = savedInstanceState.getInt("PatternTimeSec", mPatternTimeSec);

            patternDataString = savedInstanceState.getString("PatternData", null);
            Log.e("FLG_POOFER", "PatternData retrieved is " + patternDataString);
            Log.e("FLG_POOFER", "num channels retrieved is " + mNumChannels);
         }

        restoreState(repeat, patternDataString);
    }

    private void restoreState(boolean repeat, String patternDataString)
    {
        ArrayList<ArrayList<PointF>> patternData = null;

        if (patternDataString != null) {
            patternData = new ArrayList<>();
            try {
                JSONArray channels = new JSONArray(patternDataString);
                for (int i = 0; i < channels.length(); i++) {
                    JSONArray channelDataJson = channels.getJSONArray(i);
                    ArrayList<PointF> channelData = new ArrayList<>();
                    for (int j = 0; j < channelDataJson.length(); j++) {
                        JSONObject pointLike = channelDataJson.getJSONObject(j);
                        float x = (float)pointLike.getDouble("X");
                        float y = (float)pointLike.getDouble("Y");
                        channelData.add(new PointF(x,y));
                    }
                    patternData.add(channelData);
                }
            } catch (org.json.JSONException jsonException) {
                /// XXX
            }
        }

        mRepeatCheckboxWidget.setChecked(repeat);
        mPatternTimeTextWidget.setText(String.valueOf(mPatternTimeSec));

        // hide all channel headers for unused channels
        if (mNumChannels <= 0) mNumChannels = 1;
        for (int i=MAX_CHANNELS; i>mNumChannels; i--) {
            int headerId = getResources().getIdentifier("ch" + i + "Header", "id", this.getPackageName());
            View header = findViewById(headerId);
            if (header != null) {
                header.setVisibility(View.GONE);
            }
        }
        if (patternData != null && patternData.get(0) != null) {
            mProportionalView.setPoints(patternData.get(0));
        }
    }


}
