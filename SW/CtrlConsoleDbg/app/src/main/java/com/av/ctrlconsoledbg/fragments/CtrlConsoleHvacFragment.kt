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
import android.widget.SeekBar
import com.av.ctrlconsoledbg.*
import com.av.ctrlconsoledbg.CcdNodeLedLight.Companion.intensityFromIdx
import com.av.ctrlconsoledbg.R.id.ll_ch0_intensity
import kotlinx.android.synthetic.main.ctrl_console_hvac.*
import kotlinx.android.synthetic.main.ctrl_console_selector.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

class CtrlConsoleHvacFragment : Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null
    private var userInfo: NodeHvacInfo? = null

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

        enableAll(false)
        val heaterBarListener = object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar :SeekBar , progress : Int, fromUser : Boolean){
                val info = userInfo?: return
                info.heaterTarget = CcdNodeHvacHeaterCtrl.values()[progress]
                hvac_heater_value.text = CcdNodeHvac.ctrlToString(info.heaterMode, info.heaterTarget)
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
            }
        }
        val elementListener = object : View.OnClickListener {
            override fun onClick(v: View?) {
                val info = userInfo?: return
                if (v == null) return
                when(v.id) {      // how to get callers ID???
                    hvac_valve_icon.id -> {
                        if (info.stateTarget == CcdNodeHvacState.S2) {
                            info.stateTarget = CcdNodeHvacState.S1
                        } else {
                            info.stateTarget = CcdNodeHvacState.S2
                        }
                    }
                    hvac_motor_icon.id -> {
                        if (info.stateTarget == CcdNodeHvacState.S3) {
                            info.stateTarget = CcdNodeHvacState.S2
                        } else {
                            info.stateTarget = CcdNodeHvacState.S3
                        }
                    }
                    hvac_heater_icon.id -> {
                        if (info.stateTarget == CcdNodeHvacState.S4) {
                            info.stateTarget = CcdNodeHvacState.S3
                        } else {
                            info.stateTarget = CcdNodeHvacState.S4
                        }
                    }
                    else -> return
                }
                refreshHvacUserInfo(info)
            }
        }

        hvac_valve_icon.setOnClickListener(elementListener)
        hvac_motor_icon.setOnClickListener(elementListener)
        hvac_heater_icon.setOnClickListener(elementListener)

        hvac_heater_bar.setOnSeekBarChangeListener (heaterBarListener)

        hvac_heater_mode.setOnCheckedChangeListener { v, b ->
            val info = userInfo
            if (info != null) {
                if (b) {
                    info.heaterMode = CcdNodeHvacHeaterMode.AUTO
                } else {
                    info.heaterMode = CcdNodeHvacHeaterMode.MANUAL
                }
                hvac_heater_value.text = CcdNodeHvac.ctrlToString(info.heaterMode, info.heaterTarget)
            }
        }

        val it = mCtrlConsole!!.nodes.iterator()
        while (it.hasNext()) {
            val v = it.next().value
            if (v.type == CcdNodeType.HVAC) {
                val nodeHvac = v as CcdNodeHvac
                if (nodeHvac != null) {
                    updateHvacInfo(nodeHvac.info)
                }
                refreshHvacUserInfo(v.userInfo)
            }
        }

    }

    override fun onDestroyView() {
        super.onDestroyView()
        EventBus.getDefault().unregister(this)

    }
    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeHvacInfo(node: CcdNodeHvac) {
        updateHvacInfo(node.info)
    }

    fun updateHvacInfo(infoActual: NodeHvacInfo)
    {
        when(infoActual.stateTarget){
            CcdNodeHvacState.S1 -> {
                hvac_valve_state_actual.text = "Closed"
                hvac_motor_state_actual.text = "Stopped"
                hvac_heater_state_actual.text = "OFF"
            }
            CcdNodeHvacState.S2 -> {
                hvac_valve_state_actual.text = "Opened"
                hvac_motor_state_actual.text = "Stopped"
                hvac_heater_state_actual.text = "OFF"
            }
            CcdNodeHvacState.S3 -> {

                hvac_valve_state_actual.text = "Opened"
                hvac_motor_state_actual.text = "Running"
                hvac_heater_state_actual.text = "OFF"
            }
            CcdNodeHvacState.S4 -> {
                hvac_valve_state_actual.text = "Opened"
                hvac_motor_state_actual.text = "Running"
                hvac_heater_state_actual.text = "Heating"
            }
        }

        val stateStr = "${infoActual.stateCurr.name} --> ${infoActual.stateTarget.name}"
        hvac_state_actual.text = stateStr

        val s = "${infoActual.heaterMode.name} - ${CcdNodeHvac.ctrlToString(infoActual.heaterMode, infoActual.heaterTarget)}"
        hvac_heater_actual.text = s
        enableAll(true)
    }

    fun refreshHvacUserInfo(userInfoIn: NodeHvacInfo)
    {
        val elementActive = R.drawable.ic_element_en_64dp
        val elementInactive = R.drawable.ic_element_dis_64dp
        when(userInfoIn.stateTarget){
            CcdNodeHvacState.S1 -> {
                hvac_valve_icon.setImageResource(elementInactive)
                hvac_valve_state.text = "Closed"
                hvac_motor_icon.setImageResource(elementInactive)
                hvac_motor_state.text = "Stopped"
                hvac_heater_icon.setImageResource(elementInactive)
                hvac_heater_state.text = "OFF"
            }
            CcdNodeHvacState.S2 -> {
                hvac_valve_icon.setImageResource(elementActive)
                hvac_valve_state.text = "Opened"
                hvac_motor_icon.setImageResource(elementInactive)
                hvac_motor_state.text = "Stopped"
                hvac_heater_icon.setImageResource(elementInactive)
                hvac_heater_state.text = "OFF"
            }
            CcdNodeHvacState.S3 -> {
                hvac_valve_icon.setImageResource(elementActive)
                hvac_valve_state.text = "Opened"
                hvac_motor_icon.setImageResource(elementActive)
                hvac_motor_state.text = "Running"
                hvac_heater_icon.setImageResource(elementInactive)
                hvac_heater_state.text = "OFF"
            }
            CcdNodeHvacState.S4 -> {
                hvac_valve_icon.setImageResource(elementActive)
                hvac_valve_state.text = "Opened"
                hvac_motor_icon.setImageResource(elementActive)
                hvac_motor_state.text = "Running"
                hvac_heater_icon.setImageResource(elementActive)
                hvac_heater_state.text = "Heating"
            }
        }
        hvac_state_user.text = userInfoIn.stateTarget.name

        hvac_heater_mode.isChecked = (userInfoIn.heaterMode == CcdNodeHvacHeaterMode.AUTO)
        hvac_heater_bar.progress = userInfoIn.heaterTarget.ordinal
        hvac_heater_value.text = CcdNodeHvac.ctrlToString(userInfoIn.heaterMode, userInfoIn.heaterTarget)

        userInfo = userInfoIn
    }

    fun enableAll(enabled: Boolean) {

        hvac_valve_icon.isEnabled   = enabled
        hvac_motor_icon.isEnabled   = enabled
        hvac_heater_icon.isEnabled  = enabled

        hvac_heater_mode.isEnabled  = enabled
        hvac_heater_bar.isEnabled   = enabled
        hvac_heater_state.isEnabled = enabled
    }

    companion object {
        private const val TAG = "CcdUIHvac"

    }
}