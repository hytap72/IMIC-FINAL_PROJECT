package com.imic.robot

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.imic.robot.databinding.ActivityScanBinding

@SuppressLint("MissingPermission")
class ScanActivity : AppCompatActivity() {

    private lateinit var binding: ActivityScanBinding
    private lateinit var ble: BleManager
    private lateinit var bluetoothAdapter: BluetoothAdapter

    companion object {
        const val EXTRA_DEVICE = "extra_device"
        const val REQ_PERM = 100
        const val REQ_ENABLE_BT = 101
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityScanBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val btManager = getSystemService(BluetoothManager::class.java)
        bluetoothAdapter = btManager.adapter

        ble = BleManager.getInstance(this)

        ble.onDeviceFound = { device ->
            runOnUiThread {
                ble.stopScan()
                binding.tvStatus.text = "Tìm thấy: ${device.name}\nĐang kết nối..."
                connectAndProceed(device)
            }
        }

        binding.btnScan.setOnClickListener {
            when {
                !bluetoothAdapter.isEnabled -> {
                    startActivityForResult(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE), REQ_ENABLE_BT)
                }
                !isLocationEnabled() -> {
                    Toast.makeText(this, "Vui lòng bật GPS/Location để scan BLE", Toast.LENGTH_LONG).show()
                    startActivity(Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS))
                }
                checkPermissions() -> startScan()
            }
        }
    }

    private fun isLocationEnabled(): Boolean {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) return true
        val lm = getSystemService(LocationManager::class.java)
        return lm.isProviderEnabled(LocationManager.GPS_PROVIDER) ||
               lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
    }

    private fun startScan() {
        binding.tvStatus.text = "Đang tìm IMIC_Robot..."
        ble.startScan()
    }

    private fun connectAndProceed(device: BluetoothDevice) {
        ble.onConnected = {
            runOnUiThread {
                val intent = Intent(this, ControlActivity::class.java)
                intent.putExtra(EXTRA_DEVICE, device)
                startActivity(intent)
            }
        }
        ble.connect(device)
    }

    private fun checkPermissions(): Boolean {
        val perms = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        val missing = perms.filter {
            ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        return if (missing.isEmpty()) {
            true
        } else {
            ActivityCompat.requestPermissions(this, missing.toTypedArray(), REQ_PERM)
            false
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>,
                                            grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQ_PERM && grantResults.all { it == PackageManager.PERMISSION_GRANTED }) {
            startScan()
        } else {
            Toast.makeText(this, "Cần cấp đủ quyền Bluetooth để tiếp tục", Toast.LENGTH_LONG).show()
        }
    }

    @Deprecated("Deprecated in Java")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQ_ENABLE_BT && bluetoothAdapter.isEnabled) {
            if (checkPermissions()) startScan()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        ble.stopScan()
    }
}
