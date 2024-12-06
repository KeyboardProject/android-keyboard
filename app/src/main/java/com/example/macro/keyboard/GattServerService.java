package com.example.macro.keyboard;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.ParcelUuid;
import android.util.Log;
import androidx.annotation.Nullable;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.pm.PackageManager;

import com.example.macro.MainActivity;
import com.example.macro.R;

import java.util.UUID;

public class GattServerService extends Service {

    private static final String TAG = "GattServerService";
    private static final String CHANNEL_ID = "GattServerServiceChannel";
    public static final String ACTION_KEY_EVENT = "com.ckbs.blehidkeyboard.ACTION_KEY_EVENT";
    public static final String EXTRA_KEY_INDEX = "com.ckbs.blehidkeyboard.EXTRA_KEY_INDEX";
    public static final String EXTRA_KEY_PRESSED = "com.ckbs.blehidkeyboard.EXTRA_KEY_PRESSED";

    private static final UUID BOOT_MOUSE_INPUT_REPORT_UUID = UUID.fromString("00002A33-0000-1000-8000-00805f9b34fb"); // Boot Mouse Input Report

    // HID over GATT UUIDs
    private static final UUID HID_SERVICE_UUID = UUID.fromString("00001812-0000-1000-8000-00805f9b34fb"); // Human Interface Device Service
    private static final UUID REPORT_MAP_UUID = UUID.fromString("00002A4B-0000-1000-8000-00805f9b34fb"); // Report Map
    private static final UUID REPORT_UUID = UUID.fromString("00002A4D-0000-1000-8000-00805f9b34fb"); // Report
    private static final UUID HID_INFORMATION_UUID = UUID.fromString("00002A4A-0000-1000-8000-00805f9b34fb"); // HID Information
    private static final UUID PROTOCOL_MODE_UUID = UUID.fromString("00002A4E-0000-1000-8000-00805f9b34fb"); // Protocol Mode
    private static final UUID BOOT_KEYBOARD_INPUT_REPORT_UUID = UUID.fromString("00002A22-0000-1000-8000-00805f9b34fb"); // Boot Keyboard Input Report
    private static final UUID CLIENT_CHARACTERISTIC_CONFIG_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"); // Client Characteristic Configuration
    private static final UUID REPORT_REFERENCE_UUID = UUID.fromString("00002908-0000-1000-8000-00805f9b34fb"); // Report Reference Descriptor

    private BluetoothManager mBluetoothManager;
    private BluetoothAdapter mBluetoothAdapter;
    private BluetoothLeAdvertiser mBluetoothLeAdvertiser;
    private BluetoothGattServer mGattServer;
    private BluetoothDevice mConnectedDevice;

    // 키 입력 상태 배열
    private boolean[] keyPressState = new boolean[54]; // 버튼 수에 맞게 설정

    private final IBinder binder = new LocalBinder();

    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    public class LocalBinder extends Binder {
        public GattServerService getService() {
            return GattServerService.this;
        }
    }

