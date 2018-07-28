package com.av.ctrlconsoledbg

import android.util.Log

data class NodeSwitchInfo(
        var event: CcdNodeSwitchEvent,
        var dstAddr: Int
)

class CcdNodeSwitch(addr: Int, data: ByteArray) : CcdNode(addr, CcdNodeType.SWITCH) {
    // Parameters received from the Node
    var info: Array<NodeSwitchInfo>
    var eventShowTimer: Int = 0// TODO: Implement timer to show event or replace with animation

    init {
        info = Array(3, { _ -> NodeSwitchInfo(CcdNodeSwitchEvent.NONE, 0xFF) })
        update(data)
    }
    override fun pack(): ByteArray {
        // Switch node is read only so far
        return byteArrayOf()
    }

    override fun update(data: ByteArray?) {
//        if (data?.size != 1) {
//            Log.e(TAG, "Bad packet format")
//            return
//        }
        if (data?.isNotEmpty() != true) return

        try {
            data.forEach { b ->
                val event = (b.toInt() and 0x0F)
                val idx =   (b.toInt() and 0xF0) ushr 4
                info[idx] = NodeSwitchInfo(
                        event = CcdNodeSwitchEvent.values()[event],
                        dstAddr = info[idx].dstAddr)

            }
        } catch (e: Exception) {
            Log.e(TAG, "Bad packet format")
        }



    }
    companion object {
        private const val TAG = "SwitchNode"
    }
}

enum class CcdNodeSwitchEvent(v: Int) {
    NONE(0),
    ON_OFF(1),
    OFF_ON(2),
    ON_HOLD(3),
    HOLD_OFF(4)
}

enum class CcdNodeSwitchState(v: Int) {
    NONE(0),
    ON(1),
    OFF(2),
    HELD(3),
}
