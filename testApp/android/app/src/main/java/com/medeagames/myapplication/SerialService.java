package com.medeagames.myapplication;

import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Looper;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.Gravity;
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

    static SerialService gSerialService = null;

    // Context should be the global application context.
    static public SerialService getSerialService(Context context)
    {
        if (gSerialService == null && context != null) {
            gSerialService = new SerialService(context);
        } else if (context == null) {
            Log.e("FLG", "Requested creation of serial service, but global context is NULL");
        }
        return gSerialService;
    }

    LinkedBlockingQueue<SignalRequest> queue;
    SignalSendingThread signalThread;


    static D2xxManager ftD2xx = null;  // XXX why static??
    Context global_context;

    boolean isAttached;

    final byte XON  = 0x11;    /* Resume transmission */
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

    private void doToast(String toastString)
    {
        Message message = mHandler.obtainMessage(TOAST_MESSAGE, toastString);
        message.sendToTarget();
    }

    //uart_configured = true;




    enum DeviceStatus {
        DEV_NOT_CONNECT,
        DEV_NOT_CONFIG,
        DEV_CONFIG
    }


    private SerialService(Context context) {
        global_context = context;
        try {
            ftD2xx = D2xxManager.getInstance(context);
        } catch (D2xxManager.D2xxException e) {
            Log.e("FLG_POOFER", "FTDI D2XX.getInstance fail!!"); // XXX - and do what, exactly?
        }

        queue = new LinkedBlockingQueue<SignalRequest>();
        signalThread = new SignalSendingThread(queue, context);

        signalThread.start();
    }


    public void sendSignalRequest(SignalRequest request) {
        if (!queue.offer(request)) {
            Log.e("FLG_POOFER", "Could not insert request into queue");
        }
    }

    public void sendEvent(SignalRequest.SignalEvent event) {
        SignalRequest message = new SignalRequest();
        message.setEvent(event);
        sendSignalRequest(message);
    }

    public void sendDebug(String text) {
        SignalRequest message = new SignalRequest();
        message.setEvent(SignalRequest.SignalEvent.DEBUG);
        message.setDebugTxt(text);
        sendSignalRequest(message);
    }


    class SignalSendingThread extends Thread {
        LinkedBlockingQueue<SignalRequest> queue;
        FT_Device     ftDev;
        int deviceCount = -1;
        Context global_context;
        int currentDeviceIdx = -1;


        SignalSendingThread(LinkedBlockingQueue<SignalRequest> queue, Context context) {
            this.queue = queue;
            this.global_context = context;

            if (!(Thread.getDefaultUncaughtExceptionHandler() instanceof CustomExceptionHandler)) {
                Thread.setDefaultUncaughtExceptionHandler(new CustomExceptionHandler(
                        "/mnt/extsd/flg_serial"));  // XXX yes, this is a hack
            }
        }

        // what do we want to do here?
        // First time through - attempt to attach to device
        // If device is not attached, attept to attach to device every 10 seconds
        // If a message comes through that says device is attached, attempt to attach to device
        // If we fail the send because the device is not attached, mark device as not attached
        // If we get a message that says that the device is not attached, mark the device as not attached
        public void run() {
            long timeout = 1000;
            ftDev = FTDI_connect();
            while (true) {
                try {
                    SignalRequest request = queue.poll(timeout, TimeUnit.MILLISECONDS);
                    if (request != null) {
                        if (request.getEvent() != null) {
                            doToast("Received broadcast message " + request.getEvent());
                            if (request.getEvent() == SignalRequest.SignalEvent.DEBUG) {
                                doToast(request.getDebugTxt());
                            } else if (request.getEvent() == SignalRequest.SignalEvent.USB_ATTACH) {
                                if (ftDev != null) {
                                    Log.d("FLG", "Received a usbAttached message, but device was already marked attached");
                                }
                                ftDev = FTDI_connect();
                                isAttached = true;

                            } else if (request.getEvent() == SignalRequest.SignalEvent.USB_DETACH) {
                                if (ftDev == null) {
                                    Log.d("FLG", "Received a usbDetached message, but device was already marked detached");
                                }
                                ftDev = null; // XXX do I have to do anything else?
                                isAttached = false;
                            }
                        } else {
                            if (isAttached && ftDev == null) {
                                ftDev = FTDI_connect();
                            }
                            if (ftDev != null) {
                                if (request.stop) {
                                    stopSignals();
                                } else {
                                    sendSignals(request.controllerId, request.timeslice, request.signalArray, request.repeat);
                                }
                            } else {
                                Log.w("FLG", "Request to send signals received, but no device to send on");
                                doToast("No Device Found");

                                // XXX - end not running flag back at activity level
                            }
                        }
                    }
                } catch (InterruptedException e) {
                    // XXX is this why I can't break out of the program on a crash?
                } catch (Exception e) {

                }
            }
        }

        public FT_Device FTDI_connect() {
            createDeviceList();
            connectDevice(0);
            if (ftDev != null) {
                configureDevice();
            }
            return ftDev;
        }

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
                String isNull = "Context is " + global_context == null ? "null": "not null";
                doToast("No devices to connect to, " + isNull);
                return;
            }

            if (idx >= deviceCount) {
                Log.e("FLG_POOFER", "Request connection to a non-existent device");
                doToast("Device requested does not exist");
                return;
            }

            if (ftDev != null && ftDev.isOpen()) {
                doToast("device already connected");
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
                doToast("Could not open device");
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


        // Okay we've now got a problem that we're aborting the run, rather than suspending, when we get a disconnect. That's probably what we
        // want, honestly. XXX - think about this.


        private void stopSignals() {
            String sendString = "!0000.";
            sendData(sendString);
        }

        /* Send timesliced signals to the serial port, one for each poofer ''' */
        private void sendSignal(int controller_id, ArrayList<Boolean> signalArray) {
            StringBuffer sendString = new StringBuffer("!");
            sendString.append(controllerIdToString(controller_id));
            for (int i = 0; i < signalArray.size(); i++) {
                boolean onOff = signalArray.get(i);
                sendString.append(i + 1);
                sendString.append(onOff ? "1" : "0");
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
        private void sendSignals(int controller_id, long timeslice, ArrayList<ArrayList<Boolean>> signalArray, boolean repeat) {
            do {
                for (ArrayList<Boolean> signals : signalArray) {
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
                ftDev = FTDI_connect();
                StringBuilder toastString = new StringBuilder("SendData - ");
                if (ftDev == null) {
                    toastString.append("No Device. Would have sent ");
                } else {
                    toastString.append("Device not open. Would have sent "); // XXX how long does it take for the device to become 'open'? What does a not 'open' device look like?
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







