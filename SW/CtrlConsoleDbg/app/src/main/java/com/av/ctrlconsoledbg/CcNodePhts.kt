package com.av.ctrlconsoledbg

import android.util.Log
enum class PhtsCoef {
    CRC,
    C1_PRESSURE_SENSITIVITY,
    C2_PRESSURE_OFFSET,
    C3_PRESSURE_SENSITIVITY_TCOEF,
    C4_PRESSURE_OFFSET_TCOEF,
    C5_REF_TEMPERATURE,
    C6_TEMPERATURE_TCOEF
}

data class NodePhtsInfo(
    var coeffs: MutableList<Int> = MutableList(size = PhtsCoef.values().size, init = { _ -> 0}),
    var rdCnt: Int = 0,
    var state: CcdNodePhtsState = CcdNodePhtsState.NONE,
    var coeffIdx: Int = -1,
    var temperature: Int = 0,
    var pressure: Int = 0,
    var humidity: Int = 0
)

class CcdNodePhts(addr: Int, data: ByteArray) : CcdNode(addr, CcdNodeType.PHTS) {
    // Parameters received from the Node
    var info: NodePhtsInfo
    var userInfo: NodePhtsInfo
    // var pollTimer: Int
    // var state
    // var coeffReadFsm

    init {
        info = NodePhtsInfo()

        // Update Info from binary data if available
        update(data)
        userInfo = info
    }

    override fun pack(): ByteArray {

        val data = when(userInfo.state) {
            CcdNodePhtsState.RESET_PT,
            CcdNodePhtsState.RESET_RH,
            CcdNodePhtsState.IDLE -> {
                byteArrayOf(userInfo.state.v.toByte())
            }
            CcdNodePhtsState.PROM_RD -> {
                byteArrayOf(userInfo.state.v.toByte(), (userInfo.coeffIdx shl 1).toByte())
            }
            else -> byteArrayOf()
        }

        val hdr = ByteArray(CCD_NODE_INFO_DATA)

        hdr[CCD_NODE_INFO_FROM] = 0x90.toByte() // From CtrlCon
        hdr[CCD_NODE_INFO_TO]   = addr.toByte()
        hdr[CCD_NODE_INFO_CMD]  = 0
        hdr[CCD_NODE_INFO_TYPE] = type.v.toByte()
        hdr[CCD_NODE_INFO_LEN] = data.size.toByte()

        return (hdr + data)

    }

    fun updateCoeff(data: ByteArray) {

        val idx =  data[0].toInt() ushr 1

        val c = ((data[1].toInt() and 0xFF) shl 0) or
                ((data[2].toInt() and 0xFF) shl 8)

        if (info.coeffs.indices.contains(idx)) {
            info.coeffs[idx] = c
        } else {
            Log.e(TAG, "Bad PT coefficient index")
        }
    }

    fun calTemperature(adcT : Int) : Float {
        // Check is coefficient are valid
        // dT = D2 - TREF = D2 - C5 * 2^8
        // TEMP = 20°C + dT *TEMPSENS = 2000 + dT *C6 / 2^23
        val dT = adcT - info.coeffs[PhtsCoef.C5_REF_TEMPERATURE.ordinal]
        val t = 2000 + dT / (1 shl 23)
        return t.toFloat()/100
    }
    fun updateReading(data: ByteArray) {
        info.rdCnt =  data[0].toInt()
//        val p = (data[1].toInt() and 0xFF) or ((data[2].toInt() and 0xFF) shl 8)
//        val adcT = (data[3].toInt() and 0xFF) or ((data[4].toInt() and 0xFF) shl 8)
////        val h = (data[0].toInt() and 0xFF) or ((data[1].toInt() and 0xFF) shl 8)
//!!!!!!!!!
//        t = calTemperature(adcT)
        info.pressure       = 0
        info.temperature    = 1
        info.humidity       = 0

        Log.e(TAG, "Reading (${info.rdCnt}): ${info.pressure}mBar, ${info.temperature}C, ${info.humidity}%")
    }

    override fun update(data: ByteArray?) {

        if (data == null) return
        val state: CcdNodePhtsState
        try {
            state = CcdNodePhtsState.values()[data[0].toInt()]
        } catch (e: Exception){
            Log.e(TAG, "Bad State ${data[0].toInt()}")
            return
        }

        when(state) {
            CcdNodePhtsState.INIT,
            CcdNodePhtsState.RESET_PT,
            CcdNodePhtsState.RESET_RH -> {
                info.state = state
            }
            CcdNodePhtsState.PROM_RD -> {
                if (data.size != 4) {
                    Log.e(TAG, "Bad PROM_RD len ${data.size}")
                    return
                }
                updateCoeff(data.sliceArray(IntRange(1, 3)))
            }
            CcdNodePhtsState.IDLE -> {
                if (data.size != 6) {
                    Log.e(TAG, "Bad IDLE len ${data.size}")
                    return
                }
                updateReading(data.sliceArray(IntRange(1, 5)))
            }
            else -> {
                Log.e(TAG, "Unhandled state $state")
            }
        }
    }
    companion object {
        private const val TAG = "PhtsNode"
    }
}

enum class CcdNodePhtsState(val v: Int) {
    NONE(0),
    INIT(1),
    RESET_PT(2),
    RESET_RH(3),
    PROM_RD(4),
    IDLE(5),    // aka MEASURE
}


