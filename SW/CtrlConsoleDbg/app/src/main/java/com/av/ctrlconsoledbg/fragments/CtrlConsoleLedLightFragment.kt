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
import com.av.ctrlconsoledbg.CcdNodeLedLight.Companion.intensityFromIdx
import com.av.uart.AvUartTxMsg
import kotlinx.android.synthetic.main.ctrl_console_ledlight.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

class CtrlConsoleLedLightFragment: Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null

    private var userInfo: NodeLedLightInfo? = null  // Q: ? Is it link to node.userInfo
    !!!!!!!!!!! ^^^ replace with complete node !!!! Mark node on userInfo changes in order to send update to CtrlCon Node

    override fun onAttach(context: Context) {
        super.onAttach(context)
        mCtrlConsole = context as CtrlConsoleDbgActivity
    }

    init {
        Log.d(TAG, "CtrlConsoleLedLightFragment - initialized")
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
        val view = inflater?.inflate(R.layout.ctrl_console_ledlight, container, false)
        return view
    }

    override fun onViewCreated(view: View?, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        ll_user_apply.setOnClickListener { _ ->
            //applyUserSettings()
        }

        enableAll(false)
        val intensityBarListener = object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar :SeekBar , progress : Int, fromUser : Boolean){
                val _userInfo = userInfo?: return
                val s = "${intensityFromIdx(progress)}%"
                when (seekBar.id) {
                    ll_ch0_intensity_bar.id -> {
                        ll_ch0_intensity.setText(s)
                        _userInfo.channels[0].intensity = progress
                    }
                    ll_ch1_intensity_bar.id -> {
                        ll_ch1_intensity.setText(s)
                        _userInfo.channels[1].intensity = progress
                    }
                    ll_ch2_intensity_bar.id -> {
                        ll_ch2_intensity.setText(s)
                        _userInfo.channels[2].intensity = progress
                    }
                    ll_chAll_intensity_bar.id -> {
                        ll_ch0_intensity.setText(s)
                        ll_ch1_intensity.setText(s)
                        ll_ch2_intensity.setText(s)
                        ll_ch0_intensity_bar.progress = progress
                        ll_ch1_intensity_bar.progress = progress
                        ll_ch2_intensity_bar.progress = progress
                        _userInfo.channels[0].intensity = progress
                        _userInfo.channels[1].intensity = progress
                        _userInfo.channels[2].intensity = progress
                        ll_chAll_intensity.setText(s)
                    }
                    else -> return
                }
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
            }
        }
        val disabledListener = object : View.OnClickListener {
            override fun onClick(v: View?) {
                val _userInfo = userInfo?: return
                if (v == null) return
                val ch: NodeLedLightChannelInfo
                val item: View
                when(v.id) {      // how to get callers ID???
                    ll_ch0_disabled.id -> {
                        ch = _userInfo.channels[0]
                        item = ll_ch0_disabled
                    }
                    ll_ch1_disabled.id -> {
                        ch = _userInfo.channels[1]
                        item = ll_ch1_disabled
                    }
                    ll_ch2_disabled.id -> {
                        ch = _userInfo.channels[2]
                        item = ll_ch2_disabled
                    }
                    else -> return
                }
                ch.disabled = !ch.disabled
                if (ch.disabled) {
                    item.setImageResource(R.drawable.ic_check_box_outline_blank_black_24dp)
                } else {
                    item.setImageResource(R.drawable.ic_check_box_black_24dp)
                }
            }
        }

        ll_ch0_disabled.setOnClickListener(disabledListener)
        ll_ch1_disabled.setOnClickListener(disabledListener)
        ll_ch2_disabled.setOnClickListener(disabledListener)

        ll_ch0_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)
        ll_ch1_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)
        ll_ch2_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)

        ll_chAll_mode.setOnCheckedChangeListener { _, b -> userInfo?.mode = b }
        ll_chAll_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)

        val cc = mCtrlConsole!!

        val it = cc.nodes.iterator()
        while (it.hasNext()) {
            val v = it.next().value
            Log.d(TAG, "${v.addr}")
            when (v.type) {
                CcdNodeType.LEDLIGHT -> {
                    val llInfo = (v as CcdNodeLedLight).info
                    if (llInfo != null) {
                        updateLedLightInfoUI(llInfo)
                    }

                    userInfo = v.userInfo
                    refreshLedLightUserInfoUI()
                }
                CcdNodeType.SWITCH -> {
                    val sw_info = (v as CcdNodeSwitch).info
                    if (sw_info != null) {
                        updateSwitchInfo(v.addr, sw_info)
                    }
                }
                else -> {
                    // Do nothing
                }
            }
        }

    }

    override fun onDestroyView() {
        super.onDestroyView()
        EventBus.getDefault().unregister(this)
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeLedLightInfo(node: CcdNodeLedLight) {
        updateLedLightInfoUI(node.info)
    }
    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeSwitchInfo(node: CcdNodeSwitch) {
        updateSwitchInfo(node.addr, node.info)
    }

    fun updateLedLightInfoUI(infoActual: NodeLedLightInfo)
    {
        ll_chAll_mode_actual.text = if (infoActual.mode) "On" else "Off"
        ll_ch0_disabled_actual.text = if (infoActual.channels[0].disabled) "Dis" else "En"
        ll_ch1_disabled_actual.text = if (infoActual.channels[1].disabled) "Dis" else "En"
        ll_ch2_disabled_actual.text = if (infoActual.channels[2].disabled) "Dis" else "En"

        val s0 = "${intensityFromIdx(infoActual.channels[0].intensity)}%"
        val s1 = "${intensityFromIdx(infoActual.channels[1].intensity)}%"
        val s2 = "${intensityFromIdx(infoActual.channels[2].intensity)}%"
        ll_ch0_intensity_actual.text = s0
        ll_ch1_intensity_actual.text = s1
        ll_ch2_intensity_actual.text = s2

        enableAll(true)
    }

    fun refreshLedLightUserInfoUI()
    {
        val _userInfo = userInfo?: return

        val disTrue = R.drawable.ic_check_box_outline_blank_black_24dp
        val disFalse = R.drawable.ic_check_box_black_24dp
        ll_ch0_disabled.setImageResource( if (_userInfo.channels[0].disabled) disTrue else disFalse)
        ll_ch1_disabled.setImageResource( if (_userInfo.channels[1].disabled) disTrue else disFalse)
        ll_ch2_disabled.setImageResource( if (_userInfo.channels[2].disabled) disTrue else disFalse)

        ll_ch0_intensity_bar.progress = _userInfo.channels[0].intensity
        ll_ch1_intensity_bar.progress = _userInfo.channels[1].intensity
        ll_ch2_intensity_bar.progress = _userInfo.channels[2].intensity

        val s0 = "${intensityFromIdx(_userInfo.channels[0].intensity)}%"
        val s1 = "${intensityFromIdx(_userInfo.channels[1].intensity)}%"
        val s2 = "${intensityFromIdx(_userInfo.channels[2].intensity)}%"

        ll_ch0_intensity.setText(s0)
        ll_ch1_intensity.setText(s1)
        ll_ch2_intensity.setText(s2)

        ll_chAll_intensity_bar.progress = _userInfo.channels[0].intensity
        ll_chAll_intensity.setText(s0)
        ll_chAll_mode.isChecked = _userInfo.mode
    }

    fun enableAll(enabled: Boolean) {

        ll_ch0_disabled.isEnabled        = enabled
        ll_ch1_disabled.isEnabled        = enabled
        ll_ch2_disabled.isEnabled        = enabled

        ll_ch0_intensity_bar.isEnabled   = enabled
        ll_ch1_intensity_bar.isEnabled   = enabled
        ll_ch2_intensity_bar.isEnabled   = enabled

        ll_ch0_intensity.isEnabled       = enabled
        ll_ch1_intensity.isEnabled       = enabled
        ll_ch2_intensity.isEnabled       = enabled

        ll_chAll_intensity_bar.isEnabled = enabled
        ll_chAll_intensity.isEnabled     = enabled
        ll_chAll_mode.isEnabled          = enabled
    }

    fun updateSwitchInfo(addr: Int, info: NodeSwitchInfo)
    {
        val state = when(info.event) {
            CcdNodeSwitchEvent.NONE     -> "OFF"
            CcdNodeSwitchEvent.ON_OFF   -> "OFF"
            CcdNodeSwitchEvent.OFF_ON   -> "ON"
            CcdNodeSwitchEvent.ON_HOLD  -> "HELD"
            CcdNodeSwitchEvent.HOLD_OFF -> "OFF"
            else -> "ERR"
        }
        val dstAddr = String.format("0x%02X", info.dstAddr)

        when(addr) {
            0x80 -> {
                ll_sw0_state.text = state
                ll_sw0_dst.setText(dstAddr)
            }
            else -> {
                Log.e(TAG, "Err")
            }
        }
    }

    companion object {
        private const val TAG = "CcdUIxxx"

    }
}