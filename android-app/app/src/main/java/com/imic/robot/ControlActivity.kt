package com.imic.robot

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.MotionEvent
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.imic.robot.databinding.ActivityControlBinding

@SuppressLint("MissingPermission", "ClickableViewAccessibility")
class ControlActivity : AppCompatActivity() {

    private lateinit var binding: ActivityControlBinding
    private lateinit var ble: BleManager

    /* WiFi status codes khớp với ble_manager.h */
    private val STATUS_IDLE       = 0x00.toByte()
    private val STATUS_CONNECTING = 0x01.toByte()
    private val STATUS_CONNECTED  = 0x02.toByte()
    private val STATUS_FAILED     = 0x03.toByte()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityControlBinding.inflate(layoutInflater)
        setContentView(binding.root)

        ble = BleManager.getInstance(this)

        /* Nhận notify trạng thái WiFi từ ESP32 */
        ble.onWifiStatus = { status ->
            runOnUiThread {
                binding.tvWifiStatus.text = when (status) {
                    STATUS_CONNECTING -> "WiFi: Đang kết nối..."
                    STATUS_CONNECTED  -> "WiFi: Đã kết nối ✓"
                    STATUS_FAILED     -> "WiFi: Thất bại ✗"
                    else              -> "WiFi: Chờ..."
                }
            }
        }

        ble.onDisconnected = {
            runOnUiThread {
                Toast.makeText(this, "BLE đã ngắt kết nối", Toast.LENGTH_SHORT).show()
                finish()
            }
        }

        setupWifiSection()
        setupMotorSection()
    }

    /* ── WiFi Provision ──────────────────────────────── */
    private fun setupWifiSection() {
        binding.btnConnectWifi.setOnClickListener {
            val ssid = binding.etSsid.text.toString().trim()
            val pass = binding.etPassword.text.toString()

            if (ssid.isEmpty()) {
                Toast.makeText(this, "Nhập SSID", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            binding.tvWifiStatus.text = "WiFi: Đang gửi thông tin..."
            ble.sendWifiCredentials(ssid, pass)
        }
    }

    /* ── Motor Control ───────────────────────────────── */
    private fun setupMotorSection() {
        /* Giữ nút → gửi lệnh, thả → STOP */
        fun holdButton(view: android.view.View, cmd: Byte) {
            view.setOnTouchListener { _, event ->
                when (event.action) {
                    MotionEvent.ACTION_DOWN -> ble.sendMotorCmd(cmd)
                    MotionEvent.ACTION_UP,
                    MotionEvent.ACTION_CANCEL -> ble.sendMotorCmd(MotorCmd.STOP)
                }
                true
            }
        }

        holdButton(binding.btnForward,  MotorCmd.FORWARD)
        holdButton(binding.btnBackward, MotorCmd.BACKWARD)
        holdButton(binding.btnLeft,     MotorCmd.LEFT)
        holdButton(binding.btnRight,    MotorCmd.RIGHT)

        binding.btnRun.setOnClickListener  { ble.sendMotorCmd(MotorCmd.RUN) }
        binding.btnStop.setOnClickListener { ble.sendMotorCmd(MotorCmd.STOP) }
    }

    override fun onDestroy() {
        super.onDestroy()
        ble.disconnect()
    }
}
