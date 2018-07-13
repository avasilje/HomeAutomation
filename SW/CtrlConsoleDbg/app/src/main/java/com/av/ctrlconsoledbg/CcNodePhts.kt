package com.av.ctrlconsoledbg

import android.util.Log
import kotlin.experimental.and

data class NodePhtsInfo(
    var coeffs: MutableList<Int> = MutableList(size = 6, init = { _ -> 0}),
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
    var coeffNeedAll = false
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

        return when(userInfo.state) {
            CcdNodePhtsState.RESET_PT,
            CcdNodePhtsState.RESET_RH,
            CcdNodePhtsState.READING -> {
                byteArrayOf(userInfo.state.ordinal.toByte())
            }
            CcdNodePhtsState.PROM_RD -> {
                byteArrayOf(userInfo.state.ordinal.toByte(), userInfo.coeffIdx.toByte())
            }
            else -> byteArrayOf()
        }
    }

    fun updateCoeff(data: ByteArray) {

        val idx =  data[0].toInt()
        val c = data[0].toInt().or(data[1].toInt() shl 8)
        if (info.coeffs.indices.contains(idx)) {
            info.coeffs[idx] = c
        } else {
            Log.e(TAG, "Bad PT coefficient index")
        }
    }

    fun updateReading(data: ByteArray) {
        val rdCnt =  data[0].toInt()
        val p = data[0].toInt().or(data[1].toInt() shl 8)
        val t = data[0].toInt().or(data[1].toInt() shl 8)
        val h = data[0].toInt().or(data[1].toInt() shl 8)

// TODO: calculate values if coefficients are available
        info.pressure       = p
        info.temperature    = t
        info.humidity       = h

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
            CcdNodePhtsState.READING -> {
                if (data.size != 8) {
                    Log.e(TAG, "Bad READING len ${data.size}")
                    return
                }
                updateReading(data.sliceArray(IntRange(1, 3)))
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

enum class CcdNodePhtsState(v: Int) {
    NONE(0x00),
    INIT(0x01),
    RESET_PT(0x1E),
    RESET_RH(0xFE),
    PROM_RD(0xA0),
    READING(0x1B),
}


