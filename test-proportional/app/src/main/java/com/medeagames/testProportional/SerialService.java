package com.medeagames.testProportional;

import android.content.Context;
import android.os.Looper;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.Toast;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.ArrayList;

import com.ftdi.j2xx.D2xxManager;
import com.ftdi.j2xx.FT_Device;

/**
 * Created by csw on 5/8/16.
 */
public class SerialService {
    static final int MAX_VALVES = 4;

    LinkedBlockingQueue<SignalRequest> queue;
    SignalSendingThread signalThread;

    ArrayList<Float> onSignalArray = new ArrayList<>();


    static D2xxManager ftD2xx = null;  // XXX why static??
    FT_Device ftDev;
    int deviceCount = -1;
    Context global_context;
    int currentDeviceIdx = -1;

    final byte XON = 0x11;    /* Resume transmission */
    final byte XOFF = 0x13;    /* Pause transmission */

    final int TOAST_MESSAGE = 2;


    final Handler mHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(Message message) {
            if (message.what == TOAST_MESSAGE) {
                Toast.makeText(global_context, message.obj.toString(), Toast.LENGTH_SHORT).show();
            }
        }
    };

    //uart_configured = true;


    public void createDeviceList() {
        int tempDeviceCount = ftD2xx.createDeviceInfoList(global_context);

        if (tempDeviceCount > 0) {
            if (deviceCount != tempDeviceCount) {
                deviceCount = tempDeviceCount;
            }
        } else {
            deviceCount = 0;
        }
    }

    public void connectDevice(int idx) {
        if (deviceCount < 1) {
            Log.e("FLG_POOFER", "No devices to connect");
            return;
        }

        if (idx >= deviceCount) {
            Log.e("FLG_POOFER", "Request connection to a non-existent device");
            return;
        }

        if (ftDev != null && ftDev.isOpen()) {
            Log.e("FLG_POOFER", "Device already connected");
            return;
        }

        /*
        if(bReadTheadEnable) {
            bReadTheadEnable = false;
            try {
                Thread.sleep(50);
            }
            catch (InterruptedException e) {e.printStackTrace();}
        }
        */

        ftDev = ftD2xx.openByIndex(global_context, idx);
        //uart_configured = false;

        if (ftDev == null) {
            Log.e("FLG_POOFER", "Could not open device");
            // XXX log something midToast("Open port("+portIndex+") NG!", Toast.LENGTH_LONG);
            currentDeviceIdx = -1;
            return;
        }

        currentDeviceIdx = idx;
        /*
        if (ftDev.isOpen()) {
            if (false == bReadTheadEnable) {
                readThread = new ReadThread(handler);
                readThread.start();
            }
        } else {
            //midToast("Open port("+portIndex+") NG!", Toast.LENGTH_LONG);
            // XXX log something
        }
        */
    }


    private void configureDevice() {
        if (ftDev != null) {
            // reset to UART mode for 232 devices
            ftDev.setBitMode((byte) 0, D2xxManager.FT_BITMODE_RESET);

            ftDev.setBaudRate(19200);

            ftDev.setDataCharacteristics(D2xxManager.FT_DATA_BITS_8, D2xxManager.FT_STOP_BITS_1, D2xxManager.FT_PARITY_NONE);

            ftDev.setFlowControl(D2xxManager.FT_FLOW_XON_XOFF, XON, XOFF);
        }
    }


    // Really don't want this controlled by the low level functions...
    // this is the MidToast, ie, put in the middle of the screen. May no want this at all!
    /*
    void showToast(String str, int showTime)
    {
        Toast toast = Toast.makeText(global_context, str, showTime);
        toast.setGravity(Gravity.CENTER_VERTICAL|Gravity.CENTER_HORIZONTAL , 0, 0);

        TextView v = (TextView) toast.getView().findViewById(android.R.id.message);
        v.setTextColor(Color.YELLOW);
        toast.show();
    }
    */


    enum DeviceStatus {
        DEV_NOT_CONNECT,
        DEV_NOT_CONFIG,
        DEV_CONFIG
    }

    SerialService(Context context) {
        global_context = context;
        try {
            ftD2xx = D2xxManager.getInstance(context);
        } catch (D2xxManager.D2xxException e) {
            Log.e("FLG_POOFER", "FTDI D2XX.getInstance fail!!");
        }
        connect();
        /*
        if (ftD2xx != null) {
            createDeviceList();
            connectDevice(0);
            // XXX - NB - Want to be able to do this if a cable is plugged in or unplugged
            if (ftDev != null) {
                configureDevice();
            }
        }
        */
        for (int i=0; i<MAX_VALVES; i++) {
            onSignalArray.add(new Float(1.0));
        }

        queue = new LinkedBlockingQueue<SignalRequest>();
        signalThread = new SignalSendingThread(queue, ftDev, context, this);

        signalThread.start();
    }

    public FT_Device connect() {
        createDeviceList();
        connectDevice(0);
        if (ftDev != null) {
            configureDevice();
        }
        return ftDev;
    }


    public void sendSignalRequest(SignalRequest request) {
        if (!queue.offer(request)) {
            Log.e("FLG_POOFER", "Could not insert request into queue");
        }
    }


    class SignalSendingThread extends Thread {
        LinkedBlockingQueue<SignalRequest> queue;
        FT_Device     ftDev;
        SerialService parentService;

        SignalSendingThread(LinkedBlockingQueue<SignalRequest> queue, FT_Device ftDev, Context global_context, SerialService parentService) {
            this.queue = queue;
            this.ftDev = ftDev;
            this.parentService = parentService;

            if(!(Thread.getDefaultUncaughtExceptionHandler() instanceof CustomExceptionHandler)) {
                Thread.setDefaultUncaughtExceptionHandler(new CustomExceptionHandler(
                        "/sdcard/flg_serial"));
            }
        }

        public void run() {
            long timeout = 1000;
            while (true) {
                try {
                    SignalRequest request = queue.poll(timeout, TimeUnit.MILLISECONDS);
                    if (request != null) {
                        if ((request.flags & SignalRequest.SHUT_OFF) != 0) {
                            turnOff();
                        } else if ((request.flags & SignalRequest.ALL_ON) != 0) {
                            fullOn();
                        } else if ((request.flags & SignalRequest.STOP) != 0) {
                            stopSignals();
                        } else {
                            sendSignals(request.controllerId, request.timeslice, request.signalArray, request.repeat);
                        }
                    }
                } catch (InterruptedException e) {
                    // XXX is this why I can't break out of the program on a crash?
                }
            }
        }

        // just stop sending signals - no change to valve values
        private void stopSignals()
        {
            // there's nothing really to do here... the fact that we're not in the repeat loop
            // means that the valve values will not change
        }

        // turn all valves on
        private void fullOn() {
            sendSignal(0, onSignalArray);
        }

        // stop sending signals, turn all valves off
        private void turnOff() {
            String sendString = "!00000.";
            sendData(sendString);
        }

        /* Send timesliced signals to the serial port, one for each poofer ''' */
        private void sendSignal(int controller_id, ArrayList<Float> signalArray) {
            StringBuffer sendString = new StringBuffer("!");
            sendString.append(controllerIdToString(controller_id));
            for (int i = 0; i < signalArray.size(); i++) {
                Float value = signalArray.get(i);
                char charValue = (char)(value*255);

                sendString.append(i + 1);
                sendString.append(String.format("%02X", charValue & 0xFF));
                if (i == signalArray.size() - 1) {
                    sendString.append(".");
                } else {
                    sendString.append("~");
                }
            }
            sendData(sendString.toString());
        }

        private String controllerIdToString(int controllerId) {
            String controllerIdStr = Integer.toHexString(controllerId);
            if (controllerIdStr.length() < 2) {
                controllerIdStr = "0" + controllerIdStr;
            }
            return controllerIdStr;
        }

        /* Send multiple timeslices to the serial port. May repeat ''' */
        private void sendSignals(int controller_id, long timeslice, ArrayList<ArrayList<Float>> signalArray, boolean repeat) {
            do {
                for (ArrayList<Float> signals : signalArray) {
                    long now = System.currentTimeMillis();
                    long nextTime = now + timeslice;
                    sendSignal(controller_id, signals);
                    try {
                        Thread.sleep(nextTime - System.currentTimeMillis());
                    } catch (InterruptedException e) {

                    }
                    if (queue.size() != 0) {
                        repeat = false;
                        break;
                    }
                }
            } while (repeat);
        }

        private void sendData(String data) {
            if (ftDev == null || ftDev.isOpen() == false) {
                ftDev = parentService.connect();
                StringBuilder toastString = new StringBuilder("SendData - ");
                if (ftDev == null) {
                    toastString.append("No Device. Would have sent ");
                } else {
                    toastString.append("Device not open. Would have sent ");
                }
                toastString.append(data);
                Log.e("FLG_POOFER", toastString.toString());
                Message message = mHandler.obtainMessage(TOAST_MESSAGE, toastString);
                message.sendToTarget();
            } else if (data != null && data.length() > 0) {
                ftDev.write(data.getBytes());
            }

        }

    }

}







