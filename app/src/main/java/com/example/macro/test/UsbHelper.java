package com.example.macro.test;

import static com.example.macro.MainActivity.ACTION_USB_PERMISSION;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.util.Log;

import java.lang.reflect.Field;

public class UsbHelper {
    private UsbManager usbManager;
    private PendingIntent permissionIntent;
    private long nativeHandlerPtr;

    public UsbHelper(Context context) {
        usbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);

        permissionIntent = PendingIntent.getBroadcast(context, 0, new Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE);
    }

    public void requestPermission(UsbDevice device) {
        usbManager.requestPermission(device, permissionIntent);
    }

    public int getFileDescriptor(UsbDevice device) {
        UsbDeviceConnection connection = usbManager.openDevice(device);
        Log.d("device_name", device.getDeviceName());

        if (connection == null) {
            throw new IllegalStateException("Unable to open USB device.");
        }

        int fd = connection.getFileDescriptor();

        return fd;
    }

    public int getBusNum(UsbDevice device) {
        String deviceName = device.getDeviceName();
        String[] parts = deviceName.split("/");
        if (parts.length >= 2) {
            return Integer.parseInt(parts[parts.length - 2]);
        }
        throw new IllegalStateException("Unable to get bus number for the USB device.");
    }

    public int getDevAddr(UsbDevice device) {
        String deviceName = device.getDeviceName();
        String[] parts = deviceName.split("/");
        if (parts.length >= 1) {
            return Integer.parseInt(parts[parts.length - 1]);
        }
        throw new IllegalStateException("Unable to get device address for the USB device.");
    }

    public String getUsbFs() {
        return "/dev/bus/usb";
    }

    public String getUsbDeviceType(UsbDevice device) {
        UsbDeviceConnection connection = usbManager.openDevice(device);
        if (connection == null) {
            return "Invalid";
        }

        for (int i = 0; i < device.getInterfaceCount(); i++) {
            UsbInterface usbInterface = device.getInterface(i);
            int interfaceClass = usbInterface.getInterfaceClass();
            if (interfaceClass == 3) {
                // HID Class: 키보드일 가능성이 큼
                connection.close();
                return "Keyboard";
            } else if (interfaceClass == 14) {
                // Video Class: 캡처보드일 가능성이 큼
                connection.close();
                return "CaptureBoard";
            }
        }

        connection.close();
        return "Unknown";
    }

    public boolean isValidUsbDevice(UsbDevice device) {
        UsbDeviceConnection connection = usbManager.openDevice(device);
        if (connection == null) {
            return false;
        }

        for (int i = 0; i < device.getInterfaceCount(); i++) {
            UsbInterface usbInterface = device.getInterface(i);
            for (int j = 0; j < usbInterface.getEndpointCount(); j++) {
                UsbEndpoint endpoint = usbInterface.getEndpoint(j);
                Log.d("USBHelper", "Endpoint address: " + endpoint.getAddress());
            }
        }

        connection.close();
        return true;
    }



    // Load the native library
    static {
        System.loadLibrary("native_lib");  // 올바른 라이브러리 이름으로 변경
    }
}
