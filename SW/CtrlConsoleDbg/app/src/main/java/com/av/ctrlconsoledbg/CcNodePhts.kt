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
    var temperature: Long = 0,
    var pressure: Long = 0,
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

    override fun pack(data: ByteArray?, dest: Int?): ByteArray {

        val nodeData = when(userInfo.state) {
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
        return super.pack(nodeData, dest)
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

    fun calcTemperature(adcT : Long) : Long {
        // Check is coefficient are valid
        // ...

        // dT = D2 - TREF = D2 - C5 * 2^8       (24bit)
        // TEMP = 20°C + dT *TEMPSENS = 2000 + dT * C6 / 2^23
        val dT = adcT  - info.coeffs[PhtsCoef.C5_REF_TEMPERATURE.ordinal]
        val t = (dT * info.coeffs[PhtsCoef.C6_TEMPERATURE_TCOEF.ordinal]) shr 15
        return t + 2000
    }

    fun calcPressure(adcP : Long, adcT: Long) : Long {
        // Check is coefficient are valid
        // ...
        // dT = D2 - TREF = D2 - C5 * 2^8
        // OFF = OFFT1 + TCO* dT = C2 * 2^17 + (C4 * dT ) / 2^6
        // SENS = SENST1 + TCS * dT = C1 * 2^16 + (C3 * dT ) / 2^7
        // P = D1 * SENS - OFF = (D1 * SENS / 2^21 - OFF) / 2^15

        val dT: Long = adcT - info.coeffs[PhtsCoef.C5_REF_TEMPERATURE.ordinal]

        var off: Long = dT * info.coeffs[PhtsCoef.C4_PRESSURE_OFFSET_TCOEF.ordinal] * 4
        off += info.coeffs[PhtsCoef.C2_PRESSURE_OFFSET.ordinal].toLong() shl 17

        var sens: Long = dT * info.coeffs[PhtsCoef.C3_PRESSURE_SENSITIVITY_TCOEF.ordinal] * 2
        sens += info.coeffs[PhtsCoef.C1_PRESSURE_SENSITIVITY.ordinal].toLong() shl 16

        val p = ((adcP * (sens shr 13)) - off) shr 15
        return p
    }

    fun updateReading(data: ByteArray) {
        info.rdCnt =  data[0].toInt()
        val adcT = ((data[1].toInt() and 0xFF) or ((data[2].toInt() and 0xFF) shl 8)).toLong()
        val adcP = ((data[3].toInt() and 0xFF) or ((data[4].toInt() and 0xFF) shl 8)).toLong()
        val adcH = (data[0].toInt() and 0xFF) or ((data[1].toInt() and 0xFF) shl 8)

        info.pressure       = calcPressure(adcP, adcT)
        info.temperature    = calcTemperature(adcT)
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


