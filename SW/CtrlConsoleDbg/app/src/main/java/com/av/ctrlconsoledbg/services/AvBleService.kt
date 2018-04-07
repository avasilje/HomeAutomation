package com.av.bleservice

import android.Manifest
import android.app.Service
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.*
import android.content.Context
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.os.ParcelUuid
import android.support.v4.content.ContextCompat
import android.util.Log
import com.av.btstatusbar.fragment.*
import com.badoo.mobile.util.WeakHandler
import org.greenrobot.eventbus.EventBus
import org.greenrobot.eventbus.Subscribe
import org.greenrobot.eventbus.ThreadMode
import java.util.*
import java.util.concurrent.ConcurrentLinkedQueue

data class CcdBleDeviceDiscovered(val address : String)
data class AvBleRssiUpdate(val value : Int, val address: String?)
data class AvBleDisconnect(val address: String?)
data class AvBleStateChangedEvent(val status : Int)


data class AvBleGattEvent(
        val address : String,
        val code: Int
){
    companion object {
        const val GATT_FORGET = 1           // Remove gatt service from services
        const val GATT_DISCONNECT = 2       // Disconnect if can't be connected
    }
}

class AvBleService : Service() {
    enum class BtOp { READ, WRITE }

    data class ChIn(val uuid: UUID, val addr: String, val data: ByteArray, val op: BtOp? = null)
    data class ChOut(val uuid: UUID, val addr: String, val data: ByteArray, val op: BtOp? = null)

    private var mtu_request = 0
    private var bt_enabled = false
    private var mBluetoothScanner : BluetoothLeScanner? = null
    private var mBluetoothManager: BluetoothManager? = null
    private var mBluetoothAdapter: BluetoothAdapter? = null

    private var services = HashMap<String, AvBleGattService>()
    private var ble_transactions = AvBleTransactionQueue()

