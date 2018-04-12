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

        var nodeHvac: CcdNodeHvac? = null
        val cc = mCtrlConsole!!
        cc.nodes.forEach { _, v ->
            if (v.type == CcdNodeType.HVAC) {
                nodeHvac = v as CcdNodeHvac
            }
        }

        selector_butt_hvac.isEnabled = (nodeHvac != null)
        selector_butt_hvac.setOnClickListener { _ ->
            mCtrlConsole!!.supportFragmentManager.beginTransaction()
                    .replace(R.id.ctrl_console_function, CtrlConsoleSelectorFragment(), CtrlConsoleSelectorFragment::class.java.name)
                    .commit()

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