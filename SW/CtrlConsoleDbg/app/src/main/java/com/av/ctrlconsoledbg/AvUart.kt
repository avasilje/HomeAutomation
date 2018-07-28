package com.av.uart

import android.content.Context
import android.util.Log
import com.av.ctrlconsoledbg.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

import com.ftdi.j2xx.D2xxManager
import com.ftdi.j2xx.FT_Device
import java.util.concurrent.ConcurrentLinkedQueue

class AvUartTxMsg(data: ByteArray, cmd: Byte = AvUart.RX_BUFF_HDR_CMD_CTRLCON_SET) {

    var buff = ByteArray(AvUart.RX_BUFF_HDR_OFF_DATA) + data.copyOf()
    var rdIdx = 0
    init {
        buff[AvUart.RX_BUFF_HDR_OFF_MARK0] = 0x41.toByte()
        buff[AvUart.RX_BUFF_HDR_OFF_MARK1] = 0x86.toByte()
        buff[AvUart.RX_BUFF_HDR_OFF_CMD] = cmd
        buff[AvUart.RX_BUFF_HDR_OFF_LEN] = data.size.toByte()
    }
}

class AvUart(val context: Context) {


    private var pollThread: Thread? = null
    private var pollTask = AvUartPoll(this)

    var ftD2xx: D2xxManager? = null
    var ftDev: FT_Device? = null
    var ftParams: D2xxManager.DriverParameters? = null

    var rxBuff: ByteArray = byteArrayOf()

    val rxHdr: ByteArray = byteArrayOf(0, 0, 0, 0)

    var rxSynced: Boolean = false

    var txBuff: ByteArray? = null

    var txMsgQueue = ConcurrentLinkedQueue<AvUartTxMsg>()

    internal enum class DeviceStatus {
        DEV_NOT_CONNECT,
        DEV_NOT_CONFIG,
        DEV_CONFIG
    }


    fun packSync() = byteArrayOf(
            0x41.toByte(),
            0x86.toByte(),
            RX_BUFF_HDR_CMD_RESYNC,
            0)

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
             (rxHdr[RX_BUFF_HDR_OFF_MARK1] == 0x86.toByte()) &&
             (rxHdr[RX_BUFF_HDR_OFF_CMD] != RX_BUFF_HDR_CMD_INVALID))

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



    private fun rxBuffDispatch() {
        val buffLen = rxHdr[RX_BUFF_HDR_OFF_LEN].toInt()

        if (rxBuff.size != buffLen){ // TODO: Q:? where buffer size is defined ?
            Log.e(TAG, "Bad packet length ${rxBuff.size} != ${buffLen}")
            rxSynced = false
            ftDev?.purge(1) // Purge RX
            return
        }
        when(rxHdr[RX_BUFF_HDR_OFF_CMD]){
            RX_BUFF_HDR_CMD_CTRLCON_INFO -> {
                CcdNode.onRx(rxBuff)
            }
            RX_BUFF_HDR_CMD_RESYNC -> {
                txMsgQueue.add(AvUartTxMsg(data = byteArrayOf(), cmd = RX_BUFF_HDR_CMD_RESYNC))
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
                rxBuff = ByteArray(btr)
                if (btr > 0 && dev.read(rxBuff, btr) != btr) {
                    rxSynced = false
                    Log.d(TAG, "Can't read Data from the device")
                } else {
                    rxBuffDispatch()
                    rxHdr[RX_BUFF_HDR_OFF_CMD] = RX_BUFF_HDR_CMD_INVALID
                }
            }
        }
    }

    fun checkTx() {

        val dev = ftDev?:return
        if (!dev.isOpen) return

        var msg = txMsgQueue.peek()

        if (msg == null) {
            (context as CtrlConsoleDbgActivity).onUartIdle()
            msg = txMsgQueue.peek()
        }

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
        const val TAG = "CcdUart"
        const val RX_BUFF_HDR_OFF_MARK0 = 0
        const val RX_BUFF_HDR_OFF_MARK1 = 1
        const val RX_BUFF_HDR_OFF_CMD   = 2
        const val RX_BUFF_HDR_OFF_LEN   = 3
        const val RX_BUFF_HDR_OFF_DATA  = 4

        const val RX_BUFF_HDR_CMD_INVALID  = 0x11.toByte()      // Is used to invalidate RxHdr buffer
        const val RX_BUFF_HDR_CMD_CTRLCON_INFO  = 0x21.toByte()
        const val RX_BUFF_HDR_CMD_CTRLCON_SET  = 0x22.toByte()
        const val RX_BUFF_HDR_CMD_RESYNC  = 0x31.toByte()
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

                avUart.checkTx()

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

