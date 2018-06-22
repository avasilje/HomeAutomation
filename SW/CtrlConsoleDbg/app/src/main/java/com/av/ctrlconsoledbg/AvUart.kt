package com.av.uart

import android.content.Context
import android.util.Log
import com.av.ctrlconsoledbg.CcdNodeInfoResp
import com.av.ctrlconsoledbg.CcdNodeType
import com.av.ctrlconsoledbg.CcdUartConnected
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

import com.ftdi.j2xx.D2xxManager
import com.ftdi.j2xx.FT_Device
import java.util.concurrent.ConcurrentLinkedQueue

class AvUartTxMsg {
    var buff = ByteArray(10)    // Todo: get array size from contructor's input param
    var rdIdx = 0
}

class AvUart(val context: Context) {


    private var pollThread: Thread? = null
    private var pollTask = AvUartPoll(this)

    var ftD2xx: D2xxManager? = null
    var ftDev: FT_Device? = null
    var ftParams: D2xxManager.DriverParameters? = null

    var rxBuff: ByteArray = byteArrayOf()

    val rxHdr: ByteArray = byteArrayOf(0, 0, 0, 0)

    var rxSynced:Boolean = false

    var txBuff: ByteArray? = null

    var txMsgQueue = ConcurrentLinkedQueue<AvUartTxMsg>()

    internal enum class DeviceStatus {
        DEV_NOT_CONNECT,
        DEV_NOT_CONFIG,
        DEV_CONFIG
    }


    fun initialize() {
        Log.d(TAG, "Initializing")
        try {
            ftD2xx = D2xxManager.getInstance(context)
            ftParams = D2xxManager.DriverParameters()
        }
        catch (ex : D2xxManager.D2xxException) {
            Log.e(TAG, "Can't initialize D2XX")
        }

        EventBus.getDefault().register(this)
    }

    fun deinitialize() {
        Log.d(TAG, "Deinitializing")
        EventBus.getDefault().unregister(this)
    }

    fun stopPollThread() {

        if (pollThread == null) {
            Log.e(TAG, "Stopping UART RX thread - already stopped")
            return
        }

        pollTask.quit = true
        pollThread?.interrupt()
        pollThread?.join()
        Log.d(TAG, "UART RX thread stopped (${pollTask.running})")
        pollThread = null
    }