    inner class AvBleTransactionQueue() {
        var pending : AvBleTransactionState = AvBleTransactionState.IDLE
        private var queue = ConcurrentLinkedQueue<AvBleTransaction>()
        fun write(address: String, uuid: UUID, value: ByteArray?) : Boolean{
            // Log.d(TAG, "--- Write --- $uuid")
            var rc = false
            if (value == null) return rc
            val gattService = services[address]?: return rc
            val ch = gattService.gattCharacteristics[uuid]?: return rc
            ch.value = value
            if (gattService.ble_gatt?.writeCharacteristic(ch) == true) {
                pending = AvBleTransactionState.PENDING
                rc = true
            } else {
                Log.e(TAG, "Can't write characteristic")
            }
            return rc
        }
        fun write_descr(address: String, uuid: UUID, value: ByteArray?) : Boolean{
            // Log.d(TAG, "--- Write_descr --- $uuid")

            var rc = false
            if (value == null) return rc
            val gattService = services[address]?: return rc
            val ch = gattService.gattCharacteristics[uuid]?:return rc

            gattService.ble_gatt?.setCharacteristicNotification(ch, true)
            val descriptor = ch.getDescriptor(UUID_BLE_DESCR_CCC)

            //Log.d(TAG, "--------- ${descriptor.characteristic.uuid}, ${descriptor}")
            if (descriptor != null) {
                descriptor.value = value
                if (gattService.ble_gatt?.writeDescriptor(descriptor) == true){
                    pending = AvBleTransactionState.PENDING
                    rc = true
                } else {
                    Log.e(TAG, "Error writing characteristic's descriptor")
                }
            } else {
                Log.e(TAG, "Error writing characteristic - bad descriptor")
            }
            return rc
        }
        fun read(address: String, uuid: UUID) : Boolean {
            // Log.d(TAG, "--- Read --- $uuid")

            // Send immediately
            var rc = false
            val gattService = services[address]?: return rc
            val ch = gattService.gattCharacteristics[uuid]?:return rc
            if (gattService.ble_gatt?.readCharacteristic(ch) == true) {
                pending = AvBleTransactionState.PENDING
                rc = true
            } else {
                Log.e(TAG, "Can't read characteristic $uuid")
            }
            return rc
        }

        fun push_rd(address: String, uuid: UUID) {
            // Log.d(TAG, "--- Push_rd --- $uuid")

            if (pending == AvBleTransactionState.PENDING) {
                queue.add(AvBleTransaction(AvBleTransactionType.READ, address, uuid, null))
            } else {
                read(address, uuid)
            }
        }
        fun push_wr(address: String, uuid: UUID, value: ByteArray) {
            // Log.d(TAG, "--- Push_wr --- $uuid")
            if (pending == AvBleTransactionState.PENDING) {
                queue.add(AvBleTransaction(AvBleTransactionType.WRITE, address, uuid, value))
            } else {
                write(address, uuid, value)
            }
        }
        fun push_descr_wr(address: String, uuid: UUID, value: ByteArray) {
            if (pending == AvBleTransactionState.PENDING) {
                queue.add(AvBleTransaction(AvBleTransactionType.WRITE_DESCR, address, uuid, value))
            } else {
                write_descr(address, uuid, value)
            }
        }
        fun clear(address_to_clear: String) {
            var tr = queue.find { trf -> trf.address == address_to_clear }
            while (tr != null) {
                queue.remove(tr)
                tr = queue.find { trf -> trf.address == address_to_clear }
            }
        }
        fun next() {
            // Called on BLE transaction complete
            // Log.d(TAG, "--- Next --- (${queue.size})")

            var transaction_started = false
            var tr = queue.poll()

            while (tr != null) {

                when (tr.type) {
                    AvBleTransactionType.WRITE -> {
                        transaction_started = write(tr.address, tr.uuid, tr.value)
                    }
                    AvBleTransactionType.WRITE_DESCR -> {
                        transaction_started = write_descr(tr.address, tr.uuid, tr.value)
                    }
                    AvBleTransactionType.READ -> {
                        // check next transaction.
                        // Skip read if next transaction is the same as current one.
                        val tr_next = queue.peek()
                        if (!(tr_next != null &&
                                (tr_next.type == tr.type) &&
                                (tr_next.address == tr.address) &&
                                (tr_next.uuid == tr.uuid))) {

                            transaction_started = read(tr.address, tr.uuid)
                        } else {
                            Log.d(TAG, "Duplicate read transaction - skipped. ${tr_next.address}, ${tr_next.uuid}")
                        }
                    }
                }

                if (transaction_started) break  // Transaction started. Wait until onRead/Write callback called
                tr = queue.poll()               // Transaction not started due to an error or because of duplicated read.
                                                // Throw it away and poll next
            }

            if (!transaction_started) {
                pending = AvBleTransactionState.IDLE
            }
        }
    }
    enum class AvBleTransactionState { IDLE, PENDING }
    enum class AvBleTransactionType { WRITE, WRITE_DESCR, READ }
    data class AvBleTransaction(
            val type: AvBleTransactionType,
            val address: String,
            val uuid: UUID,
            val value: ByteArray? = null)

    var scanCallback :ScanCallback? = object : ScanCallback() {
        var log_skip_cnt = 0
        var terminated = false

        override fun onScanFailed(errorCode : Int) {
            Log.e(TAG, "Scan failed ($errorCode)")
            super.onScanFailed(errorCode)
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>?) {
            Log.e(TAG, "On batch result (${results?.size}")
            super.onBatchScanResults(results)
        }

        override fun onScanResult(callbackType: Int, result: ScanResult) {

            val scan_record = result.scanRecord
            if (scan_record == null) {
                Log.w(TAG, "Empty scan result")
                return
            }
            if (terminated) {
                Log.e(TAG, "Terminated scan triggered  ${result.device}")
                return
            }

            if (0 == log_skip_cnt.and(0xF)) {
                Log.d(TAG, "scan record $callbackType for dev ${result.device} (@rssi ${result.rssi}\n"
                        + "    uuid ${scan_record.serviceUuids}"
                        + "    dev name ${scan_record.deviceName}")
            }
            log_skip_cnt++

            connect(result)
        }
    }

