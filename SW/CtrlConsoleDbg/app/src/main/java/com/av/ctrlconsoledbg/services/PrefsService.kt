package com.av.prefs

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.provider.Settings
import com.av.ctrlconsoledbg.CtrlConsoleDbgActivity


class AvPrefsService(private val mContext: CtrlConsoleDbgActivity) {

    private var prefs: SharedPreferences? = null
    private fun prefs_getter(key: String): String? {
        if (prefs!!.contains(key)) {
            return prefs!!.getString(key, "{}")
        }
        return null
    }
    private fun prefs_setter(key: String, in_str: String?) {
        val editor = prefs!!.edit()
        editor.putString(key, in_str)
        editor.apply()
    }

    var app_prefs: String?
        get() = prefs_getter(PREFS_KEY_APP)
        set (in_str) = prefs_setter(PREFS_KEY_APP, in_str)

    fun initialize() {
        // TODO: check request result
        // TODO: register broadcast receiver???
        prefs = mContext.getSharedPreferences("Ccd", Context.MODE_PRIVATE)
        if (!Settings.System.canWrite(mContext)) {
            val intent = Intent(android.provider.Settings.ACTION_MANAGE_WRITE_SETTINGS)
            intent.data = Uri.parse("package:" + mContext.getPackageName())
            mContext.startActivity(intent)
        }

    }

    companion object {
        private val TAG = "CcdPrefsService"

        /* Preferences related stuff */
        private val PREFS_KEY_APP = "com.av.ccd.prefs"
    }

}
