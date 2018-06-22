package com.av.ctrlconsoledbg

import android.bluetooth.BluetoothGattDescriptor
import android.support.v7.app.AppCompatActivity
import android.os.Bundle
import android.support.v4.app.Fragment
import android.text.SpannableStringBuilder
import android.util.Log
import com.av.bleservice.AvBleService
import com.av.bleservice.CcdBleDeviceDiscovered
import com.av.ctrlconsoledbg.fragments.CtrlConsoleSelectorFragment
import com.av.uart.AvUart
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode
import kotlin.experimental.and

data class CcdUartConnected(val dummy:Int = 0)
data class CcdNodeInfoGet(val addr: Int = 0)

// xxxInfoSet -> Msg from UI to the node
// xxxInfoReq -> Empty msg to the node. Node should respond with NodeInfo soon
// xxxInfoResp -> Msg from the node.

// Message passed to device from main
data class CcdNodeInfoReq(val dummy: Int) {
    val addr: Int = 0
}

// Message passed to device from main
data class CcdNodeInfoSet(val dummy: Int) {
    val addr: Int = 0
    val type: CcdNodeType = CcdNodeType.HVAC
    val data: ByteArray? = null
}

// Message received from the device
data class CcdNodeInfoResp(
    val addr: Int,
    val type: CcdNodeType,
    val data: ByteArray
)

abstract class CcdNode(
        val addr: Int,
        val type: CcdNodeType) {

    abstract fun update(data: ByteArray?)
    abstract fun pack() : ByteArray?

}
enum class CcdNodeType(val type: Int) {
    NONE(0x00),
    HVAC(0x10),
    LEDLIGHT(0x20),
    SWITCH(0x30),
    CTRLCON(0x40)
}

class CtrlConsoleDbgActivity : AppCompatActivity() {

    private var mAvBleService: AvBleService? = null

    var nodes: MutableMap<Int, CcdNode> = mutableMapOf()
    var uart = AvUart(this)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_ctrl_console_dbg)
        EventBus.getDefault().register(this)

        uart.initialize()
        uart.startPollThread()

        supportFragmentManager.beginTransaction()
                .replace(R.id.ctrl_console_frame_selector, CtrlConsoleSelectorFragment(), CtrlConsoleSelectorFragment::class.java.name)
                .commit()

    }


    override fun onDestroy() {
        super.onDestroy()
        EventBus.getDefault().unregister(this)
        uart.stopPollThread()
        uart.deinitialize()
    }

    @Subscribe(threadMode = ThreadMode.BACKGROUND)
    fun onEvent(bt_dev: CcdBleDeviceDiscovered) {

        Log.d(TAG, "BLE CCD service discovered")
        mAvBleService?.writeCharacteristicDescr(
                bt_dev.address,
                AvBleService.UUID_BLE_CH_CCD_NODE,
                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onEvent(ch: AvBleService.ChIn) {

        Log.d(TAG, "BLE CCD CH Received")

        when(ch.uuid) {
            AvBleService.UUID_BLE_CH_CCD_NODE-> {
                // TODO: Functionality
            }
        }
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onEvent(msg: CcdUartConnected) {

        Log.d(TAG, "CCD Uart connected")
        // Send broadcast NodeInfoGet
        EventBus.getDefault().post(CcdNodeInfoGet(addr = -1))
    }


    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onEvent(msg: CcdNodeInfoResp) {

        var node: CcdNode? = null
        Log.d(TAG, "Node Resp ${msg.addr}:${msg.type}")

        // check is node already exist
        node = nodes[msg.addr]
        if (node == null) {
            // Add node if not exist
            when (msg.type) {
                CcdNodeType.HVAC -> {
                    node = CcdNodeHvac(msg.addr, msg.data)
                }
                CcdNodeType.SWITCH -> {
                    node = CcdNodeSwitch(msg.addr, msg.data)
                }
                CcdNodeType.LEDLIGHT -> {
                    node = CcdNodeLedLight(msg.addr, msg.data)
                }
                else -> {
                    Log.d(TAG, "Unknow node type - ${msg.type}, from ${msg.addr}")
                    return
                }
            }
            nodes[msg.addr] = node
        } else {
            node.update(msg.data)
        }

        // Send notification to UI
        EventBus.getDefault().post(node)
    }

    companion object {
        const val TAG = "CcdActivity"
    }

}
