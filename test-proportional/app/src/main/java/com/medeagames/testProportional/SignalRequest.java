package com.medeagames.testProportional;

import java.util.ArrayList;

/**
 * Created by carolyn on 5/9/16.
 */
public class SignalRequest {
    int     flags;   // NB, if 'stop' is set, none of the other members are valid
    int      controllerId; // must be between 0 and 255
    int      timeslice;
    boolean  repeat;
    ArrayList<ArrayList<Float>> signalArray;

    static final char SHUT_OFF = 0x01;
    static final char ALL_ON   = 0x02;
    static final char STOP     = 0x04;

    SignalRequest(int flags, int controllerId, int timeslice, boolean repeat) {
        this.flags = flags;
        this.controllerId = controllerId; // XXX check for valid values... somewhere. Could make this a number...
        this.timeslice = timeslice;
        this.repeat = repeat;
        this.signalArray = new ArrayList<ArrayList<Float>>();
    }

    public int getFlags() {
        return flags;
    }

    public int getControllerId() {
        return controllerId;
    }

    public int getTimeslice() { return timeslice; }

    public boolean getRepeat() {
        return repeat;
    }

    public ArrayList<ArrayList<Float>> getSignalArray() {
        return signalArray;
    }

    public void addSignals(ArrayList<Float> signals) {
        signalArray.add(signals);
    }
 }