    inner class AvBleGattService(val address : String ) {

        internal var mtu = 0
        internal var dev_name = String()
        internal var man_name = String()

        internal var connection_state = STATE_NOT_CONNECTED
        internal var ble_gatt: BluetoothGatt? = null
        internal var gattCharacteristics = HashMap<UUID, BluetoothGattCharacteristic>()
        internal var  connection_timeout = WeakHandler() // armed on connection initialization
                                                         // canceled on final connection stage (Service discovered)
        internal var  avoid_timeout = WeakHandler()      // time between disconnection and obliteration

        val rssi_avg: Int
            get() {
                if (rssi_ready) {
                    return rssi.sum().div(BT_RSSI_SAMPLE_CNT)
                } else {
                    return -128
                }
            }
        var rssi_sample: Int = -128
            set(v) {
                rssi[rssi_idx] = v
                rssi_idx = (rssi_idx + 1).and(BT_RSSI_SAMPLE_CNT - 1)
                if (rssi_idx == 0) {
                    rssi_ready = true
                    EventBus.getDefault().post(AvBleRssiUpdate(rssi_avg, address))
                }
            }
        private var rssi = Array(BT_RSSI_SAMPLE_CNT, {0})
        private var rssi_ready = false
        private var rssi_idx = 0

        internal val gatt_callback = object : BluetoothGattCallback() {

            override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
                val tid = android.os.Process.myTid()

                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    // Connection timeout still running and will be cleared
                    // after MTU negotiated and service fully discovered
                    connection_state = STATE_CONNECTED
                    // Att: MTU on server side might be already configured by previous connections
                    //      Probably need to be negotiated unconditionally?
                    //      TODO: Can MTU be different for different HOSTs?
                    if (mtu_request > 0) {
                        if (mtu_request < 23 ) mtu_request = 23
                        if (mtu_request > 512) mtu_request = 512
                        val rc = ble_gatt?.requestMtu(mtu_request)
                        Log.i(TAG, "Connected to GATT server (${gatt.device.address}). " +
                                "Requesting new MTU ($mtu_request): $rc")
                    } else {
                        val rc = ble_gatt?.discoverServices()
                        mtu = 23
                        Log.i(TAG, "Connected to GATT server (${gatt.device.address}). " +
                                "Default MTU is in use. Discovering started: $rc")
                    }
                    EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "${gatt.device.address} CONNECTED"))
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    Log.i(TAG, "Disconnected from GATT server (${gatt.device.address})," +
                            " $status, $newState, $gatt.")