    private static final byte[] REPORT_MAP = new byte[]{
            0x05, 0x01,        // Usage Page (Generic Desktop)
            0x09, 0x02,        // Usage (Mouse)
            (byte) 0xA1, 0x01, // Collection (Application)
            (byte) 0x85, 0x02, //   Report ID (2)
            0x09, 0x01,        //   Usage (Pointer)
            (byte) 0xA1, 0x00, //   Collection (Physical)
            0x05, 0x09,        //     Usage Page (Buttons)
            0x19, 0x01,        //     Usage Minimum (01)
            0x29, 0x03,        //     Usage Maximum (03)
            0x15, 0x00,        //     Logical Minimum (0)
            0x25, 0x01,        //     Logical Maximum (1)
            (byte) 0x95, 0x03, //     Report Count (3)
            0x75, 0x01,        //     Report Size (1)
            (byte) 0x81, 0x02, //     Input (Data, Variable, Absolute) ;3 button bits
            (byte) 0x95, 0x01, //     Report Count (1)
            0x75, 0x05,        //     Report Size (5)
            (byte) 0x81, 0x01, //     Input (Constant) ;5 bit padding
            0x05, 0x01,        //     Usage Page (Generic Desktop)
            0x09, 0x30,        //     Usage (X)
            0x09, 0x31,        //     Usage (Y)
            0x15, 0x00,        //     Logical Minimum (0)
            0x26, (byte) 0xFF, 0x7F,  //     Logical Maximum (32767)
            0x75, 0x10,        //     Report Size (16 bits)
            (byte) 0x95, 0x02, //     Report Count (2)
            (byte) 0x81, 0x02, //     Input (Data, Variable, Absolute) ;Absolute X & Y positions

            0x09, 0x38,        //     Usage (Wheel)
            0x15, (byte) 0x81, //     Logical Minimum (-127)
            0x25, 0x7F,        //     Logical Maximum (127)
            0x75, 0x08,        //     Report Size (8)
            (byte) 0x95, 0x01, //     Report Count (1)
            (byte) 0x81, 0x06, //     Input (Data, Variable, Relative) ;1 wheel byte

            (byte) 0xC0,       //   End Collection
            (byte) 0xC0,       // End Collection

            // Keyboard Report Descriptor
            0x05, 0x01,        // Usage Page (Generic Desktop)
            0x09, 0x06,        // Usage (Keyboard)
            (byte) 0xA1, 0x01, // Collection (Application)
            (byte) 0x85, 0x01, //   Report ID (1)
            0x05, 0x07,        //   Usage Page (Key Codes)
            0x19, (byte) 0xE0, //   Usage Minimum (224)
            0x29, (byte) 0xE7, //   Usage Maximum (231)
            0x15, 0x00,        //   Logical Minimum (0)
            0x25, 0x01,        //   Logical Maximum (1)
            0x75, 0x01,        //   Report Size (1)
            (byte) 0x95, 0x08, //   Report Count (8)
            (byte) 0x81, 0x02, //   Input (Data, Variable, Absolute) ; Modifier byte
            0x75, 0x08,        //   Report Size (8)
            (byte) 0x95, 0x01, //   Report Count (1)
            (byte) 0x81, 0x01, //   Input (Constant) ; Reserved byte
            0x75, 0x08,        //   Report Size (8)
            (byte) 0x95, 0x06, //   Report Count (6)
            0x15, 0x00,        //   Logical Minimum (0)
            0x25, 0x65,        //   Logical Maximum (101)
            0x05, 0x07,        //   Usage Page (Key Codes)
            0x19, 0x00,        //   Usage Minimum (0)
            0x29, 0x65,        //   Usage Maximum (101)
            (byte) 0x81, 0x00, //   Input (Data, Array)
            (byte) 0xC0        // End Collection
    };

    // HID Information
    private static final byte[] HID_INFORMATION = new byte[]{
            0x11, 0x01, // bcdHID (HID Class Specification release number)
            0x00,       // bCountryCode
            0x01        // Flags (RemoteWake)
    };

    private BluetoothGattCharacteristic mReportCharacteristic;
    private BluetoothGattCharacteristic mMouseInputReportCharacteristic;

