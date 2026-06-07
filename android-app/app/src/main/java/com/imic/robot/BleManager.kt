package com.imic.robot

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.util.Log
import java.util.UUID



/* UUID phải khớp với ble_manager.c bên ESP32 */
object BleUUID {
    val SVC_WIFI       = UUID.fromString("0000aa01-0000-1000-8000-00805f9b34fb")
    val CHAR_SSID      = UUID.fromString("0000aa11-0000-1000-8000-00805f9b34fb")
    val CHAR_PASSWORD  = UUID.fromString("0000aa12-0000-1000-8000-00805f9b34fb")
    val CHAR_STATUS    = UUID.fromString("0000aa13-0000-1000-8000-00805f9b34fb")

    val SVC_MOTOR      = UUID.fromString("0000aa02-0000-1000-8000-00805f9b34fb")
    val CHAR_CMD       = UUID.fromString("0000aa21-0000-1000-8000-00805f9b34fb")

    val CCCD           = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}

object MotorCmd {
    const val FORWARD  = 0x01.toByte()
    const val BACKWARD = 0x02.toByte()
    const val LEFT     = 0x03.toByte()
    const val RIGHT    = 0x04.toByte()
    const val RUN      = 0x05.toByte()
    const val STOP     = 0x06.toByte()
}

@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {

    companion object {
        @Volatile private var instance: BleManager? = null

        fun getInstance(context: Context): BleManager =
            instance ?: synchronized(this) {
                instance ?: BleManager(context.applicationContext).also { instance = it }
            }
    }

    private val TAG = "BleManager"

    private var gatt: BluetoothGatt? = null
    private var wifiSvc: BluetoothGattService? = null
    private var motorSvc: BluetoothGattService? = null

    var onDeviceFound: ((BluetoothDevice) -> Unit)? = null
    var onConnected: (() -> Unit)? = null
    var onDisconnected: (() -> Unit)? = null
    var onWifiStatus: ((Byte) -> Unit)? = null

    /* ── Scan ──────────────────────────────────────────── */
    private val adapter: BluetoothAdapter by lazy {
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    }
    private val scanner by lazy { adapter.bluetoothLeScanner }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.device.name ?: return
            if (name == "IMIC_Robot") {
                onDeviceFound?.invoke(result.device)
            }
        }
    }

    fun startScan() {
        Log.d(TAG, "Start scanning...")
        scanner.startScan(scanCallback)
    }

    fun stopScan() {
        scanner.stopScan(scanCallback)
    }

    /* ── Connect ───────────────────────────────────────── */
    fun connect(device: BluetoothDevice) {
        gatt = device.connectGatt(context, false, gattCallback)
    }

    fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
    }

    /* ── GATT Callback ─────────────────────────────────── */
    private val gattCallback = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "Connected, discovering services...")
                g.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Disconnected")
                onDisconnected?.invoke()
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            wifiSvc  = g.getService(BleUUID.SVC_WIFI)
            motorSvc = g.getService(BleUUID.SVC_MOTOR)

            /* Bật notify cho Status characteristic */
            wifiSvc?.getCharacteristic(BleUUID.CHAR_STATUS)?.let { char ->
                g.setCharacteristicNotification(char, true)
                char.getDescriptor(BleUUID.CCCD)?.let { desc ->
                    desc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    g.writeDescriptor(desc)
                }
            }

            Log.d(TAG, "Services discovered")
            onConnected?.invoke()
        }

        override fun onCharacteristicChanged(g: BluetoothGatt,
                                             char: BluetoothGattCharacteristic) {
            if (char.uuid == BleUUID.CHAR_STATUS) {
                val status = char.value?.firstOrNull() ?: return
                Log.d(TAG, "WiFi status notify: $status")
                onWifiStatus?.invoke(status)
            }
        }
    }

    /* ── Write helpers ─────────────────────────────────── */
    fun sendWifiCredentials(ssid: String, password: String) {
        writeChar(BleUUID.SVC_WIFI, BleUUID.CHAR_SSID, ssid.toByteArray())
        /* Delay nhỏ để ESP32 xử lý xong SSID trước */
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
            writeChar(BleUUID.SVC_WIFI, BleUUID.CHAR_PASSWORD, password.toByteArray())
        }, 300)
    }

    fun sendMotorCmd(cmd: Byte) {
        writeChar(BleUUID.SVC_MOTOR, BleUUID.CHAR_CMD, byteArrayOf(cmd))
    }

    private fun writeChar(svcUUID: UUID, charUUID: UUID, value: ByteArray) {
        val g = gatt ?: return
        val svc = g.getService(svcUUID) ?: run {
            Log.e(TAG, "Service $svcUUID not found"); return
        }
        val char = svc.getCharacteristic(charUUID) ?: run {
            Log.e(TAG, "Characteristic $charUUID not found"); return
        }
        char.value = value
        g.writeCharacteristic(char)
    }
}
