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
import kotlinx.android.synthetic.main.ctrl_console_ledlight.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

class CtrlConsoleLedLightFragment: Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null

    private var userInfo: NodeLedLightInfo? = null

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
                val info = userInfo?: return
                val s = "${intensityFromIdx(progress)}%"
                when (seekBar.id) {
                    ll_ch0_intensity_bar.id -> {
                        ll_ch0_intensity.setText(s)
                        info.channels[0].intensity = progress
                    }
                    ll_ch1_intensity_bar.id -> {
                        ll_ch1_intensity.setText(s)
                        info.channels[1].intensity = progress
                    }
                    ll_ch2_intensity_bar.id -> {
                        ll_ch2_intensity.setText(s)
                        info.channels[2].intensity = progress
                    }
                    ll_chAll_intensity_bar.id -> {
                        ll_ch0_intensity.setText(s)
                        ll_ch1_intensity.setText(s)
                        ll_ch2_intensity.setText(s)
                        ll_ch0_intensity_bar.progress = progress
                        ll_ch1_intensity_bar.progress = progress
                        ll_ch2_intensity_bar.progress = progress
                        info.channels[0].intensity = progress
                        info.channels[1].intensity = progress
                        info.channels[2].intensity = progress
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
                val info = userInfo?: return
                if (v == null) return
                val ch: NodeLedLightChannelInfo
                val item: View
                when(v.id) {      // how to get callers ID???
                    ll_ch0_disabled.id -> {
                        ch = info.channels[0]
                        item = ll_ch0_disabled
                    }
                    ll_ch1_disabled.id -> {
                        ch = info.channels[1]
                        item = ll_ch1_disabled
                    }
                    ll_ch2_disabled.id -> {
                        ch = info.channels[2]
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
                        updateLedLightInfo(llInfo)
                    }

                    refreshLedLightUserInfo(v.userInfo)
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
        val ll_info = node.info ?: return
        updateLedLightInfo(ll_info)
    }
    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeSwitchInfo(node: CcdNodeSwitch) {
        val sw_info = node.info ?: return
        updateSwitchInfo(node.addr, sw_info)
    }

    fun intensityFromIdx(idx: Int) = if (idx < intensityTbl.size) intensityTbl[idx] else  -1

    fun updateLedLightInfo(infoActual: NodeLedLightInfo)
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

    fun refreshLedLightUserInfo(userInfoIn: NodeLedLightInfo)
    {
        val disTrue = R.drawable.ic_check_box_outline_blank_black_24dp
        val disFalse = R.drawable.ic_check_box_black_24dp
        ll_ch0_disabled.setImageResource( if (userInfoIn.channels[0].disabled) disTrue else disFalse)
        ll_ch1_disabled.setImageResource( if (userInfoIn.channels[1].disabled) disTrue else disFalse)
        ll_ch2_disabled.setImageResource( if (userInfoIn.channels[2].disabled) disTrue else disFalse)

        ll_ch0_intensity_bar.progress = userInfoIn.channels[0].intensity
        ll_ch1_intensity_bar.progress = userInfoIn.channels[1].intensity
        ll_ch2_intensity_bar.progress = userInfoIn.channels[2].intensity

        val s0 = "${intensityFromIdx(userInfoIn.channels[0].intensity)}%"
        val s1 = "${intensityFromIdx(userInfoIn.channels[1].intensity)}%"
        val s2 = "${intensityFromIdx(userInfoIn.channels[2].intensity)}%"

        ll_ch0_intensity.setText(s0)
        ll_ch1_intensity.setText(s1)
        ll_ch2_intensity.setText(s2)

        ll_chAll_intensity_bar.progress = userInfoIn.channels[0].intensity
        ll_chAll_intensity.setText(s0)
        ll_chAll_mode.isChecked = userInfoIn.mode

        userInfo = userInfoIn
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
        val state = when(info.state) {
            SW_EVENT_NONE     -> ""
            SW_EVENT_ON_OFF   -> "ON->OFF"
            SW_EVENT_OFF_ON   -> "OFF->ON"
            SW_EVENT_ON_HOLD  -> "ON->HOLD"
            SW_EVENT_HOLD_OFF -> "HOLD->OFF"
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
        private val intensityTbl = intArrayOf(0, 3, 6, 10, 14, 18, 23, 28, 33, 38, 44, 50, 56, 63, 70, 75, 85, 93, 100)
        private const val SW_EVENT_NONE     = 0
        private const val SW_EVENT_ON_OFF   = 1
        private const val SW_EVENT_OFF_ON   = 2
        private const val SW_EVENT_ON_HOLD  = 3
        private const val SW_EVENT_HOLD_OFF = 4

        private const val TAG = "CcdUIxxx"

    }
}