/*
 * ha_node_switch.c
 *
 * Created: 3/17/2019 5:07:32 PM
 *  Author: solit
 */ 

void switch_on_rx(uint8_t idx, const uint8_t *buf_in)
{
    UNREFERENCED_PARAM(idx);
    uint8_t len = buf_in[NLINK_HDR_OFF_LEN];

    if (buf_in[NLINK_HDR_OFF_TYPE] == NODE_TYPE_SWITCH) {
        // Switch event

        // Sanity check
        if (len > SWITCHES_NUM)
        return;

        for (uint8_t i = 0; i < len; i++) {
            uint8_t sw_event = buf_in[NLINK_HDR_OFF_DATA + i] & 0x0F;
            uint8_t sw_num = buf_in[NLINK_HDR_OFF_DATA + i] >> 4;
            ha_node_switch_on_event(sw_num, sw_event);
        }

    } else {
        // Unexpected event type
        return;
    }
}
