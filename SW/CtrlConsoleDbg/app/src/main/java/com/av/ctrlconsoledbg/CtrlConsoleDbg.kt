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

// Message received from UI. Upon reception generalized to CcdNodeInfo and sent to the device
data class NodeLedLightChannelInfo(
        var disabled: Boolean,
        var intensity: Int
)

data class NodeLedLightInfo(
        var mode: Boolean,
        var channels: Array<NodeLedLightChannelInfo>
)

class CcdNodeLedLight(addr: Int, data: ByteArray) : CcdNode(addr, CcdNodeType.LEDLIGHT) {
    // Parameters received from the Node
    var info: NodeLedLightInfo

    // Parameters configured by user from UI
    var userInfo : NodeLedLightInfo

    init {

        info = NodeLedLightInfo(    // Default value
                mode = false,
                channels = arrayOf(
                        NodeLedLightChannelInfo( disabled = false, intensity = 0),
                        NodeLedLightChannelInfo( disabled = false, intensity = 0),
                        NodeLedLightChannelInfo( disabled = false, intensity = 0)))

        // Update Info from binary data
        update(data)
        userInfo = info
    }

    override fun pack(): ByteArray? {
        val disabledMask =
                (if (userInfo.channels[0].disabled) 1 else 0) +
                (if (userInfo.channels[1].disabled) 2 else 0) +
                (if (userInfo.channels[2].disabled) 4 else 0)
        return byteArrayOf(
                if (userInfo.mode) 1 else 0,
                disabledMask.toByte(),
                userInfo.channels[0].intensity.toByte(),
                userInfo.channels[1].intensity.toByte(),
                userInfo.channels[2].intensity.toByte())
    }

    override fun update(data: ByteArray?) {
        if (data == null) return

        info = NodeLedLightInfo(
                mode = (data[0].toInt() != 0),
                channels = arrayOf(
                        NodeLedLightChannelInfo( disabled = (data[1].and(0x01).toInt() != 0) , intensity = data[2].toInt()),
                        NodeLedLightChannelInfo( disabled = (data[1].and(0x02).toInt() != 0) , intensity = data[3].toInt()),
                        NodeLedLightChannelInfo( disabled = (data[1].and(0x04).toInt() != 0) , intensity = data[4].toInt())))
    }

    companion object {
        private const val TAG = "LedLightNode"
    }
}

data class NodeSwitchInfo(
    var state: Int = 0,
    var dstAddr: Int
)

class CcdNodeSwitch(addr: Int) : CcdNode(addr, CcdNodeType.SWITCH) {
    // Parameters received from the Node
    var info: NodeSwitchInfo? = null

    // Parameters configured by user from UI
    var userState = 0
    var userAddr = 0

    override fun pack(): ByteArray? {
        TODO("not implemented") //To change body of created functions use File | Settings | File Templates.
    }

    override fun update(data: ByteArray?) {
        if (data?.size != 2) return
        info = NodeSwitchInfo(
                state = data[0].toInt(),
                dstAddr = data[1].toInt())
    }
    companion object {
        private const val TAG = "SwitchNode"
    }
}

data class NodeHvacInfo(
    var temp: Int = 0,
    var pressure: Int = 0,
    var humidity: Int = 0,
    var motorActual: Int = 0,
    var motor: Int = 0,
    var heaterActual: Int = 0,
    var heater: Int = 0,
    var valveActual: Int = 0,
    var valve: Int = 0
)
class CcdNodeHvac(addr: Int) : CcdNode(addr, CcdNodeType.HVAC) {
    // Parameters received from the Node
    var info: NodeHvacInfo? = null

    // Parameters configured by user from UI
    var userMotor = 0
    var userHeater = SpannableStringBuilder("0%")
    var userValve = 0
    // var userTemp:Int
    // var smokeTimer: Int

    override fun pack(): ByteArray? {
        TODO("not implemented") //To change body of created functions use File | Settings | File Templates.
    }

    override fun update(data: ByteArray?) {
        if (data?.size != 9) {
            Log.e(TAG, "Bad HVAC data")
            return
        }
        info = NodeHvacInfo(
                temp = data[0].toInt(),
                pressure = data[1].toInt(),
                humidity = data[2].toInt())
    }
    companion object {
        private const val TAG = "HvacNode"
    }
}

abstract class CcdNode(
        val addr: Int,
        val type: CcdNodeType) {

    abstract fun update(data: ByteArray?)
    abstract fun pack() : ByteArray?

}
enum class CcdNodeType(v: Int) {
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
                    node = CcdNodeHvac(msg.addr)
                }
                CcdNodeType.SWITCH -> {
                    node = CcdNodeSwitch(msg.addr)
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
