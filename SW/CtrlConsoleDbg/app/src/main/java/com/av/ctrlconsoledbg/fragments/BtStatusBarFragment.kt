package com.av.btstatusbar.fragment

import android.app.Activity
import android.os.Bundle
import android.view.View
import kotlinx.android.synthetic.main.bt_status_bar_fragment_layout.*
import org.greenrobot.eventbus.EventBus
import android.bluetooth.BluetoothAdapter
import android.content.*
import android.os.IBinder
import android.support.v4.app.Fragment
import android.util.Log
import android.view.LayoutInflater
import android.view.ViewGroup
import com.av.bleservice.AvBleService
import com.av.ctrlconsoledbg.R
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

data class BtStatusBarInfoEvent(val state: Int? = null,
                                val msg : String? = null,
                                val dev_num : Int? = null,
                                val evt: String? = null) {
    companion object {
        const val BLE_STATUS_BAR_STATE_OFF = 0
        const val BLE_STATUS_BAR_STATE_ON = 1
        const val BLE_STATUS_BAR_STATE_PERMISSIONS = 2
    }
}

class BtStatusBarFragment : Fragment() {

    private val TAG = BtStatusBarFragment::class.java.simpleName
    init {
        Log.d(TAG, " BtStatusBarFragment - initialized")
    }
    private val REQUEST_ENABLE_BT: Int = 123

    private val mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter()
    private var mAvBleService: AvBleService? = null

    private var mServiceConnection: ServiceConnection? = object : ServiceConnection {
        override fun onServiceConnected(componentName: ComponentName, service: IBinder) {
            mAvBleService = (service as AvBleService.LocalBinder).mAvBleService
            if (mAvBleService?.initialize() != true) {
                Log.e(TAG, "Unable to initialize Bluetooth")
            }
        }

        override fun onServiceDisconnected(componentName: ComponentName) {
            mAvBleService?.deinitialize()
            mAvBleService = null
            Log.i(TAG, "Bluetooth Service disconnected")
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (mServiceConnection != null) {
            context.unbindService(mServiceConnection)
            mServiceConnection = null
        }

        // Should be cleaned in onServiceDisconnected()
        mAvBleService?.deinitialize()
        mAvBleService = null
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val gattServiceIntent = Intent(context, mAvBleService!!::class.java)
        context.bindService(gattServiceIntent, mServiceConnection, Context.BIND_AUTO_CREATE)
        bt_en?.setOnClickListener {
            val bt_enabled = mBluetoothAdapter.isEnabled
            if (bt_enabled) {
                mBluetoothAdapter.disable()
            } else {
                val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
            }
        }

        // Enable Bluetooth by default
        if (mBluetoothAdapter != null) {
            val bt_enabled = mBluetoothAdapter.isEnabled
            if (!bt_enabled) {
                val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
            }
        }
    }

    override fun onCreateView(inflater: LayoutInflater?, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        EventBus.getDefault().register(this)
        val view = inflater?.inflate(R.layout.bt_status_bar_fragment_layout, container, false)

        return view
    }

    override fun onViewCreated(view: View?, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (mBluetoothAdapter == null) {
            bt_status.text = "BT not supported"
        }
        bt_device_evt.text = ""
        bt_device_num.text = ""

    }

    override fun onDestroyView() {
        super.onDestroyView()
        EventBus.getDefault().unregister(this)

    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_ENABLE_BT) {
            if (resultCode != Activity.RESULT_OK) {
                bt_status.text = "BT not allowed"
            }
        }
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onBtStatusBarChanged(info: BtStatusBarInfoEvent) {

        if (info.state != null) {
            when (info.state) {
                BtStatusBarInfoEvent.BLE_STATUS_BAR_STATE_ON -> {
                    // Set BT icon
                }
            }
        }
        if (info.msg != null) {
            bt_status.text = info.msg
        }

        // Don't update device num if not specified
        if (info.dev_num != null) {
            if (info.dev_num > 0) {
                bt_device_num.text = "(${info.dev_num})"
            } else {
                bt_device_num.text = ""
            }
        }

        if (info.evt != null) {
            bt_device_evt.text = info.evt
        } else {
            bt_device_evt.text = ""
        }

    }


}