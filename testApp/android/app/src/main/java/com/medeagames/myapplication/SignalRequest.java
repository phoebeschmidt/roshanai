package com.medeagames.myapplication;

import java.util.ArrayList;

/**
 * Created by carolyn on 5/9/16.
 */
public class SignalRequest {
    boolean  stop;   // NB, if 'stop' is set, none of the other members are valid
    String   controllerId;
    int      timeslice;
    boolean  repeat;
    ArrayList<ArrayList<Boolean>> signalArray;

    SignalRequest(boolean stop, String controllerId, int timeslice, boolean repeat) {
        this.stop = stop;
        this.controllerId = controllerId; // XXX check for valid values... somewhere. Could make this a number...
        this.timeslice = timeslice;
        this.repeat = repeat;
        this.signalArray = new ArrayList<ArrayList<Boolean>>();
    }

    public boolean getStop() {
        return stop;
    }

    public String getControllerId() {
        return controllerId;
    }

    public int getTimeslice() {
        return timeslice;
    }

    public boolean getRepeat() {
        return repeat;
    }

    public ArrayList<ArrayList<Boolean>> getSignalArray() {
        return signalArray;
    }

    public void addSignals(ArrayList<Boolean> signals) {
        signalArray.add(signals);
    }
 }