                    if (connection_state == STATE_CONNECTING){
                        // Status code 133 can be received on connection
                        // Do nothing in that case, will retry connection attempt on next scan result
                        connection_timeout.removeCallbacksAndMessages(null)
                        // Postpone it?
                        connection_state = STATE_NOT_CONNECTED
                    } else {
                        connection_state = STATE_DISCONNECTED
                        EventBus.getDefault().post(AvBleGattEvent(address, AvBleGattEvent.GATT_DISCONNECT))
                    }
                } else {
                    connection_state = STATE_DISCONNECTED
                    Log.w(TAG, "Unknow BLE connection status: $status, $newState")
                    EventBus.getDefault().post(AvBleGattEvent(address, AvBleGattEvent.GATT_DISCONNECT))
                }
            }

            override fun onDescriptorRead(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
                Log.i(TAG, "onDescriptorRead ($status)")
                super.onDescriptorRead(gatt, descriptor, status)
            }
            override fun onMtuChanged(gatt: BluetoothGatt, negotiated_mtu: Int, status: Int) {
                mtu = negotiated_mtu
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    val rc = ble_gatt?.discoverServices()
                    Log.d(TAG, "MTU negotiated for $address ($negotiated_mtu)." +
                            " Starting service discovery: $rc")
                    EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "${gatt.device.address} MTU"))
                } else {
                    Log.e(TAG, "Can't negotiate MTU")
                    EventBus.getDefault().post(AvBleGattEvent(address, AvBleGattEvent.GATT_DISCONNECT))
                }
            }
            override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {

                if (connection_state != STATE_CONNECTED) {
                    Log.d(TAG,"------------------Debug me ---------------")
                    return
                }
                connection_timeout.removeCallbacksAndMessages(null)
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    // Read all device characteristics
                    val ble_gatt_services : List<BluetoothGattService> = ble_gatt?.services ?: ArrayList()
                    for (service in ble_gatt_services) {
                        val ble_gatt_characteristics = service.characteristics
                        for (ch in ble_gatt_characteristics) {
                            gattCharacteristics.put(ch.uuid, ch)
                        }
                    }

                    Log.i(TAG, "Service discovered ${gatt.device.address} ($status)")
                    EventBus.getDefault().post(CcdBleDeviceDiscovered(gatt.device.address))
                    EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "${gatt.device.address} DISCOVERED"))
                } else {
                    Log.e(TAG, "Error - onServicesDiscovered received: " + status)
                    EventBus.getDefault().post(AvBleGattEvent(address, AvBleGattEvent.GATT_DISCONNECT))
                }
            }
            override fun onCharacteristicWrite(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?, status: Int) {
                super.onCharacteristicWrite(gatt, characteristic, status)
                // Log.d(TAG, " --- On ch write: ($status) ${characteristic?.value?.size}, ${characteristic?.value}")
                ble_transactions.next()
                // Log.d(TAG, "--- EOF on Ch write ---")
            }

            override fun onReliableWriteCompleted(gatt: BluetoothGatt?, status: Int) {
                super.onReliableWriteCompleted(gatt, status)
                // Log.d(TAG, " --- On reliable write: ($status)")
                ble_transactions.next()
                // Log.d(TAG, "--- EOF on reliable write ---")
            }
            fun dispatchCh(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, ch_val: ByteArray) {
                EventBus.getDefault().post(
                        ChIn(characteristic.uuid, gatt.device.address, ch_val))
            }
            override fun onCharacteristicRead(gatt: BluetoothGatt,
                                              characteristic: BluetoothGattCharacteristic,
                                              status: Int) {
                Log.d(TAG, " --- On ch read: ${characteristic.uuid} ($status) ")
                if (status == BluetoothGatt.GATT_SUCCESS) {

                    when(characteristic.uuid) {
                        UUID_BLE_CHARACTERISTIC_DEVICE_NAME -> {
                            val str_val = characteristic.getStringValue(0)
                            dev_name = str_val
                        }
                        UUID_BLE_CHARACTERISTIC_MANUFACTURER -> {
                            val str_val = characteristic.getStringValue(0)
                            man_name = str_val
                        }
                        else -> {
                            dispatchCh(gatt, characteristic, characteristic.value)
                        }
                    }
                } else {
                    Log.e(TAG, "Error reading characteristic ${characteristic.uuid} ($status), ${gatt.device.address} ")
                }
                ble_transactions.next()
                // Log.d(TAG, "--- EOF on Ch Read ---")
            }

            override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
                // Log.d(TAG, " --- On descr write: ($status) ")
                super.onDescriptorWrite(gatt, descriptor, status)
            ble_transactions.next()
                // Log.d(TAG, " --- EOF On descr write: ($status) ")
            }
            override fun onCharacteristicChanged(gatt: BluetoothGatt,
                                                 characteristic: BluetoothGattCharacteristic) {
                if (characteristic.value.size == 4) { // AV: 3 => BLE HEADER SIZE
                    Log.d(TAG, " --- On ch changed short: (${characteristic.uuid}) ")
                    ble_transactions.push_rd(address, characteristic.uuid)
                }
                if (characteristic.value.size == mtu - 3) { // AV: 3 => BLE HEADER SIZE
                    Log.d(TAG, " --- On ch changed truncated: (${characteristic.uuid}) ")
                    ble_transactions.push_rd(address, characteristic.uuid)
                } else {
                    Log.d(TAG, " --- On ch changed: (${characteristic.uuid}) ")
                    dispatchCh(gatt, characteristic, characteristic.value)
                }
                // Log.d(TAG, " --- EOF On ch changed: (${characteristic.uuid}) ")
            }

            override fun onReadRemoteRssi(gatt: BluetoothGatt?, rssi_rm: Int, status: Int) {
                super.onReadRemoteRssi(gatt, rssi_rm, status)
                Log.d(TAG, " --- On remote rssi read ($rssi_rm) ")
            }
        }
        fun cleanup() {
            ble_gatt?.close()
            ble_gatt = null
            connection_timeout.removeCallbacksAndMessages(null)
            avoid_timeout.removeCallbacksAndMessages(null)
        }

        init {

        }
    }

    inner class LocalBinder : Binder() {
        internal val mAvBleService: AvBleService
            get() = this@AvBleService
    }

    override fun onBind(intent: Intent): IBinder? {
        return mBinder
    }

    override fun onUnbind(intent: Intent): Boolean {
        // All device should be already disconnected. So, nothing to do
        // ??? Check is any device still connected ???
        return super.onUnbind(intent)
    }

    private val mBinder = LocalBinder()

    private fun connect(result: ScanResult): Boolean {

        val address = result.device.address

        if (address.isNullOrBlank()) return false

        // TODO: for small arrays try linear checking
        var gattService = services[address]

        gattService?.rssi_sample = result.rssi
        if (gattService != null && gattService.connection_state != STATE_NOT_CONNECTED) {
            return true
        }

        // Protect access to HashMap elements with synchronized attribute
        if (mBluetoothAdapter == null) {
            Log.w(TAG, "BluetoothAdapter not initialized")
            return false
        }

        if (gattService == null) {
            gattService = AvBleGattService(address)
            services.put(address, gattService)
        }

        // Previously connected device.  Try to reconnect.
        if (gattService.ble_gatt != null) {
            if (gattService.ble_gatt!!.connect()) {
                Log.d(TAG, "Connecting existing GATT for $address, ${gattService.ble_gatt}")
                EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "$address RECONNECTING"))
            } else {
                Log.d(TAG, "Can't connect Gatt service ${gattService.ble_gatt?.device?.address}")
                EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "$address ERROR"))
                // Let timeout to be triggered
            }
        } else {
            val plcBtDevice = mBluetoothAdapter!!.getRemoteDevice(address)
            gattService.ble_gatt = plcBtDevice.connectGatt(this, false, gattService.gatt_callback)
            Log.d(TAG, "Connecting new GATT for $address, ${gattService.ble_gatt}")
            EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "$address CONNECTING"))
        }

        gattService.connection_state = STATE_CONNECTING

        gattService.connection_timeout.postDelayed({
            Log.w(TAG, "GATT Connection timeout for $address")
            if (gattService?.connection_state == STATE_CONNECTED) {
                // If timeout triggered after GATT service already connected
                // then, probably MTU can't be negotiated. So, clear it and
                // use default on next connection attempt
                mtu_request = 0
                Log.w(TAG, "BLE MTU size set to default for $address")
            }
            EventBus.getDefault().post(AvBleGattEvent(address, AvBleGattEvent.GATT_DISCONNECT))
        }, 15000)

        return true
    }

    /**
     * Called by service initiator in onBind
     * MainThread
     * ??? Call as an Intent ???
     */
    fun initialize() : Boolean {
        Log.d(TAG, "BLE service initialization")
        EventBus.getDefault().register(this)

        if (mBluetoothManager == null) {
            mBluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            if (mBluetoothManager == null) {
                Log.e(TAG, "Unable to initialize BluetoothManager.")
                return false
            }
        }

        mBluetoothAdapter = mBluetoothManager?.adapter
        if (mBluetoothAdapter == null) {
            Log.e(TAG, "Unable to obtain a BluetoothAdapter.")
            return false
        }

        if (mBluetoothAdapter?.isEnabled ?: false) {
            EventBus.getDefault().postSticky(AvBleStateChangedEvent(BluetoothAdapter.STATE_ON))
        }

        return true
    }

    fun deinitialize() {
        Log.d(TAG, "Deinitialize BLE Service (${services.size})")
        EventBus.getDefault().unregister(this)
        mBluetoothScanner?.flushPendingScanResults(scanCallback)
        mBluetoothScanner?.stopScan(scanCallback)
        scanCallback = null
        services.forEach { t ->
            val plcGattService = t.value
            Log.d(TAG, "Close Gatt Service (${plcGattService.address})")
            plcGattService.cleanup()
        }
        services.clear()
    }

    /**/
    fun readCharacteristic(address: String, uuid : UUID ) {

        if (mBluetoothAdapter == null) {
            Log.w(TAG, "BluetoothAdapter not initialized")
            return
        }
        ble_transactions.push_rd(address, uuid)
    }

    fun writeCharacteristicDescr(address: String, uuid: UUID, value: ByteArray) {
        if (mBluetoothAdapter == null) {
            Log.w(TAG, "BluetoothAdapter not initialized")
            return
        }
        ble_transactions.push_descr_wr(address, uuid, value)
    }
    fun writeCharacteristic(address: String, uuid: UUID, value: ByteArray) {

        if (mBluetoothAdapter == null) {
            Log.w(TAG, "BluetoothAdapter not initialized")
            return
        }
        ble_transactions.push_wr(address, uuid, value)
    }

    @Subscribe(sticky = true, threadMode = ThreadMode.BACKGROUND)
    fun onBtStateChanged(event : AvBleStateChangedEvent) {

        if (event.status == BluetoothAdapter.STATE_OFF) {
            bt_enabled = false
            mBluetoothScanner  = null
            EventBus.getDefault().post(BtStatusBarInfoEvent(
                    state = BtStatusBarInfoEvent.BLE_STATUS_BAR_STATE_OFF,
                    msg = "BT Disabled",
                    dev_num = 0))
        }
        if (event.status == BluetoothAdapter.STATE_ON) {
            bt_enabled = true

            /* Check permissions */
            if ( ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) !=
                    android.content.pm.PackageManager.PERMISSION_GRANTED) {

                EventBus.getDefault().post(BtStatusBarInfoEvent(
                        state = BtStatusBarInfoEvent.BLE_STATUS_BAR_STATE_PERMISSIONS,
                        msg = "BT need permissions...",
                        dev_num = 0))
                return
            }
            val blem = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            mBluetoothScanner = blem.adapter?.bluetoothLeScanner
            val settings = ScanSettings.Builder()
                    .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                    .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                    .build()

            val filters = ArrayList<ScanFilter>()
            val uuid_filter = ScanFilter.Builder()
                    .setServiceUuid(ParcelUuid(UUID_BLE_SERVICE_CCD))
                    .build()

            filters.add(uuid_filter)

            mBluetoothScanner?.startScan(filters, settings, scanCallback)
            EventBus.getDefault().post(BtStatusBarInfoEvent(
                    state = BtStatusBarInfoEvent.BLE_STATUS_BAR_STATE_ON,
                    msg = "BT scanning...",
                    dev_num = 0))
        }
    }

    @Subscribe(threadMode = ThreadMode.BACKGROUND)
    fun onGattEvent(event : AvBleGattEvent) {

        when (event.code) {
            AvBleGattEvent.GATT_DISCONNECT -> {
                ble_transactions.clear(event.address)

                val plcGattService = services[event.address]
                val state = plcGattService?.connection_state
                // plcGattService?.cleanup()
                if (state == STATE_CONNECTED || state == STATE_CONNECTING) {
                    Log.d(TAG, "GATT_DISCONNECT-1 for ${event.address}, state=$state")
                    plcGattService.ble_gatt?.disconnect()
                    val addr = event.address
                    // Normally onDisconnect callback should be triggered, but sometimes it doesn't
                    plcGattService.connection_timeout.postDelayed({
                        Log.w(TAG, "GATT Connection timeout for $addr, state=$state")
                        plcGattService.connection_state = STATE_DISCONNECTED
                        EventBus.getDefault().post(AvBleGattEvent(addr, AvBleGattEvent.GATT_DISCONNECT))
                    }, 2000)

                } else if (state == STATE_DISCONNECTED) {
                    Log.d(TAG, "GATT_DISCONNECT-2 for ${event.address}, state=$state")
                    plcGattService.cleanup()
                    ble_transactions.clear(event.address)
                    // Keep the service in service list for a while to avoid too fast reconnection
                    plcGattService.avoid_timeout.postDelayed({
                        EventBus.getDefault().post(
                                AvBleGattEvent(event.address, AvBleGattEvent.GATT_FORGET))
                    }, 5000)
                    // Update Device list
                    EventBus.getDefault().post(AvBleDisconnect(event.address))
                    EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "${event.address} DISCONNECTED"))

                } else {
                    Log.e(TAG, "----------- debug me --------")
                }
            }
            AvBleGattEvent.GATT_FORGET -> {
                val plcGattService = services.remove(event.address)
                Log.d(TAG, "GATT_FORGET for ${event.address}, state=${plcGattService?.connection_state}")
                EventBus.getDefault().post(BtStatusBarInfoEvent(evt = "${event.address} FORGOTTEN"))
            } else -> {
                Log.w(TAG, "GATT_UNKNOWN for ${event.address} ${event.code}")
            }
        }
    }

    @Subscribe(threadMode = ThreadMode.BACKGROUND)
    fun onEvent(ch_out: ChOut) {

        when(ch_out.op) {
            BtOp.WRITE -> {
                Log.d(TAG, "CH write event: ${ch_out.addr}, ${ch_out.uuid}, ${ch_out.data.size}")
                if (ch_out.data == null || ch_out.data.isEmpty()) {
                    return
                }

                writeCharacteristic(
                        ch_out.addr,
                        ch_out.uuid,
                        ch_out.data)

            }
            BtOp.READ -> {
                Log.d(TAG, "CH read event: ${ch_out.addr}, ${ch_out.uuid}")
                readCharacteristic(
                        ch_out.addr,
                        ch_out.uuid)
            }
        }
    }

    companion object {
        private val TAG = "CcdBleService"

        const private val BT_RSSI_SAMPLE_CNT = 16 // == 2^N

        const private val STATE_DISCONNECTED = 0        // Was connected before, but disconnected now
        const private val STATE_CONNECTING = 1
        const private val STATE_CONNECTED = 2
        const private val STATE_NOT_CONNECTED = 3       // Initial state

        /* Standard services */
        val UUID_BLE_SERVICE_DEVICE_INFORMATION  = UUID.fromString("0000180a-0000-1000-8000-00805f9b34fb")
        val UUID_BLE_CHARACTERISTIC_MODEL        = UUID.fromString("00002A24-0000-1000-8000-00805f9b34fb")
        val UUID_BLE_CHARACTERISTIC_SERIAL       = UUID.fromString("00002A25-0000-1000-8000-00805f9b34fb")
        val UUID_BLE_CHARACTERISTIC_SW_REVISION  = UUID.fromString("00002A26-0000-1000-8000-00805f9b34fb")
        val UUID_BLE_CHARACTERISTIC_HW_REVISION  = UUID.fromString("00002A27-0000-1000-8000-00805f9b34fb")
        val UUID_BLE_CHARACTERISTIC_MANUFACTURER = UUID.fromString("00002A29-0000-1000-8000-00805f9b34fb")
        val UUID_BLE_CHARACTERISTIC_DEVICE_NAME  = UUID.fromString("00002A00-0000-1000-8000-00805f9b34fb")

        /* CCD services */
        val UUID_BLE_SERVICE_CCD                 = UUID.fromString("7da93704-1e10-4888-abf8-028f4e20c02c")
        val UUID_BLE_CH_CCD_NODE                 = UUID.fromString("769c097e-7fab-4856-87c9-cf4bba527619")


        val UUID_BLE_DESCR_CCC = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB") // Client Characteristic Configuration
    }

}

