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
import kotlinx.android.synthetic.main.ctrl_console_ledlight.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode

class CtrlConsoleLedLightFragment: Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null

    private var nodeLL: CcdNodeLedLight? = null  // Q: ? Is it link to node.nodeLL
    private var nodeSwitch: CcdNodeSwitch? = null
    private var autoUpdate: Boolean = false

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
            val _nodeLL = nodeLL
            if (_nodeLL != null) _nodeLL.userInfoChanges ++
        }

        if (autoUpdate) {
            ll_user_apply_auto.setImageResource(R.drawable.ic_check_box_black_24dp)
        } else {
            ll_user_apply_auto.setImageResource(R.drawable.ic_check_box_outline_blank_black_24dp)
        }

        ll_user_apply_auto.setOnClickListener { _ ->
            autoUpdate = !autoUpdate
            if (autoUpdate) {
                ll_user_apply_auto.setImageResource(R.drawable.ic_check_box_black_24dp)
            } else {
                ll_user_apply_auto.setImageResource(R.drawable.ic_check_box_outline_blank_black_24dp)
            }

        }

        enableAll(false)
        val intensityBarListener = object : SeekBar.OnSeekBarChangeListener{
            override fun onProgressChanged(seekBar :SeekBar , progress : Int, fromUser : Boolean){
                val _userInfo = nodeLL?.userInfo?: return
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
                    ll_ch3_intensity_bar.id -> {
                        ll_ch3_intensity.setText(s)
                        _userInfo.channels[3].intensity = progress
                    }
                    ll_chAll_intensity_bar.id -> {
                        ll_ch0_intensity.setText(s)
                        ll_ch1_intensity.setText(s)
                        ll_ch2_intensity.setText(s)
                        ll_ch3_intensity.setText(s)
                        ll_ch0_intensity_bar.progress = progress
                        ll_ch1_intensity_bar.progress = progress
                        ll_ch2_intensity_bar.progress = progress
                        ll_ch3_intensity_bar.progress = progress
                        _userInfo.channels[0].intensity = progress
                        _userInfo.channels[1].intensity = progress
                        _userInfo.channels[2].intensity = progress
                        _userInfo.channels[3].intensity = progress
                        ll_chAll_intensity.setText(s)
                    }
                    else -> return
                }
                if (autoUpdate)  nodeLL?.userInfoChanges = (nodeLL?.userInfoChanges?:0) + 1
            }

            override fun onStartTrackingTouch(seekBar: SeekBar?) {
            }

            override fun onStopTrackingTouch(seekBar: SeekBar?) {
            }
        }
        val disabledListener = object : View.OnClickListener {
            override fun onClick(v: View?) {
                val _userInfo = nodeLL?.userInfo?: return
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
                    ll_ch3_disabled.id -> {
                        ch = _userInfo.channels[3]
                        item = ll_ch3_disabled
                    }
                    else -> return
                }
                ch.disabled = !ch.disabled
                if (ch.disabled) {
                    item.setImageResource(R.drawable.ic_check_box_outline_blank_black_24dp)
                } else {
                    item.setImageResource(R.drawable.ic_check_box_black_24dp)
                }
                if (autoUpdate)  nodeLL?.userInfoChanges = (nodeLL?.userInfoChanges?:0) + 1
            }
        }

        ll_ch0_disabled.setOnClickListener(disabledListener)
        ll_ch1_disabled.setOnClickListener(disabledListener)
        ll_ch2_disabled.setOnClickListener(disabledListener)
        ll_ch3_disabled.setOnClickListener(disabledListener)

        ll_ch0_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)
        ll_ch1_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)
        ll_ch2_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)
        ll_ch3_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)

        ll_chAll_mode.setOnCheckedChangeListener { _, b ->
            nodeLL?.userInfo?.mode = if (b) CcdNodeLedLightMode.ON else CcdNodeLedLightMode.OFF
            if (autoUpdate)  nodeLL?.userInfoChanges = (nodeLL?.userInfoChanges?:0) + 1
        }

        ll_chAll_intensity_bar.setOnSeekBarChangeListener (intensityBarListener)

        val cc = mCtrlConsole!!

        val it = cc.nodes.iterator()
        while (it.hasNext()) {
            val _node = it.next().value
            Log.d(TAG, "${_node.addr}")
            when (_node.type) {
                CcdNodeType.LEDLIGHT -> {
                    nodeLL = _node as CcdNodeLedLight
                    updateLedLightInfoUI()
                    refreshLedLightUserInfoUI()
                }
                CcdNodeType.SWITCH -> {
                    nodeSwitch = _node as CcdNodeSwitch
                    updateSwitchInfoUI(_node.addr)
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
        nodeLL = node // ?? nodeLL == node
        updateLedLightInfoUI()
    }
    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeSwitchInfo(node: CcdNodeSwitch) {
        nodeSwitch = node
        updateSwitchInfoUI(node.addr)
    }

    fun updateLedLightInfoUI()
    {
        val _infoActual = nodeLL?.info?:return

        ll_chAll_mode_actual.text = if (_infoActual.mode == CcdNodeLedLightMode.ON) "On" else "Off"
        ll_ch0_disabled_actual.text = if (_infoActual.channels[0].disabled) "Dis" else "En"
        ll_ch1_disabled_actual.text = if (_infoActual.channels[1].disabled) "Dis" else "En"
        ll_ch2_disabled_actual.text = if (_infoActual.channels[2].disabled) "Dis" else "En"
        ll_ch3_disabled_actual.text = if (_infoActual.channels[3].disabled) "Dis" else "En"

        val s0 = "${intensityFromIdx(_infoActual.channels[0].intensity)}%"
        val s1 = "${intensityFromIdx(_infoActual.channels[1].intensity)}%"
        val s2 = "${intensityFromIdx(_infoActual.channels[2].intensity)}%"
        val s3 = "${intensityFromIdx(_infoActual.channels[3].intensity)}%"
        ll_ch0_intensity_actual.text = s0
        ll_ch1_intensity_actual.text = s1
        ll_ch2_intensity_actual.text = s2
        ll_ch3_intensity_actual.text = s3

        enableAll(true)

        if (autoUpdate) {
            autoUpdate = false
            nodeLL?.userInfo = _infoActual
            refreshLedLightUserInfoUI()
            autoUpdate = true
        }
    }

    fun refreshLedLightUserInfoUI()
    {
        val _userInfo = nodeLL?.userInfo?: return

        val disTrue = R.drawable.ic_check_box_outline_blank_black_24dp
        val disFalse = R.drawable.ic_check_box_black_24dp
        ll_ch0_disabled.setImageResource( if (_userInfo.channels[0].disabled) disTrue else disFalse)
        ll_ch1_disabled.setImageResource( if (_userInfo.channels[1].disabled) disTrue else disFalse)
        ll_ch2_disabled.setImageResource( if (_userInfo.channels[2].disabled) disTrue else disFalse)
        ll_ch3_disabled.setImageResource( if (_userInfo.channels[3].disabled) disTrue else disFalse)

        ll_ch0_intensity_bar.progress = _userInfo.channels[0].intensity
        ll_ch1_intensity_bar.progress = _userInfo.channels[1].intensity
        ll_ch2_intensity_bar.progress = _userInfo.channels[2].intensity
        ll_ch3_intensity_bar.progress = _userInfo.channels[3].intensity

        val s0 = "${intensityFromIdx(_userInfo.channels[0].intensity)}%"
        val s1 = "${intensityFromIdx(_userInfo.channels[1].intensity)}%"
        val s2 = "${intensityFromIdx(_userInfo.channels[2].intensity)}%"
        val s3 = "${intensityFromIdx(_userInfo.channels[3].intensity)}%"

        ll_ch0_intensity.setText(s0)
        ll_ch1_intensity.setText(s1)
        ll_ch2_intensity.setText(s2)
        ll_ch3_intensity.setText(s3)

        ll_chAll_intensity_bar.progress = _userInfo.channels[0].intensity
        ll_chAll_intensity.setText(s0)
        ll_chAll_mode.isChecked = (_userInfo.mode == CcdNodeLedLightMode.ON)
    }

    fun enableAll(enabled: Boolean) {

        ll_user_apply_auto.isEnabled     = enabled

        ll_ch0_disabled.isEnabled        = enabled
        ll_ch1_disabled.isEnabled        = enabled
        ll_ch2_disabled.isEnabled        = enabled
        ll_ch3_disabled.isEnabled        = enabled

        ll_ch0_intensity_bar.isEnabled   = enabled
        ll_ch1_intensity_bar.isEnabled   = enabled
        ll_ch2_intensity_bar.isEnabled   = enabled
        ll_ch3_intensity_bar.isEnabled   = enabled

        ll_ch0_intensity.isEnabled       = enabled
        ll_ch1_intensity.isEnabled       = enabled
        ll_ch2_intensity.isEnabled       = enabled
        ll_ch3_intensity.isEnabled       = enabled

        ll_chAll_intensity_bar.isEnabled = enabled
        ll_chAll_intensity.isEnabled     = enabled
        ll_chAll_mode.isEnabled          = enabled
    }

    fun updateSwitchInfoUI(addr: Int)
    {
        val _infoActualArr = nodeSwitch?.info?:return

        _infoActualArr.forEachIndexed { idx, _infoActual ->
            val state = when (_infoActual.event) {
                CcdNodeSwitchEvent.NONE -> "OFF"
                CcdNodeSwitchEvent.ON_OFF -> "OFF"
                CcdNodeSwitchEvent.OFF_ON -> "ON"
                CcdNodeSwitchEvent.ON_HOLD -> "HELD"
                CcdNodeSwitchEvent.HOLD_OFF -> "OFF"
                else -> "ERR"
            }
            val dstAddr = String.format("0x%02X", _infoActual.dstAddr)

            when (addr) {
                0x80.toByte().toInt() -> {
                    // 0x80 Node has only one button
                    when (idx) {
                        0 -> {
                            ll_sw0_state.text = state
                            ll_sw0_dst.setText(dstAddr)
                        }
                        1 -> {
                            ll_sw1_state.text = state
                            ll_sw1_dst.setText(dstAddr)
                        }
                    }
                }
                else -> {
                    Log.e(TAG, "Err")
                }
            }
        }
    }

    companion object {
        private const val TAG = "CcdUIxxx"

    }
}