    // 페어링 상태 변경을 수신하는 BroadcastReceiver
    private final BroadcastReceiver mBondingReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(intent.getAction())) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                int bondState = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR);
                int previousBondState = intent.getIntExtra(BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE, BluetoothDevice.ERROR);

                if (device != null && device.equals(mConnectedDevice)) {
                    if (bondState == BluetoothDevice.BOND_BONDED) {
                        Log.i(TAG, "Device bonded: " + device.getAddress());
                        // 페어링된 기기 주소 저장
                        saveBondedDevice(device);
                    } else if (bondState == BluetoothDevice.BOND_NONE && previousBondState == BluetoothDevice.BOND_BONDED) {
                        Log.i(TAG, "Device unbonded: " + device.getAddress());
                        // 페어링 정보 삭제
                        removeBondedDevice();
                    }
                }
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();

        Log.d("adfasdf", "astasdfa");
        createNotificationChannel();
        startForegroundService();

        mBluetoothManager = (BluetoothManager) getSystemService(Context.BLUETOOTH_SERVICE);
        mBluetoothAdapter = mBluetoothManager.getAdapter();

        if (mBluetoothAdapter == null || !mBluetoothAdapter.isEnabled()) {
            Log.e(TAG, "Bluetooth is not enabled.");
            stopSelf();
            return;
        }

        if (!mBluetoothAdapter.isMultipleAdvertisementSupported()) {
            Log.e(TAG, "BLE Advertising not supported.");
            stopSelf();
            return;
        }

        initialize();

        // 키 이벤트 수신을 위한 BroadcastReceiver 등록
        IntentFilter filter = new IntentFilter(ACTION_KEY_EVENT);
        registerReceiver(mKeyEventReceiver, filter, Context.RECEIVER_NOT_EXPORTED);

        // 페어링 상태 변경 수신을 위한 BroadcastReceiver 등록
        IntentFilter bondFilter = new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        registerReceiver(mBondingReceiver, bondFilter, Context.RECEIVER_EXPORTED);
    }

    private void initialize() {
        mBluetoothLeAdvertiser = mBluetoothAdapter.getBluetoothLeAdvertiser();
        startAdvertising();
        startServer();

        attemptAutoPairing();
    }

    private void startAdvertising() {
        Log.d(TAG, "startAdvertising");
        if (mBluetoothLeAdvertiser != null) {
            mBluetoothLeAdvertiser.stopAdvertising(mAdvertiseCallback);
        }

        AdvertiseSettings settings = new AdvertiseSettings.Builder()
                .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                .setConnectable(true)
                .setTimeout(0)
                .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_HIGH)
                .build();

        AdvertiseData data = new AdvertiseData.Builder()
                .setIncludeDeviceName(true)
                .addServiceUuid(new ParcelUuid(HID_SERVICE_UUID))
                .build();

        mBluetoothLeAdvertiser.startAdvertising(settings, data, mAdvertiseCallback);
    }


    private AdvertiseCallback mAdvertiseCallback = new AdvertiseCallback() {
        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            Log.i(TAG, "Advertise started successfully.");
        }

        @Override
        public void onStartFailure(int errorCode) {
            Log.e(TAG, "Advertise failed with error code: " + errorCode);
        }
    };

    private void startServer() {
        mGattServer = mBluetoothManager.openGattServer(this, mGattServerCallback);

        // HID Service
        BluetoothGattService hidService = new BluetoothGattService(HID_SERVICE_UUID, BluetoothGattService.SERVICE_TYPE_PRIMARY);

        // HID Information Characteristic
        BluetoothGattCharacteristic hidInfoCharacteristic = new BluetoothGattCharacteristic(
                HID_INFORMATION_UUID,
                BluetoothGattCharacteristic.PROPERTY_READ,
                BluetoothGattCharacteristic.PERMISSION_READ
        );
        hidInfoCharacteristic.setValue(HID_INFORMATION);

        // Report Map Characteristic
        BluetoothGattCharacteristic reportMapCharacteristic = new BluetoothGattCharacteristic(
                REPORT_MAP_UUID,
                BluetoothGattCharacteristic.PROPERTY_READ,
                BluetoothGattCharacteristic.PERMISSION_READ
        );
        reportMapCharacteristic.setValue(REPORT_MAP);

        // Protocol Mode Characteristic
        BluetoothGattCharacteristic protocolModeCharacteristic = new BluetoothGattCharacteristic(
                PROTOCOL_MODE_UUID,
                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
                BluetoothGattCharacteristic.PERMISSION_READ | BluetoothGattCharacteristic.PERMISSION_WRITE
        );
        protocolModeCharacteristic.setValue(new byte[]{0x01}); // Report Protocol Mode

        // Keyboard Report Characteristic (Input Report)
        mReportCharacteristic = new BluetoothGattCharacteristic(
                REPORT_UUID,
                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ
        );

        // Mouse Report Characteristic (Input Report)
        mMouseInputReportCharacteristic = new BluetoothGattCharacteristic(
                REPORT_UUID,
                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ
        );

        // Client Characteristic Configuration Descriptor (CCCD)
        BluetoothGattDescriptor clientConfigDescriptor = new BluetoothGattDescriptor(
                CLIENT_CHARACTERISTIC_CONFIG_UUID,
                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE
        );
        mReportCharacteristic.addDescriptor(clientConfigDescriptor);

        // Mouse CCCD
        BluetoothGattDescriptor mouseClientConfigDescriptor = new BluetoothGattDescriptor(
                CLIENT_CHARACTERISTIC_CONFIG_UUID,
                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE
        );
        mMouseInputReportCharacteristic.addDescriptor(mouseClientConfigDescriptor);

        // Report Reference Descriptor 추가 (Keyboard)
        BluetoothGattDescriptor reportReferenceDescriptor = new BluetoothGattDescriptor(
                REPORT_REFERENCE_UUID,
                BluetoothGattDescriptor.PERMISSION_READ
        );
        // Report ID = 1, Report Type = Input (1)
        byte[] reportReferenceValue = new byte[]{0x01, 0x01};
        reportReferenceDescriptor.setValue(reportReferenceValue);
        mReportCharacteristic.addDescriptor(reportReferenceDescriptor);

        // Report Reference Descriptor 추가 (Mouse)
        BluetoothGattDescriptor mouseReportReferenceDescriptor = new BluetoothGattDescriptor(
                REPORT_REFERENCE_UUID,
                BluetoothGattDescriptor.PERMISSION_READ
        );
// Report ID = 2, Report Type = Input (1)
        byte[] mouseReportReferenceValue = new byte[]{0x02, 0x01};
        mouseReportReferenceDescriptor.setValue(mouseReportReferenceValue);
        mMouseInputReportCharacteristic.addDescriptor(mouseReportReferenceDescriptor);

        // Boot Keyboard Input Report Characteristic
        BluetoothGattCharacteristic bootKeyboardInputReportCharacteristic = new BluetoothGattCharacteristic(
                BOOT_KEYBOARD_INPUT_REPORT_UUID,
                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ
        );
        BluetoothGattDescriptor bootClientConfigDescriptor = new BluetoothGattDescriptor(
                CLIENT_CHARACTERISTIC_CONFIG_UUID,
                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE
        );
        bootKeyboardInputReportCharacteristic.addDescriptor(bootClientConfigDescriptor);

        // Boot Mouse Input Report Characteristic
        // Boot Mouse Input Report Characteristic
        BluetoothGattCharacteristic bootMouseInputReportCharacteristic = new BluetoothGattCharacteristic(
                BOOT_MOUSE_INPUT_REPORT_UUID,
                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ
        );
// CCCD 추가
        BluetoothGattDescriptor bootMouseClientConfigDescriptor = new BluetoothGattDescriptor(
                CLIENT_CHARACTERISTIC_CONFIG_UUID,
                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE
        );
        bootMouseInputReportCharacteristic.addDescriptor(bootMouseClientConfigDescriptor);

// HID Service에 추가
        hidService.addCharacteristic(bootMouseInputReportCharacteristic);

        // Add characteristics to HID Service
        hidService.addCharacteristic(hidInfoCharacteristic);
        hidService.addCharacteristic(reportMapCharacteristic);
        hidService.addCharacteristic(protocolModeCharacteristic);
        hidService.addCharacteristic(mReportCharacteristic);
        hidService.addCharacteristic(mMouseInputReportCharacteristic);

        BluetoothGattCharacteristic hidControlPointCharacteristic = new BluetoothGattCharacteristic(
                UUID.fromString("00002A4C-0000-1000-8000-00805f9b34fb"),
                BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
                BluetoothGattCharacteristic.PERMISSION_WRITE
        );
        hidService.addCharacteristic(hidControlPointCharacteristic);

        mGattServer.addService(hidService);
    }

