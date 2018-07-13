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
import kotlinx.android.synthetic.main.ctrl_console_phts.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

class CtrlConsolePhtsFragment : Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null
    private var nodePhts: CcdNodePhts? = null

    override fun onAttach(context: Context) {
        super.onAttach(context)
        mCtrlConsole = context as CtrlConsoleDbgActivity
    }

    init {
        Log.d(TAG, "${this.javaClass.canonicalName} - initialized") // TODO: check this
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
        val view = inflater?.inflate(R.layout.ctrl_console_phts, container, false)
        return view
    }

    override fun onViewCreated(view: View?, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        enableAll(false)

        val buttonListener = object : View.OnClickListener {
            override fun onClick(v: View?) {
                val _nodePhts = nodePhts?:return
                val _userInfo = _nodePhts.userInfo
                if (v == null) return
                when(v.id) {      // how to get callers ID???
                    phts_coeff_3.id -> {
                        // Read single
                        _userInfo.state = CcdNodePhtsState.PROM_RD
                        _userInfo.coeffIdx = 2
                        _nodePhts.coeffNeedAll = true
                        _nodePhts.userInfoChanges ++
                    }
                    phts_coef_read.id -> {
                        // Read all
                        _userInfo.state = CcdNodePhtsState.PROM_RD
                        _userInfo.coeffIdx = 0
                        _nodePhts.coeffNeedAll = true
                        _nodePhts.userInfoChanges ++
                    }
                    phts_reset_pt.id -> {
                        _userInfo.state = CcdNodePhtsState.RESET_PT
                        _nodePhts.userInfoChanges ++
                    }
                    phts_reset_rh.id -> {
                        _userInfo.state = CcdNodePhtsState.RESET_RH
                        _nodePhts.userInfoChanges ++
                    }
                    phts_read.id -> {
                        _userInfo.state = CcdNodePhtsState.READING
                        _nodePhts.userInfoChanges ++
                    }

                    else -> return
                }
                refreshPhtsUserInfoUI()
            }
        }

        phts_coeff_1.setOnClickListener(buttonListener)
        phts_coeff_2.setOnClickListener(buttonListener)
        phts_coeff_3.setOnClickListener(buttonListener)
        phts_coeff_4.setOnClickListener(buttonListener)
        phts_coeff_5.setOnClickListener(buttonListener)
        phts_coeff_6.setOnClickListener(buttonListener)
        phts_reset_pt.setOnClickListener(buttonListener)
        phts_reset_rh.setOnClickListener(buttonListener)
        phts_coef_read.setOnClickListener(buttonListener)
        phts_read.setOnClickListener(buttonListener)

        val it = mCtrlConsole!!.nodes.iterator()
        while (it.hasNext()) {
            val v = it.next().value
            if (v.type == CcdNodeType.PHTS) {
                nodePhts = v as CcdNodePhts
                updatePhtsInfoUI()
                refreshPhtsUserInfoUI()
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        EventBus.getDefault().unregister(this)

    }
    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodePhtsInfo(node: CcdNodePhts) {
        nodePhts = node
        updatePhtsInfoUI()
    }

    fun updatePhtsInfoUI()
    {
        val _infoActual = nodePhts?.info?:return

        var s: String
        s = "C1 PRESS SENS: ${_infoActual.coeffs[0]}"  ; phts_coeff_1.text = s
        s = "C2 PRESS OFF: ${_infoActual.coeffs[1]}"   ; phts_coeff_2.text = s
        s = "C3 TCS: ${_infoActual.coeffs[2]}"         ; phts_coeff_3.text = s
        s = "C4 TCO: ${_infoActual.coeffs[3]}"         ; phts_coeff_4.text = s
        s = "C5 REF TEMP: ${_infoActual.coeffs[4]}"    ; phts_coeff_5.text = s
        s = "C6 TEMP SENS : ${_infoActual.coeffs[5]}"  ; phts_coeff_6.text = s

        s = "State: ${_infoActual.state.name}" ; phts_state_actual.text     = s
        s = "Rd Cnt: ${_infoActual.rdCnt}"     ; phts_read_cnt_actual.text  = s
        s = "Coef Idx: ${_infoActual.coeffIdx}"; phts_coeff_idx_actual.text = s

        s = "Temp: ${_infoActual.temperature}GradC" ; phts_temp_value.text = s
        s = "Pressure: ${_infoActual.temperature}Pa"; phts_pressure_value.text = s
        s = "Humidity: ${_infoActual.humidity}%"    ; phts_humidity_value.text = s

        enableAll(true)
    }

    fun refreshPhtsUserInfoUI()
    {
        val _userInfo = nodePhts?.userInfo?:return
        var s: String

        s = "State: ${_userInfo.state.name}" ; phts_state_user.text     = s
        s = "Rd Cnt: ${_userInfo.rdCnt}"     ; phts_read_cnt_user.text  = s
        s = "Coef Idx: ${_userInfo.coeffIdx}"; phts_coeff_idx_user.text = s

    }

    fun enableAll(enabled: Boolean) {
        phts_coeff_3.isEnabled    = enabled

        phts_reset_pt.isEnabled   = enabled
        phts_reset_rh.isEnabled   = enabled
        phts_coef_read.isEnabled  = enabled
        phts_read.isEnabled       = enabled

    }

    companion object {
        private const val TAG = "CcdUIHvac"

    }
}