package com.av.ctrlconsoledbg

import android.util.Log
import kotlin.experimental.and

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

    override fun pack(): ByteArray {
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

        if (data.size != 5) {
            Log.e(TAG, "Bad length on update")
            return
        }

        info = NodeLedLightInfo(
                mode = (data[0].toInt() != 0),
                channels = arrayOf(
                        NodeLedLightChannelInfo( disabled = (data[1].and(0x01).toInt() != 0) , intensity = data[2].toInt()),
                        NodeLedLightChannelInfo( disabled = (data[1].and(0x02).toInt() != 0) , intensity = data[3].toInt()),
                        NodeLedLightChannelInfo( disabled = (data[1].and(0x04).toInt() != 0) , intensity = data[4].toInt())))
    }

    companion object {
        fun intensityFromIdx(idx: Int) = if (idx < intensityTbl.size) intensityTbl[idx] else  -1

        private val intensityTbl = intArrayOf(0, 3, 6, 10, 14, 18, 23, 28, 33, 38, 44, 50, 56, 63, 70, 75, 85, 93, 100)
        private const val TAG = "LedLightNode"
    }
}