//    private void startServer() {
//        mGattServer = mBluetoothManager.openGattServer(this, mGattServerCallback);
//
//        // Generic Attribute Service 추가
//        BluetoothGattService gattService = new BluetoothGattService(
//                UUID.fromString("00001801-0000-1000-8000-00805f9b34fb"), // GATT Service UUID
//                BluetoothGattService.SERVICE_TYPE_PRIMARY
//        );
//
//        // Service Changed Characteristic
//        BluetoothGattCharacteristic serviceChangedCharacteristic = new BluetoothGattCharacteristic(
//                UUID.fromString("00002A05-0000-1000-8000-00805f9b34fb"), // Service Changed UUID
//                BluetoothGattCharacteristic.PROPERTY_INDICATE,
//                BluetoothGattCharacteristic.PERMISSION_READ
//        );
//        gattService.addCharacteristic(serviceChangedCharacteristic);
//
//        // GATT Server에 Generic Attribute Service 추가
//        mGattServer.addService(gattService);
//
//        // HID Service
//        BluetoothGattService hidService = new BluetoothGattService(HID_SERVICE_UUID, BluetoothGattService.SERVICE_TYPE_PRIMARY);
//
//        // HID Information Characteristic
//        BluetoothGattCharacteristic hidInfoCharacteristic = new BluetoothGattCharacteristic(
//                HID_INFORMATION_UUID,
//                BluetoothGattCharacteristic.PROPERTY_READ,
//                BluetoothGattCharacteristic.PERMISSION_READ
//        );
//        hidInfoCharacteristic.setValue(HID_INFORMATION);
//
//        // Report Map Characteristic
//        BluetoothGattCharacteristic reportMapCharacteristic = new BluetoothGattCharacteristic(
//                REPORT_MAP_UUID,
//                BluetoothGattCharacteristic.PROPERTY_READ,
//                BluetoothGattCharacteristic.PERMISSION_READ
//        );
//        reportMapCharacteristic.setValue(REPORT_MAP);
//
//        // Protocol Mode Characteristic
//        BluetoothGattCharacteristic protocolModeCharacteristic = new BluetoothGattCharacteristic(
//                PROTOCOL_MODE_UUID,
//                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE,
//                BluetoothGattCharacteristic.PERMISSION_READ | BluetoothGattCharacteristic.PERMISSION_WRITE
//        );
//        protocolModeCharacteristic.setValue(new byte[]{0x01}); // Report Protocol Mode
//
//        // Report Characteristic (Input Report)
//        mReportCharacteristic = new BluetoothGattCharacteristic(
//                REPORT_UUID,
//                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
//                BluetoothGattCharacteristic.PERMISSION_READ
//        );
//
//        // Client Characteristic Configuration Descriptor (CCCD)
//        BluetoothGattDescriptor clientConfigDescriptor = new BluetoothGattDescriptor(
//                CLIENT_CHARACTERISTIC_CONFIG_UUID,
//                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE
//        );
//        mReportCharacteristic.addDescriptor(clientConfigDescriptor);
//
//        // Report Reference Descriptor 추가
//        BluetoothGattDescriptor reportReferenceDescriptor = new BluetoothGattDescriptor(
//                REPORT_REFERENCE_UUID,
//                BluetoothGattDescriptor.PERMISSION_READ
//        );
//        // Report ID = 1, Report Type = Input (1)
//        byte[] reportReferenceValue = new byte[]{0x01, 0x01};
//        reportReferenceDescriptor.setValue(reportReferenceValue);
//        mReportCharacteristic.addDescriptor(reportReferenceDescriptor);
//
//        // Boot Keyboard Input Report Characteristic
//        BluetoothGattCharacteristic bootKeyboardInputReportCharacteristic = new BluetoothGattCharacteristic(
//                BOOT_KEYBOARD_INPUT_REPORT_UUID,
//                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
//                BluetoothGattCharacteristic.PERMISSION_READ
//        );
//        BluetoothGattDescriptor bootClientConfigDescriptor = new BluetoothGattDescriptor(
//                CLIENT_CHARACTERISTIC_CONFIG_UUID,
//                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE
//        );
//        bootKeyboardInputReportCharacteristic.addDescriptor(bootClientConfigDescriptor);
//
//        // Add characteristics to HID Service
//        hidService.addCharacteristic(hidInfoCharacteristic);
//        hidService.addCharacteristic(reportMapCharacteristic);
//        hidService.addCharacteristic(protocolModeCharacteristic);
//        hidService.addCharacteristic(mReportCharacteristic);
//        hidService.addCharacteristic(bootKeyboardInputReportCharacteristic);
//
//        mGattServer.addService(hidService);
//    }


    private BluetoothGattServerCallback mGattServerCallback = new BluetoothGattServerCallback() {
        @Override
        public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            super.onConnectionStateChange(device, status, newState);
            Log.d(TAG, "onConnectionStateChange: device=" + device.getAddress() + ", status=" + status + ", newState=" + newState);
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i(TAG, "Device connected: " + device.getAddress());
                mConnectedDevice = device;
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.i(TAG, "Device disconnected: " + device.getAddress());
                mConnectedDevice = null;
            }
        }

        @Override
        public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattCharacteristic characteristic) {
            super.onCharacteristicReadRequest(device, requestId, offset, characteristic);
            Log.d(TAG, "onCharacteristicReadRequest: device=" + device.getAddress() + ", characteristic=" + characteristic.getUuid().toString() + ", requestId=" + requestId + ", offset=" + offset);
            if (REPORT_MAP_UUID.equals(characteristic.getUuid())) {
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, REPORT_MAP);
            } else if (HID_INFORMATION_UUID.equals(characteristic.getUuid())) {
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, HID_INFORMATION);
            } else if (PROTOCOL_MODE_UUID.equals(characteristic.getUuid())) {
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, characteristic.getValue());
            } else {
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, characteristic.getValue());
            }
        }

        @Override
        public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
                                                 BluetoothGattCharacteristic characteristic,
                                                 boolean preparedWrite, boolean responseNeeded,
                                                 int offset, byte[] value) {
            super.onCharacteristicWriteRequest(device, requestId, characteristic, preparedWrite, responseNeeded, offset, value);
            Log.d(TAG, "onCharacteristicWriteRequest: device=" + device.getAddress() + ", characteristic=" + characteristic.getUuid().toString() + ", requestId=" + requestId + ", offset=" + offset + ", value=" + bytesToHex(value));

            if (characteristic.getUuid().equals(PROTOCOL_MODE_UUID)) {
                // Protocol Mode Write 처리
                characteristic.setValue(value);
                if (responseNeeded) {
                    mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value);
                }
            } else {
                if (responseNeeded) {
                    mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, null);
                }
            }
        }

        @Override
        public void onDescriptorReadRequest(BluetoothDevice device, int requestId, int offset, BluetoothGattDescriptor descriptor) {
            super.onDescriptorReadRequest(device, requestId, offset, descriptor);
            Log.d(TAG, "onDescriptorReadRequest: device=" + device.getAddress() + ", descriptor=" + descriptor.getUuid().toString() + ", requestId=" + requestId + ", offset=" + offset);
            if (CLIENT_CHARACTERISTIC_CONFIG_UUID.equals(descriptor.getUuid())) {
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, descriptor.getValue());
            } else if (REPORT_REFERENCE_UUID.equals(descriptor.getUuid())) {
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, descriptor.getValue());
            } else {
                mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null);
            }
        }

        @Override
        public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
                                             BluetoothGattDescriptor descriptor,
                                             boolean preparedWrite, boolean responseNeeded,
                                             int offset, byte[] value) {
            super.onDescriptorWriteRequest(device, requestId, descriptor, preparedWrite, responseNeeded, offset, value);
            Log.d(TAG, "onDescriptorWriteRequest: device=" + device.getAddress() + ", descriptor=" + descriptor.getUuid().toString() + ", requestId=" + requestId + ", offset=" + offset + ", value=" + bytesToHex(value));
            if (descriptor.getUuid().equals(CLIENT_CHARACTERISTIC_CONFIG_UUID)) {
                if (descriptor.getCharacteristic().getUuid().equals(REPORT_UUID) ||
                        descriptor.getCharacteristic().getUuid().equals(BOOT_KEYBOARD_INPUT_REPORT_UUID)) {
                    descriptor.setValue(value);
                    if (responseNeeded) {
                        mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, value);
                    }
                    Log.d(TAG, "Notifications enabled for " + descriptor.getCharacteristic().getUuid().toString());
                } else {
                    if (responseNeeded) {
                        mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null);
                    }
                }
            } else {
                if (responseNeeded) {
                    mGattServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null);
                }
            }
        }
    };

    // 키 이벤트를 수신하는 BroadcastReceiver
    private BroadcastReceiver mKeyEventReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (ACTION_KEY_EVENT.equals(intent.getAction())) {
                int keyIndex = intent.getIntExtra(EXTRA_KEY_INDEX, -1);
                boolean pressed = intent.getBooleanExtra(EXTRA_KEY_PRESSED, false);

                if (keyIndex >= 0 && keyIndex < keyPressState.length) {
                    keyPressState[keyIndex] = pressed;
                    sendReport();
                }
            }
        }
    };

    public void sendMouseInput(int absoluteX, int absoluteY, int wheelDelta, boolean leftButton, boolean rightButton, boolean middleButton) {
        if (mConnectedDevice == null) {
            Log.w(TAG, "No connected device to send mouse input.");
            return;
        }

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            Log.e(TAG, "BLUETOOTH_CONNECT permission not granted");
            return;
        }

        // 마우스 버튼 상태 설정
        byte buttons = 0;
        if (leftButton) {
            buttons |= (1 << 0); // Left button
        }
        if (rightButton) {
            buttons |= (1 << 1); // Right button
        }
        if (middleButton) {
            buttons |= (1 << 2); // Middle button
        }

        // 마우스 리포트 생성
        byte[] report = new byte[6];
        report[0] = buttons;                       // 버튼 상태
        report[1] = (byte) (absoluteX & 0xFF);     // X 좌표 하위 바이트
        report[2] = (byte) ((absoluteX >> 8) & 0xFF); // X 좌표 상위 바이트
        report[3] = (byte) (absoluteY & 0xFF);     // Y 좌표 하위 바이트
        report[4] = (byte) ((absoluteY >> 8) & 0xFF); // Y 좌표 상위 바이트
        report[5] = (byte) wheelDelta;             // 휠 동작 (음수: 아래로, 양수: 위로)

