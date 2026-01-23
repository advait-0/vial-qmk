// Copyright 2020 QMK / James Young (@noroadsleft)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "ws2812.h"
#include "print.h"
#include <stdio.h>

/* ───── Space → ". " logic ───── */
#define SPACE_DOT_TERM 200
static uint16_t space_timer = 0;
static bool space_pending = false;
static bool fn_oneshot_active = false;

/* ───── Caps logic ───── */
#define CAPS_TERM 200
static uint16_t caps_timer = 0;
static bool caps_pending = false;

/* ───── Fn logic ───── */
#define FN_TERM 200

static uint16_t fn_timer = 0;
static bool fn_pending = false;
static bool fn_latched = false;

/* Layers */
#define L_BASE 0
#define L_FN1  1
#define L_FN2  2

enum custom_keycodes {
    CAPS_SFT = SAFE_RANGE,
    FN_KEY,
};

void keyboard_post_init_user(void) {
    ws2812_init();
    debug_enable = true;
    debug_matrix = true;
    debug_keyboard = true;
    debug_mouse = true;
    print("string\n");
}

void matrix_scan_user(void) {

    /* finalize single space */
    if (space_pending &&
        timer_elapsed(space_timer) > SPACE_DOT_TERM &&
        get_highest_layer(layer_state) == L_BASE) {
        tap_code(KC_SPC);
        space_pending = false;
    }

    /* reset caps window */
    if (caps_pending && timer_elapsed(caps_timer) > CAPS_TERM) {
        caps_pending = false;
    }

    /* reset fn tap window */
    if (fn_pending && timer_elapsed(fn_timer) > FN_TERM) {
        fn_pending = false;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {

    /* Clear sticky Fn after ANY key press */
    if (fn_oneshot_active &&
        record->event.pressed &&
        keycode != FN_KEY) {

        clear_oneshot_layer_state(ONESHOT_PRESSED);
        fn_oneshot_active = false;
    }

    /* ───── CAPS: hold = shift, double tap = caps lock ───── */
    if (keycode == CAPS_SFT) {
        if (record->event.pressed) {
            register_mods(MOD_BIT(KC_LSFT));

            if (caps_pending && timer_elapsed(caps_timer) < CAPS_TERM) {
                unregister_mods(MOD_BIT(KC_LSFT));
                tap_code(KC_CAPS);
                caps_pending = false;
                return false;
            }

            caps_timer = timer_read();
            caps_pending = true;
            return false;
        } else {
            unregister_mods(MOD_BIT(KC_LSFT));
        }
        return false;
    }

    /* ───── FN: tap / double tap / hold ───── */
    if (keycode == FN_KEY) {
        if (record->event.pressed) {

            /* double tap → latch layer 1 */
            if (fn_pending && timer_elapsed(fn_timer) < FN_TERM) {
                if (fn_latched) {
                    layer_off(L_FN1);
                    fn_latched = false;
                } else {
                    layer_on(L_FN1);
                    fn_latched = true;
                }
                fn_pending = false;
                return false;
            }

            /* first press → start hold + tap window */
            fn_pending = true;
            fn_timer = timer_read();

            /* HOLD → layer 2 */
            layer_on(L_FN2);
            return false;

        } else {
            /* release */

            layer_off(L_FN2);

            /* single tap → sticky layer 1 */
            if (fn_pending && !fn_latched) {
            set_oneshot_layer(L_FN1, ONESHOT_START);
            fn_oneshot_active = true;
            fn_pending = false;

            }
            return false;
        }
    }

    /* ───── Space logic ───── */
    if (keycode == KC_SPC) {
        if (record->event.pressed) {
            if (space_pending && timer_elapsed(space_timer) < SPACE_DOT_TERM) {
                tap_code(KC_DOT);
                tap_code(KC_SPC);
                space_pending = false;
                return false;
            }
            space_timer = timer_read();
            space_pending = true;
            return false;
        }
        return false;
    }

    if (space_pending &&
        record->event.pressed &&
        get_highest_layer(layer_state) == L_BASE) {
        tap_code(KC_SPC);
        space_pending = false;
    }

    return true;
}

/* ───── Keymaps ───── */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    /* BASE */
    [L_BASE] = LAYOUT(
        KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_ESC,
        KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_BSPC, KC_MS_BTN1,
        CAPS_SFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_UP, KC_ENT, KC_MS_BTN2,
        FN_KEY, KC_LCTL, KC_LALT, KC_SPC, KC_NO, KC_NO, KC_DOT, KC_LEFT, KC_DOWN, KC_RGHT, KC_LGUI
    ),

    /* FN1 — sticky / latched */
    [L_FN1] = LAYOUT(
        KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_ESC,
        KC_AT, KC_HASH, KC_DLR, KC_UNDS, KC_AMPR, KC_BSLS, KC_PLUS, KC_LPRN, KC_RPRN, KC_DEL, KC_MS_BTN1,
        KC_CAPS, KC_ASTR, KC_DQUO, KC_QUOT, KC_COLN, KC_SCLN, KC_SLSH, KC_QUES, KC_UP, KC_ENT, KC_MS_BTN2,
        _______, KC_LCTL, KC_LALT, KC_SPC, KC_NO, KC_NO, KC_DOT, LSFT(KC_TAB), KC_DOWN, KC_TAB, KC_LGUI
    ),

    /* FN2 — hold-only layer */
    [L_FN2] = LAYOUT(
        KC_TILD, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
        _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______
    ),
};