    fun startPollThread() {

        if (pollThread != null) {
            Log.e(TAG, "Something wrong in UART RX thread")
            return
        }

        Log.d(TAG, "Starting UART RX thread")
        pollThread = Thread(pollTask)
        pollThread?.start()
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onEvent(dataOut: ByteArray) {

    }

    internal fun connect(): Boolean
    {
        val ftD2xxL = ftD2xx?: return false
        val ftParamsL = ftParams?: return false

        if (ftDev != null) {
            Log.d(TAG, "Already connected")
            // Purge RX/TX buffers?

        } else {
            if (ftD2xxL.createDeviceInfoList(context) > 0) {
                val dev = ftD2xxL.openByIndex(context, 0)
                Log.d(TAG, "Open device at index 0 $dev")

                dev.setBitMode(0, D2xxManager.FT_BITMODE_RESET)
                dev.setBaudRate(250000)

                dev.setDataCharacteristics(
                        D2xxManager.FT_DATA_BITS_8,
                        D2xxManager.FT_STOP_BITS_1,
                        D2xxManager.FT_PARITY_NONE)

                dev.setFlowControl(D2xxManager.FT_FLOW_NONE, 0, 0)

                rxBuff = ByteArray(ftParamsL.maxBufferSize)
                txBuff = ByteArray(ftParamsL.maxBufferSize)

                ftDev = dev
                EventBus.getDefault().post(CcdUartConnected())
            } else {
                // Log.d(TAG, "Device not found")
            }
        }

        return ftDev?.isOpen?:false
    }
    private fun rxHdrIsValid() =
            ((rxHdr[RX_BUFF_HDR_OFF_MARK0] == 0x41.toByte()) &&
             (rxHdr[RX_BUFF_HDR_OFF_MARK1] == 0x86.toByte()))

    private fun rxHdrReadSingle() : Boolean{
        val ba = ByteArray(1)
        if (ba.size == ftDev?.read(ba, ba.size)) {
            // new byte read
            rxHdr[0] = rxHdr[1]
            rxHdr[1] = rxHdr[2]
            rxHdr[2] = rxHdr[3]
            rxHdr[3] = ba[0]
        }
        return rxHdrIsValid()
    }
    private fun rxHdrRead() : Boolean {
        return if (rxHdr.size == ftDev?.read(rxHdr, rxHdr.size)) {
            rxHdrIsValid()
        } else {
            false
        }

    }
    private fun rxBuffDispatchNodeInfo() {


    }
    private fun rxBuffDispatch() {

        val buffLen = rxHdr[RX_BUFF_HDR_OFF_LEN].toInt()
        when(rxHdr[RX_BUFF_HDR_OFF_CMD]){
            RX_BUFF_HDR_CMD_NODE_INFO -> {
                val nodeInfoLen = rxBuff[CCD_NODE_INFO_LEN].toInt()
                val nodeInfoAddr = rxBuff[CCD_NODE_INFO_FROM].toInt()

                if (rxBuff.size < CCD_NODE_INFO_DATA) {
                    Log.e(TAG, "NODE_INFO: Corrupted")
                    return
                }
                if (buffLen != nodeInfoLen + CCD_NODE_INFO_DATA) {
                    Log.e(TAG, "NODE_INFO: Bad message length $buffLen != ${nodeInfoLen + CCD_NODE_INFO_DATA}")
                    return
                }
                if ( nodeInfoAddr == 0xFF) {
                    Log.e(TAG, "NODE_INFO: Bad address")
                }

                val nodeType = CcdNodeType.values().find { t -> t.type == rxBuff[CCD_NODE_INFO_TYPE].toInt() }
                if (nodeType == null) {
                    Log.d(TAG, "NODE_INFO: Bad type $nodeType")
                    return
                }
                EventBus.getDefault().post(
                        CcdNodeInfoResp(
                                addr = rxBuff[CCD_NODE_INFO_FROM].toInt(),
                                type = nodeType,
                                data = rxBuff.copyOfRange(CCD_NODE_INFO_DATA, CCD_NODE_INFO_DATA + nodeInfoLen)))
            }
            else -> {
                Log.e(TAG, "Unknown message")
            }
        }
    }

    fun checkRx() {

        val dev = ftDev?:return
        if (!dev.isOpen) return

        var bytesToRead = dev.queueStatus

        // Not synced. Try to sync
        if (!rxSynced) {
            while(dev.queueStatus > 0) {
                rxSynced = rxHdrReadSingle()
                if (rxSynced) break
            }
        }

        // Synced, No header - read header
        if (rxSynced && !rxHdrIsValid()) {
            if (dev.queueStatus >= rxHdr.size) {
                if (!rxHdrRead()) {
                    rxSynced = false
                    Log.d(TAG, "Can't read Hdr from the device")
                }
            }
        }

        // Synced, header is valid - read data
        if (rxSynced && rxHdrIsValid()) {
            // Read data
            val btr = rxHdr[RX_BUFF_HDR_OFF_LEN].toInt()
            if (dev.queueStatus >= btr) {
                if (dev.read(rxBuff, btr) != btr) {
                    rxSynced = false
                    Log.d(TAG, "Can't read Data from the device")
                } else {
                    rxBuffDispatch()
                    rxHdr[RX_BUFF_HDR_OFF_LEN] = 0    // Invalidate packet
                }
            }
        }
    }

    fun checkTx() {

        val dev = ftDev?:return
        if (!dev.isOpen) return

        val msg = txMsgQueue.peek()
        if (msg != null) {
            val bytesWritten = dev.write(msg.buff)
            if (bytesWritten == msg.buff.size) {
                txMsgQueue.remove(msg)
            } else {
                msg.buff = msg.buff.sliceArray(IntRange(bytesWritten, msg.buff.size))
            }
        }
    }

    companion object {
        private const val TAG = "CcdUart"
        private const val RX_BUFF_HDR_OFF_MARK0 = 0
        private const val RX_BUFF_HDR_OFF_MARK1 = 1
        private const val RX_BUFF_HDR_OFF_CMD   = 2
        private const val RX_BUFF_HDR_OFF_LEN   = 3

        private const val RX_BUFF_HDR_CMD_NODE_INFO  = 0x21.toByte()

/*
#define NLINK_HDR_OFF_FROM 0
#define NLINK_HDR_OFF_TO   1
#define NLINK_HDR_OFF_CMD  2
#define NLINK_HDR_OFF_TYPE 3
#define NLINK_HDR_OFF_LEN  4
#define NLINK_HDR_OFF_DATA 5
 */
        private const val CCD_NODE_INFO_FROM = 0
        private const val CCD_NODE_INFO_TO   = 1
        private const val CCD_NODE_INFO_CMD  = 2
        private const val CCD_NODE_INFO_TYPE = 3
        private const val CCD_NODE_INFO_LEN  = 4
        private const val CCD_NODE_INFO_DATA = 5
    }
}

class AvUartPoll(private val avUart: AvUart) : Runnable {

    var running = false
    var quit = false
    var opened = false


    override fun run() {

        running = true

        Log.d(TAG, "Entering polling loop")
        rcv_loop@ while (!quit) {

            if (!opened) {
                try {
                    if(avUart.connect()) {
                        opened = true
                    } else {
                        Thread.sleep(3000)
                        continue@rcv_loop
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "Something wrong while connection to a device")
                    break@rcv_loop
                }
            }

            try {

                avUart.checkRx()

                // Check is something to read

                Thread.sleep(50)

            } catch (ex: Exception) {
                when(ex) {
                    is D2xxManager.D2xxException -> {
                        // It is OK just start receiver again
                        Log.d(TAG, "D2xxManager Exception")
                    }
                    is InterruptedException -> {
                        Log.d(TAG, "Interrupt Exception")
                    }
                    else -> {
                        Log.d(TAG, "Unknown exception")
                        throw ex
                    }
                }
                break@rcv_loop
            }
        }
        Log.d(TAG, "Terminating")
        running = false
    }

    companion object {
        private const val TAG = "AvUartPoll"
    }
}

