package com.medeagames.myapplication;

import java.util.ArrayList;

/**
 * Created by carolyn on 5/9/16.
 */
public class SignalRequest {
    boolean     stop;   // NB, if 'stop' is set, none of the other members are valid
    int         controllerId; // must be between 0 and 255
    int         timeslice;
    boolean     repeat;
    SignalEvent event;
    String      debugTxt;
    ArrayList<ArrayList<Boolean>> signalArray;

    enum SignalEvent {
        USB_ATTACH,
        USB_DETACH,
        DEBUG
    };

    public SignalRequest() {

    }

    public SignalRequest(boolean stop, int controllerId, int timeslice, boolean repeat) {
        this.stop = stop;
        this.controllerId = controllerId; // XXX check for valid values... somewhere. Could make this a number...
        this.timeslice = timeslice;
        this.repeat = repeat;
        this.signalArray = new ArrayList<ArrayList<Boolean>>();
    }

    public boolean getStop() {
        return stop;
    }

    public int getControllerId() {
        return controllerId;
    }

    public int getTimeslice() { return timeslice; }

    public boolean getRepeat() {
        return repeat;
    }

    public ArrayList<ArrayList<Boolean>> getSignalArray() {
        return signalArray;
    }

    public void addSignals(ArrayList<Boolean> signals) {
        signalArray.add(signals);
    }

    public SignalEvent getEvent() {
        return event;
    }

    public void setEvent(SignalEvent event) {
        this.event = event;
    }

    public String getDebugTxt() {
        return debugTxt;
    }

    public void setDebugTxt(String text)
    {
        this.debugTxt = text;
    }
 }
