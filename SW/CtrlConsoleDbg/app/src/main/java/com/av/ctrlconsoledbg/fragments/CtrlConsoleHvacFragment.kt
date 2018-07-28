package com.av.ctrlconsoledbg.fragments

import android.content.Context
import android.os.Bundle
import android.support.v4.app.Fragment
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.SeekBar
import com.av.ctrlconsoledbg.*
import kotlinx.android.synthetic.main.ctrl_console_hvac.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

class CtrlConsoleHvacFragment : Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null
    private var nodeHvac: CcdNodeHvac? = null

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
        hvac_state_timer.visibility = View.INVISIBLE

        val heaterBarListener = object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar :SeekBar , progress : Int, fromUser : Boolean){
                val _userInfo = nodeHvac?.userInfo?: return
                _userInfo.heaterTarget = CcdNodeHvacHeaterCtrl.valueOf(progress)
                hvac_heater_value.text = CcdNodeHvac.ctrlToString(_userInfo.heaterMode, _userInfo.heaterTarget)
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
            }
        }
        val elementListener = object : View.OnClickListener {
            override fun onClick(v: View?) {
                val _userInfo = nodeHvac?.userInfo?: return
                if (v == null) return
                when(v.id) {      // how to get callers ID???
                    hvac_valve_icon.id -> {
                        if (_userInfo.stateTarget == CcdNodeHvacState.S2) {
                            _userInfo.stateTarget = CcdNodeHvacState.S1
                        } else {
                            _userInfo.stateTarget = CcdNodeHvacState.S2
                        }
                    }
                    hvac_motor_icon.id -> {
                        if (_userInfo.stateTarget == CcdNodeHvacState.S3) {
                            _userInfo.stateTarget = CcdNodeHvacState.S2
                        } else {
                            _userInfo.stateTarget = CcdNodeHvacState.S3
                        }
                    }
                    hvac_heater_icon.id -> {
                        if (_userInfo.stateTarget == CcdNodeHvacState.S4) {
                            _userInfo.stateTarget = CcdNodeHvacState.S3
                        } else {
                            _userInfo.stateTarget = CcdNodeHvacState.S4
                        }
                    }
                    else -> return
                }
                refreshHvacUserInfoUI()
                val _node = nodeHvac
                if (_node != null) { _node.userInfoChanges++ }
                nodeHvac?.userInfoChanges?.inc()
            }
        }

        hvac_valve_icon.setOnClickListener(elementListener)
        hvac_motor_icon.setOnClickListener(elementListener)
        hvac_heater_icon.setOnClickListener(elementListener)

        hvac_heater_bar.setOnSeekBarChangeListener (heaterBarListener)

        hvac_heater_mode.setOnCheckedChangeListener { v, b ->
            val _userInfo = nodeHvac?.userInfo
            if (_userInfo != null) {
                if (b) {
                    _userInfo.heaterMode = CcdNodeHvacHeaterMode.AUTO
                } else {
                    _userInfo.heaterMode = CcdNodeHvacHeaterMode.MANUAL
                }
                hvac_heater_value.text = CcdNodeHvac.ctrlToString(_userInfo.heaterMode, _userInfo.heaterTarget)
            }
        }

        val it = mCtrlConsole!!.nodes.iterator()
        while (it.hasNext()) {
            val v = it.next().value
            if (v.type == CcdNodeType.HVAC) {
                nodeHvac = v as CcdNodeHvac
                updateHvacInfoUI()
                refreshHvacUserInfoUI()
            }
        }

    }

    override fun onDestroyView() {
        super.onDestroyView()
        EventBus.getDefault().unregister(this)

    }
    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeHvacInfo(node: CcdNodeHvac) {
        val isNew = (nodeHvac == null)
        nodeHvac = node
        updateHvacInfoUI()
        if (isNew)
            refreshHvacUserInfoUI()
    }

    fun updateHvacInfoUI()
    {
        val _infoActual = nodeHvac?.info?:return
        when(_infoActual.stateCurr){
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


        val stateStr: String

        if (_infoActual.stateCurr.v < _infoActual.stateTarget.v) {
            stateStr = "${_infoActual.stateCurr.name} --> ${_infoActual.stateTarget.name}"
        } else if (_infoActual.stateCurr.v > _infoActual.stateTarget.v) {
            stateStr = "${_infoActual.stateTarget.name} <-- ${_infoActual.stateCurr.name}"
        } else {
            stateStr = "${_infoActual.stateCurr.name}"
        }
        hvac_state_actual.text = stateStr

        if (_infoActual.stateTimer != 0) {
            hvac_state_timer.visibility = View.VISIBLE
            val s = "${(_infoActual.stateTimer * 256 /100)}sec"
            hvac_state_timer.text = s
        } else {
            hvac_state_timer.visibility = View.INVISIBLE
        }

        val s = "${_infoActual.heaterMode.name} - ${CcdNodeHvac.ctrlToString(_infoActual.heaterMode, _infoActual.heaterTarget)}"
        hvac_heater_actual.text = s
        enableAll(true)
    }

    fun refreshHvacUserInfoUI()
    {
        val _userInfo = nodeHvac?.userInfo?:return

        val elementActive = R.drawable.ic_element_en_64dp
        val elementInactive = R.drawable.ic_element_dis_64dp
        when(_userInfo.stateTarget){
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
        hvac_state_user.text = _userInfo.stateTarget.name

        hvac_heater_mode.isChecked = (_userInfo.heaterMode == CcdNodeHvacHeaterMode.AUTO)
        hvac_heater_bar.progress = _userInfo.heaterTarget.v
        hvac_heater_value.text = CcdNodeHvac.ctrlToString(_userInfo.heaterMode, _userInfo.heaterTarget)
    }

    fun enableAll(enabled: Boolean) {

        val visibility = if (enabled) View.VISIBLE else View.INVISIBLE

        hvac_valve_icon.isEnabled   = enabled
        hvac_motor_icon.isEnabled   = enabled
        hvac_heater_icon.isEnabled  = enabled

        hvac_heater_mode.isEnabled  = enabled
        hvac_heater_bar.isEnabled   = enabled


        hvac_valve_state.visibility = visibility
        hvac_motor_state.visibility = visibility
        hvac_heater_state.visibility = visibility

        hvac_valve_state_actual.visibility = visibility
        hvac_motor_state_actual.visibility = visibility
        hvac_heater_state_actual.visibility = visibility
        hvac_state_user.visibility = visibility
        hvac_state_actual.visibility = visibility

    }

    companion object {
        private const val TAG = "CcdUIHvac"

    }
}