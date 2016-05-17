package com.medeagames.myapplication;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import java.util.ArrayList;

/**
 * Created by carolyn on 5/14/16.
 */
public class TimesliceRow extends LinearLayout {

    int mNumRows = 1;

    public TimesliceRow(Context context,int numRows, boolean showDelete){
        super(context); // should create per the xml
        init(numRows, showDelete);
    }

    public TimesliceRow(Context context) {
        super(context);
        init(1,false);
    }

    public TimesliceRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(1, false);
    }

    public TimesliceRow(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        init(1, false);
    }

    private void init(int numRows, boolean showDelete) {
        inflate(getContext(), R.layout.timeslice_row, this);

        if (numRows <= 0) numRows = 1;

        mNumRows = numRows;

        // remove delete button if necessary
        /*
        if (!showDelete) {
            View deleteButton = findViewById(R.id.btn_delete);
            deleteButton.setVisibility(View.GONE);
        }
        */

        // hide the cells if necessary
        if (numRows < MainActivity.MAX_CHANNELS) {
            for (int i=MainActivity.MAX_CHANNELS; i > numRows; i--) {  // check that numRows = 0 still gets us a row, and it works for other values as well
                int cellId = getResources().getIdentifier("cell" + i, "id", getContext().getPackageName());
                View cell = findViewById(cellId);
                if (cell != null) {
                    cell.setVisibility(View.GONE);
                }
            }
        }

    }

    // This makes some assumptions about the layout of the TimesliceRow which are slightly uncomfortable
    public void setPattern(ArrayList<Boolean> patternData) {
        // go through the cells, setting the color
        // check that we're not going to run off of the
        // get to the SingleColorView arrays...
        int cellIdx = -1;
        ViewGroup childGroup = (ViewGroup)getChildAt(0); // XXX - there's an additional Linear Layout here that I need to get rid of
        for (int i = 0; i < childGroup.getChildCount(); i++) {
            View child = childGroup.getChildAt(0);
            if (child.getClass() == SingleColorView.class) {
                cellIdx = i;
                break;
            }
        }

        if (cellIdx >= 0) {
            for (Boolean onOffBoxed : patternData) {
                try {
                    SingleColorView child = (SingleColorView) childGroup.getChildAt(cellIdx++);
                    if (child != null) {
                        child.turnOn(onOffBoxed == null ? false : onOffBoxed);
                    }
                } catch (ClassCastException e) {
                    // Ack! Do something! this should never happen, but...
                }
            }
        }
    }
}
