package com.av.ctrlconsoledbg.fragments

import android.content.Context
import android.os.Bundle
import android.support.v4.app.Fragment
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.av.ctrlconsoledbg.*
import kotlinx.android.synthetic.main.ctrl_console_selector.*
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode


class CtrlConsoleSelectorFragment : Fragment() {

    private var mCtrlConsole: CtrlConsoleDbgActivity? = null

    override fun onAttach(context: Context) {
        super.onAttach(context)
        mCtrlConsole = context as CtrlConsoleDbgActivity
    }

    init {
        Log.d(TAG, "CtrlConsoleSelectorFragment - initialized")
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
        return inflater?.inflate(R.layout.ctrl_console_selector, container, false)
    }

    override fun onViewCreated(view: View?, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        selector_butt_hvac.isEnabled = true
        selector_butt_hvac.setOnClickListener { _ ->
            mCtrlConsole!!.supportFragmentManager.beginTransaction()
                    .replace(R.id.ctrl_console_function, CtrlConsoleHvacFragment(), CtrlConsoleHvacFragment::class.java.name)
                    .commit()
        }

        selector_butt_ledlight.isEnabled = true
        selector_butt_ledlight.setOnClickListener { _ ->
            mCtrlConsole!!.supportFragmentManager.beginTransaction()
                    .replace(R.id.ctrl_console_function, CtrlConsoleLedLightFragment(), CtrlConsoleLedLightFragment::class.java.name)
                    .commit()
        }

        selector_butt_test.setOnClickListener { _ ->
//            EventBus.getDefault().post(
//                    CcdNodeInfoResp(
//                            addr = 0x70,
//                            type = CcdNodeType.LEDLIGHT,
//                            data = byteArrayOf(
//                                    1, // mode
//                                    3, // disabled mask
//                                    4, 5, 6 // Intensity
//                            )))
            var test0: Byte = 0
            var test1: Byte = 0
            EventBus.getDefault().post(
                    CcdNodeInfoResp(
                            addr = 0x70,
                            type = CcdNodeType.HVAC,
                            data = byteArrayOf(test0, test1)))
        }

    }

    override fun onDestroyView() {
        super.onDestroyView()
        EventBus.getDefault().unregister(this)

    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    fun onNodeInfo(info: CcdNode) {


    }

    companion object {
        private const val TAG = "CcdUISelector"

    }
}