/*
 * moNa2 layer notifier
 *
 * 有効レイヤーが変わるたびに、その状態を Raw HID (zzeneg/zmk-raw-hid) でホストへ送る。
 * ホストアプリ (moNa2 Studio のレイヤーオーバーレイ) が受け取って画面に表示する。
 *
 * レポート形式 (KeyPeek / srwi/zmk-keypeek-layer-notifier と互換):
 *   [0]     0xFF  レイヤー状態パケット
 *   [1]     4     状態 1 つ分のバイト数
 *   [2..5]  既定レイヤーのビット列 (uint32, little endian)
 *   [6..9]  有効レイヤーのビット列 (uint32, little endian)
 * ビットはレイヤーの並び順 (keymap に書いた順) で、ZMK Studio で並べ替えた場合も順番に変換して送る。
 *
 * 打鍵 (キー位置) は送らない。ホストから先頭 0xFF のレポートを受け取ると、現在の状態を返す
 * (アプリ起動時・再接続時に最新の状態を得るため)。
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include <raw_hid/events.h>

#define LAYER_PACKET_MARKER 0xFF
#define LAYER_STATE_SIZE 4

BUILD_ASSERT(CONFIG_RAW_HID_REPORT_SIZE >= 2 + 2 * LAYER_STATE_SIZE,
             "Raw HID report is too small for the layer state packet");

static uint8_t report[CONFIG_RAW_HID_REPORT_SIZE];

/* レイヤー ID のビット列を、並び順 (index) のビット列に変換する */
static uint32_t layer_bits_by_index(bool (*is_set)(zmk_keymap_layer_id_t)) {
    uint32_t bits = 0;
    for (zmk_keymap_layer_index_t i = 0; i < ZMK_KEYMAP_LAYERS_LEN && i < 32; i++) {
        zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(i);
        if (id != ZMK_KEYMAP_LAYER_ID_INVAL && is_set(id)) {
            bits |= BIT(i);
        }
    }
    return bits;
}

static bool is_default_layer(zmk_keymap_layer_id_t id) { return id == zmk_keymap_layer_default(); }

static void send_layer_state(void) {
    memset(report, 0, sizeof(report));
    report[0] = LAYER_PACKET_MARKER;
    report[1] = LAYER_STATE_SIZE;
    sys_put_le32(layer_bits_by_index(is_default_layer), &report[2]);
    sys_put_le32(layer_bits_by_index(zmk_keymap_layer_active), &report[2 + LAYER_STATE_SIZE]);

    raise_raw_hid_sent_event((struct raw_hid_sent_event){.data = report, .length = sizeof(report)});
}

static int layer_state_changed_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    send_layer_state();
    return ZMK_EV_EVENT_BUBBLE;
}

static int raw_hid_received_listener(const zmk_event_t *eh) {
    const struct raw_hid_received_event *ev = as_raw_hid_received_event(eh);
    if (ev != NULL && ev->length > 0 && ev->data[0] == LAYER_PACKET_MARKER) {
        send_layer_state();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(mona2_layer_notifier, layer_state_changed_listener);
ZMK_SUBSCRIPTION(mona2_layer_notifier, zmk_layer_state_changed);

ZMK_LISTENER(mona2_layer_request, raw_hid_received_listener);
ZMK_SUBSCRIPTION(mona2_layer_request, raw_hid_received_event);