//        Log.d(TAG, "sendMouseInput(): " + bytesToHex(report));

        // 마우스 리포트 전송
        mMouseInputReportCharacteristic.setValue(report);
        mGattServer.notifyCharacteristicChanged(mConnectedDevice, mMouseInputReportCharacteristic, false);
    }


    public void sendReport(byte[] report) {
        if (mConnectedDevice == null) {
            Log.w(TAG, "No connected device to send report.");
            return;
        }

        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_CONNECT
        ) != PackageManager.PERMISSION_GRANTED) {
            Log.e(TAG, "BLUETOOTH_CONNECT permission not granted");
            return;
        }

//        Log.d(TAG, "sendReport(): " + bytesToHex(report));

        mReportCharacteristic.setValue(report);

        if (mGattServer != null) {
            mGattServer.notifyCharacteristicChanged(mConnectedDevice, mReportCharacteristic, false);
        }
    }

    private void sendReport() {
        if (mConnectedDevice == null) {
            Log.w(TAG, "No connected device to send report.");
            return;
        }

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADMIN) != PackageManager.PERMISSION_GRANTED) {
            Log.e(TAG, "BLUETOOTH_CONNECT permission not granted");
            return;
        }

        // keyPressState 배열을 기반으로 리포트 생성
        byte modifier = 0;
        byte[] keyCodes = new byte[6];

        // Modifier keys 처리
        if (keyPressState[51]) {
            modifier |= (1 << 1); // Left Shift
        }
        if (keyPressState[52]) {
            modifier |= (1 << 0); // Left Ctrl
        }
        if (keyPressState[53]) {
            modifier |= (1 << 2); // Left Alt
        }

        int keyCount = 0;
        for (int i = 0; i < 51 && keyCount < 6; ++i) {
            if (keyPressState[i]) {
                byte keyCode = 0;
                // i를 keyCode로 매핑 (이전 코드와 동일)
                switch (i){
                    case 0:
                        keyCode = 0x14;
                        break;
                    case 1:
                        keyCode = 0x1a;
                        break;
                    case 2:
                        keyCode = 0x08;
                        break;
                    case 3:
                        keyCode = 0x15;
                        break;
                    case 4:
                        keyCode = 0x17;
                        break;
                    case 5:
                        keyCode = 0x1c;
                        break;
                    case 6:
                        keyCode = 0x18;
                        break;
                    case 7:
                        keyCode = 0x0c;
                        break;
                    case 8:
                        keyCode = 0x12;
                        break;
                    case 9:
                        keyCode = 0x13;
                        break;
                    case 10:
                        keyCode = 0x2f;
                        break;
                    case 11:
                        keyCode = 0x30;
                        break;
                    case 12:
                        keyCode = 0x04;
                        break;
                    case 13:
                        keyCode = 0x16;
                        break;
                    case 14:
                        keyCode = 0x07;
                        break;
                    case 15:
                        keyCode = 0x09;
                        break;
                    case 16:
                        keyCode = 0x0a;
                        break;
                    case 17:
                        keyCode = 0x0b;
                        break;
                    case 18:
                        keyCode = 0x0d;
                        break;
                    case 19:
                        keyCode = 0x0e;
                        break;
                    case 20:
                        keyCode = 0x0f;
                        break;
                    case 21:
                        keyCode = 0x33;
                        break;
                    case 22:
                        keyCode = 0x34;
                        break;
                    case 23:
                        keyCode = 0x1d;
                        break;
                    case 24:
                        keyCode = 0x1b;
                        break;
                    case 25:
                        keyCode = 0x06;
                        break;
                    case 26:
                        keyCode = 0x19;
                        break;
                    case 27:
                        keyCode = 0x05;
                        break;
                    case 28:
                        keyCode = 0x11;
                        break;
                    case 29:
                        keyCode = 0x10;
                        break;
                    case 30:
                        keyCode = 0x36;
                        break;
                    case 31:
                        keyCode = 0x37;
                        break;
                    case 32:
                        keyCode = 0x38;
                        break;
                    case 33:
                        keyCode = 0x28; // Enter
                        break;
                    case 34:
                        keyCode = 0x2a; // Backspace
                        break;
                    case 35:
                        keyCode = (byte) 0x90; // Custom key
                        break;
                    case 36:
                        keyCode = 0x1e; // '1'
                        break;
                    case 37:
                        keyCode = 0x1f; // '2'
                        break;
                    case 38:
                        keyCode = 0x20; // '3'
                        break;
                    case 39:
                        keyCode = 0x21; // '4'
                        break;
                    case 40:
                        keyCode = 0x22; // '5'
                        break;
                    case 41:
                        keyCode = 0x23; // '6'
                        break;
                    case 42:
                        keyCode = 0x24; // '7'
                        break;
                    case 43:
                        keyCode = 0x25; // '8'
                        break;
                    case 44:
                        keyCode = 0x26; // '9'
                        break;
                    case 45:
                        keyCode = 0x27; // '0'
                        break;
                    case 46:
                        keyCode = 0x2d; // '-'
                        break;
                    case 47:
                        keyCode = 0x2e; // '='
                        break;
                    case 48:
                        keyCode = 0x31; // '['
                        break;
                    case 49:
                        keyCode = 0x2c; // ' '
                        break;
                    case 50:
                        keyCode = 0x2b; // '\'
                        break;
                }
                keyCodes[keyCount++] = keyCode;
            }
        }

        byte[] report = new byte[8];
        report[0] = modifier;
        report[1] = 0; // Reserved byte
        System.arraycopy(keyCodes, 0, report, 2, keyCodes.length);

        Log.d(TAG, "sendReport(): " + bytesToHex(report));

        mReportCharacteristic.setValue(report);
        mGattServer.notifyCharacteristicChanged(mConnectedDevice, mReportCharacteristic, false);
    }

    // 페어링된 기기 정보 저장
    private void saveBondedDevice(BluetoothDevice device) {
        SharedPreferences prefs = getSharedPreferences("bonded_devices", MODE_PRIVATE);
        prefs.edit().putString("device_address", device.getAddress()).apply();
    }

    // 페어링된 기기 정보 삭제
    private void removeBondedDevice() {
        SharedPreferences prefs = getSharedPreferences("bonded_devices", MODE_PRIVATE);
        prefs.edit().remove("device_address").apply();
    }

    // 페어링된 기기 정보 가져오기
    private BluetoothDevice getBondedDevice() {
        SharedPreferences prefs = getSharedPreferences("bonded_devices", MODE_PRIVATE);
        String deviceAddress = prefs.getString("device_address", null);
        if (deviceAddress != null) {
            return mBluetoothAdapter.getRemoteDevice(deviceAddress);
        }
        return null;
    }

    // 자동 페어링 시도
    private void attemptAutoPairing() {
        BluetoothDevice device = getBondedDevice();
        if (device != null) {
            if (device.getBondState() != BluetoothDevice.BOND_BONDED) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADMIN) != PackageManager.PERMISSION_GRANTED) {
                    Log.e(TAG, "BLUETOOTH_CONNECT permission not granted");
                    return;
                }
                device.createBond();
            }
            mConnectedDevice = device;
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // 리소스 정리
        unregisterReceiver(mKeyEventReceiver);
        unregisterReceiver(mBondingReceiver);
        if (mGattServer != null) {
            mGattServer.close();
        }
        if (mBluetoothLeAdvertiser != null) {
            try {
                mBluetoothLeAdvertiser.stopAdvertising(mAdvertiseCallback);
            } catch (Exception e) {
                Log.e(TAG, "Failed to stop advertising: " + e.getMessage());
            }
        }
    }

    // 포그라운드 서비스 설정
    private void startForegroundService() {
        Log.d(TAG, "asdfsdf");
        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, PendingIntent.FLAG_IMMUTABLE);
        } else {
            pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, PendingIntent.FLAG_IMMUTABLE);
        }

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("BLE HID Keyboard")
                .setContentText("Service is running")
                .setSmallIcon(R.drawable.baseline_location_on_24)
                .setContentIntent(pendingIntent);

        startForeground(1, builder.build());
    }

    private void createNotificationChannel() {
        // Android O 이상에서 Notification Channel 생성
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = "GattServerService";
            String description = "BLE HID Keyboard Service";
            int importance = NotificationManager.IMPORTANCE_LOW;
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, name, importance);
            channel.setDescription(description);

            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }

    // 유틸리티 메서드
    private static String bytesToHex(byte[] bytes) {
        if (bytes == null)
            return "null";
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02X ", b));
        }
        return sb.toString();
    }
}
