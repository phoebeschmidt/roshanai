package com.medeagames.myapplication;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.util.Log;

/**
 * Created by carolyn on 7/7/16.
 */
public class UsbBroadcastReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String actionName = intent.getAction();
        //SerialService.getSerialService(context).sendEvent(SignalRequest.SignalEvent.DEBUG);
        if (actionName.equals("android.hardware.usb.action.USB_DEVICE_ATTACHED")) {
            UsbDevice device = (UsbDevice)intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
            int vendorId = device.getVendorId();
            int productId = device.getProductId();
            SerialService service = SerialService.getSerialService(null);
            if (service != null) {
                service.sendDebug("Vender Id " + vendorId + ", productId " + productId);
                service.sendEvent(SignalRequest.SignalEvent.USB_ATTACH);
            }
        } else if (actionName.equals("android.hardware.usb.action.USB_DEVICE_DETACHED")){
            SerialService.getSerialService(context).sendEvent(SignalRequest.SignalEvent.USB_DETACH);
        } else {
            Log.e("FLG", "Unexpected action received by usbBroadcastReceiver," + actionName);
        }
    }
}
