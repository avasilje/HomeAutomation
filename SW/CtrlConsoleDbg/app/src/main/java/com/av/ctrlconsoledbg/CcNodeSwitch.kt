package com.av.ctrlconsoledbg

data class NodeSwitchInfo(
        var event: CcdNodeSwitchEvent,
        var dstAddr: Int
)

class CcdNodeSwitch(addr: Int, data: ByteArray) : CcdNode(addr, CcdNodeType.SWITCH) {
    // Parameters received from the Node
    var info: NodeSwitchInfo
    var eventShowTimer: Int = 0// TODO: Implement timer to show event or replace with animation

    init {
        info = NodeSwitchInfo(CcdNodeSwitchEvent.NONE, 0)
        update(data)
    }
    override fun pack(): ByteArray {
        // Switch node is read only so far
        return byteArrayOf()
    }

    override fun update(data: ByteArray?) {
        if (data?.size != 2) return
        info = NodeSwitchInfo(
                event = CcdNodeSwitchEvent.values()[data[0].toInt()],
                dstAddr = data[1].toInt())
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
