package com.av.ctrlconsoledbg

import android.bluetooth.BluetoothGattDescriptor
import android.support.v7.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import com.av.bleservice.AvBleService
import com.av.bleservice.CcdBleDeviceDiscovered
import com.av.ctrlconsoledbg.fragments.CtrlConsoleSelectorFragment
import com.av.uart.AvUart
import com.av.uart.AvUartTxMsg
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

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

    open fun pack(data: ByteArray? = null, dest: Int? = null) : ByteArray {
        val hdr = ByteArray(CCD_NODE_INFO_DATA)

        hdr[CCD_NODE_INFO_FROM] = 0x90.toByte() // From CtrlCon
        hdr[CCD_NODE_INFO_TO]   = (dest?:addr).toByte()
        hdr[CCD_NODE_INFO_CMD]  = 0
        hdr[CCD_NODE_INFO_TYPE] = type.v.toByte()
        hdr[CCD_NODE_INFO_LEN]  = data?.size?.toByte()?:0
        return (hdr + (data?:byteArrayOf()))
    }

    var userInfoChanges: Int = 0
    var userInfoChangesLastTx: Int = 0

    companion object {
        const val TAG = "CcdNode"
        /*
        #define NLINK_HDR_OFF_FROM 0
        #define NLINK_HDR_OFF_TO   1
        #define NLINK_HDR_OFF_CMD  2
        #define NLINK_HDR_OFF_TYPE 3
        #define NLINK_HDR_OFF_LEN  4
        #define NLINK_HDR_OFF_DATA 5
         */
        const val CCD_NODE_INFO_FROM = 0
        const val CCD_NODE_INFO_TO   = 1
        const val CCD_NODE_INFO_CMD  = 2
        const val CCD_NODE_INFO_TYPE = 3
        const val CCD_NODE_INFO_LEN  = 4
        const val CCD_NODE_INFO_DATA = 5

        /*
         * Called from AvUart thread
         * Does sanity check and post message with specified note v
         */
        fun onRx(rxBuff: ByteArray) {

            if (rxBuff.size < CCD_NODE_INFO_DATA) {
                Log.e(TAG, "NODE_INFO: Corrupted")
                return
            }

            val nodeInfoLen = rxBuff[CCD_NODE_INFO_LEN].toInt()
            val nodeInfoAddr = rxBuff[CCD_NODE_INFO_FROM].toInt()

            if (rxBuff.size != nodeInfoLen + CCD_NODE_INFO_DATA) {
                Log.e(TAG, "NODE_INFO: Bad message length ${rxBuff.size}!=${nodeInfoLen + CCD_NODE_INFO_DATA}")
                return
            }
            if ( nodeInfoAddr == 0xFF) {
                Log.e(TAG, "NODE_INFO: Bad address")
            }

            val nodeType = CcdNodeType.values().find { type -> type.v == rxBuff[CCD_NODE_INFO_TYPE].toInt() }
            if (nodeType == null) {
                Log.d(TAG, "NODE_INFO: Bad v $nodeType")
                return
            }
            EventBus.getDefault().post(
                    CcdNodeInfoResp(
                            addr = rxBuff[CCD_NODE_INFO_FROM].toInt(),
                            type = nodeType,
                            data = rxBuff.copyOfRange(CCD_NODE_INFO_DATA, CCD_NODE_INFO_DATA + nodeInfoLen)))

        }
    }
}
enum class CcdNodeType(val v: Int) {
    NONE(0x00),
    HVAC(0x10),
    LEDLIGHT(0x20),
    SWITCH(0x30),
    CTRLCON(0x40),
    PHTS(0x50)
}

class CtrlConsoleDbgActivity : AppCompatActivity() {

    private var mAvBleService: AvBleService? = null

    var discoveryRequested = false
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
                CcdNodeType.PHTS -> {
                    node = CcdNodePhts(msg.addr, msg.data)
                }
                else -> {
                    Log.d(TAG, "Unknow node v - ${msg.type}, from ${msg.addr}")
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

//    @Subscribe(threadMode = ThreadMode.BACKGROUND)
    /** Called from UART thread */
    fun onUartIdle() {

        if (discoveryRequested) {
            discoveryRequested = false
            uart.txMsgQueue.add( AvUartTxMsg(
                    data = byteArrayOf(), cmd = AvUart.RX_BUFF_HDR_CMD_CTRLCON_DISC) )
            return
        }
        val it = nodes.iterator()
        while (it.hasNext()) {
            val node = it.next().value
            when (node.type) {
                CcdNodeType.PHTS,
                CcdNodeType.HVAC,
                CcdNodeType.LEDLIGHT -> {
                    if (node.userInfoChangesLastTx != node.userInfoChanges) {
                        node.userInfoChangesLastTx = node.userInfoChanges
                        uart.txMsgQueue.add( AvUartTxMsg(data = node.pack(), cmd = AvUart.RX_BUFF_HDR_CMD_CTRLCON_SET) )
                        Log.d(TAG, "Node (${node.type}@${node.addr}) updated by userInfo")
                    }
                }
                else -> { // Do nothing }
                }
            }
        }

    }

    companion object {
        const val TAG = "CcdActivity"
    }

}
