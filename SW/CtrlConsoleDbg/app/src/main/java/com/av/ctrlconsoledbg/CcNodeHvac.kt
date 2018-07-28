package com.av.ctrlconsoledbg

import android.util.Log
import kotlin.experimental.and

data class NodeHvacInfo(
    var heaterMode: CcdNodeHvacHeaterMode,
    var heaterCurr: CcdNodeHvacHeaterCtrl,
    var heaterTarget: CcdNodeHvacHeaterCtrl,

    var stateCurr: CcdNodeHvacState,
    var stateTarget: CcdNodeHvacState,
    var stateTimer: Int,
    var stateAlarm: Int,

    var temperature: Int = 0,
    var pressure: Int = 0,
    var humidity: Int = 0
)
class CcdNodeHvac(addr: Int, data: ByteArray) : CcdNode(addr, CcdNodeType.HVAC) {
    // Parameters received from the Node
    var info: NodeHvacInfo
    var userInfo: NodeHvacInfo

    // var userTemp:Int
    // var smokeTimer: Int

    init {
        info = NodeHvacInfo(    // Default value
                stateCurr = CcdNodeHvacState.S1, stateTarget = CcdNodeHvacState.S1, stateTimer = 0, stateAlarm = 0,
                heaterMode = CcdNodeHvacHeaterMode.MANUAL,
                heaterCurr = CcdNodeHvacHeaterCtrl.OFF_DEG20, heaterTarget = CcdNodeHvacHeaterCtrl.OFF_DEG20)

        // Update Info from binary data
        update(data)
        userInfo = info
    }

    override fun pack(): ByteArray {
        // Pack set only target state & heater control
        val state = (userInfo.stateTarget.v shl 4).toByte()
        val heater =
                ((userInfo.heaterMode.v shl 7) +
                 (userInfo.heaterTarget.v shl 0)).toByte()

        val data = byteArrayOf(state, heater, 0)

        val hdr = ByteArray(CCD_NODE_INFO_DATA)

        hdr[CCD_NODE_INFO_FROM] = 0x90.toByte() // From CtrlCon
        hdr[CCD_NODE_INFO_TO]   = addr.toByte()
        hdr[CCD_NODE_INFO_CMD]  = 0
        hdr[CCD_NODE_INFO_TYPE] = type.v.toByte()
        hdr[CCD_NODE_INFO_LEN] = data.size.toByte()

        return (hdr + data)
    }

    override fun update(data: ByteArray?) {
        if (data?.size != 3) {
            Log.e(TAG, "Bad HVAC data")
            return
        }
        info = NodeHvacInfo(
                stateCurr   = CcdNodeHvacState.valueOf(data[0].and(0x07.toByte()).toInt() ushr 0),
                stateTarget = CcdNodeHvacState.valueOf(data[0].and(0x70.toByte()).toInt() ushr 4),
                stateTimer  = data[2].toInt(),
                stateAlarm  = data[0].and(0x80.toByte()).toInt() ushr 7,
                heaterMode = CcdNodeHvacHeaterMode.valueOf(data[1].and(0x80.toByte()).toInt() ushr 7),
                heaterCurr   = CcdNodeHvacHeaterCtrl.valueOf(data[1].and(0x70.toByte()).toInt() ushr 4),
                heaterTarget = CcdNodeHvacHeaterCtrl.valueOf(data[1].and(0x07.toByte()).toInt() ushr 0))
    }
    companion object {
        fun ctrlToString(mode: CcdNodeHvacHeaterMode, ctrl: CcdNodeHvacHeaterCtrl) : String {
            if (mode == CcdNodeHvacHeaterMode.MANUAL) {
                return when(ctrl) {
                    CcdNodeHvacHeaterCtrl.OFF_DEG20 -> "OFF"
                    CcdNodeHvacHeaterCtrl.P20_DEG21 -> "20%"
                    CcdNodeHvacHeaterCtrl.P40_DEG22 -> "40%"
                    CcdNodeHvacHeaterCtrl.P60_DEG23 -> "60%"
                    CcdNodeHvacHeaterCtrl.P80_DEG24 -> "80%"
                    CcdNodeHvacHeaterCtrl.ON_DEG25  -> "ON"
                    CcdNodeHvacHeaterCtrl.INVALID -> "INVALID"
                }
            } else {
                val grad = when(ctrl) {
                    CcdNodeHvacHeaterCtrl.OFF_DEG20 -> "20"
                    CcdNodeHvacHeaterCtrl.P20_DEG21 -> "21"
                    CcdNodeHvacHeaterCtrl.P40_DEG22 -> "22"
                    CcdNodeHvacHeaterCtrl.P60_DEG23 -> "23"
                    CcdNodeHvacHeaterCtrl.P80_DEG24 -> "24"
                    CcdNodeHvacHeaterCtrl.ON_DEG25  -> "25"
                    CcdNodeHvacHeaterCtrl.INVALID  -> "INVALID"
                }
                return "$grad${0x00B0.toChar()}C"
            }
        }

        private const val TAG = "HvacNode"
    }
}

enum class CcdNodeHvacState(val v: Int) {
    S1(1),      //  S1 Ventilation closed  V- M- H- S-   (Valve closed, Motor OFF, Heater OFF, Sensing: Velocity = 0)
    S2(2),      //  S2 Ventilation open    V+ M- H- Sv   (Valve open, Motor OFF, Heater OFF, Sensing: none)
    S3(3),      //  S3 Ventilation forced  V+ M+ H- Sv   (Valve open, Motor ON, Heater OFF, Sensing: Velocity > Vthrs_f)
    S4(4),      //  S4 Ventilation heated  V+ M+ H+ Svt  (Valve open, Motor ON, Heater ON, Sensing: Velocity > Vthrs_h; T > Tthrs_t);
    INVALID(-1);

    companion object {
        fun valueOf(value: Int): CcdNodeHvacState = CcdNodeHvacState.values().find { it.v == value } ?: INVALID
    }
}

enum class CcdNodeHvacHeaterMode(val v: Int) {
    MANUAL(0),    // User sets the heater's control value explicitly
    AUTO(1),      // User sets target temperature on sensor after the heater
    INVALID(-1);
    companion object {
        fun valueOf(value: Int): CcdNodeHvacHeaterMode = CcdNodeHvacHeaterMode.values().find { it.v == value } ?: INVALID
    }

}

enum class CcdNodeHvacHeaterCtrl(val v: Int) {
    OFF_DEG20(0),
    P20_DEG21(1),
    P40_DEG22(2),
    P60_DEG23(3),
    P80_DEG24(4),
    ON_DEG25(5),
    INVALID(-1);

    companion object {
        fun valueOf(value: Int): CcdNodeHvacHeaterCtrl = CcdNodeHvacHeaterCtrl.values().find { it.v == value } ?: INVALID
    }
}


