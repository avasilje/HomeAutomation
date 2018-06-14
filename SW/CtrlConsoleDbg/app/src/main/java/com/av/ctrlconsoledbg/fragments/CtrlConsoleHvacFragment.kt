package com.av.ctrlconsoledbg.fragments

import android.content.Context
import android.os.Bundle
import android.support.v4.app.Fragment
import android.text.Editable
import android.text.Spannable
import android.text.SpannableString
import android.text.SpannableStringBuilder
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.av.ctrlconsoledbg.*
import kotlinx.android.synthetic.main.ctrl_console_hvac.*
import kotlinx.android.synthetic.main.ctrl_console_selector.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

class CtrlConsoleHvacFragment : Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null

    override fun onAttach(context: Context) {
        super.onAttach(context)
        mCtrlConsole = context as CtrlConsoleDbgActivity
    }

    init {
        Log.d(TAG, "CtrlConsoleHvacFragment - initialized")
    }

    override fun onDetach() {
        super.onDetach()
    }
    override fun onStop() {
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    override fun onCreateView(inflater: LayoutInflater?, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        EventBus.getDefault().register(this)
        val view = inflater?.inflate(R.layout.ctrl_console_hvac, container, false)
        return view
    }

    override fun onViewCreated(view: View?, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        var nodeHvac: CcdNodeHvac? = null
        val cc = mCtrlConsole!!

        val it = cc.nodes.iterator()
        while (it.hasNext()) {
            val v = it.next().value
            if (v.type == CcdNodeType.HVAC) {
                nodeHvac = v as CcdNodeHvac
            }
        }

        if (nodeHvac != null) {
            updateHvacInfo(nodeHvac.info)
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        EventBus.getDefault().unregister(this)

    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeInfo(info: CcdNode) {


    }

    fun updateHvacUser(hvac: CcdNodeHvac)
    {
        hvac_motor_user.isChecked = (hvac.userMotor != 0)

        hvac_heater_seekbar.progress = hvac.userHeater.toString().toInt()
        hvac_heater_user.text = hvac.userHeater

        hvac_valve_user.isChecked = (hvac.userValve != 0)
    }

    fun updateHvacInfo(info: NodeHvacInfo?)
    {
        var s: String
        hvac_motor_actual.text = when (info?.motorActual) {
            0 -> "Off"
            1 -> "On"
            else -> "n/a"
        }

        hvac_motor_configured.text = when (info?.motor) {
            0 -> "Off"
            1 -> "On"
            else -> "n/a"
        }

        s = "${info?.heaterActual}%"
        hvac_heater_actual.text = s

        s = "${info?.heater}%"
        hvac_heater_configured.text = s

        hvac_valve_actual.text = when (info?.valveActual) {
            0 -> "Off"
            1 -> "On"
            else -> "n/a"
        }

        hvac_valve_configured.text = when (info?.valve) {
            0 -> "Off"
            1 -> "On"
            else -> "n/a"
        }

        s = "Temp: ${info?.temp?:"n/a"} degC"
        hvac_temp.text = s

        s = "Pressure: ${info?.pressure?:"n/a"} Bar"
        hvac_pressure.text = s

        s = "Pressure: ${info?.pressure?:"n/a"}%"
        hvac_humidity.text = s
    }

    companion object {
        private const val TAG = "CcdUISelector"

    }
}