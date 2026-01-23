// Copyright 2020 QMK / James Young (@noroadsleft)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "ws2812.h"
#include "print.h"
#include <stdio.h>

// #define TRACKPAD_RXS 9   // Number of RX electrodes (columns)
// #define TRACKPAD_TXS 4   // Number of TX electrodes (rows)
// #define TRACKPAD_X_RES 1000  // X resolution
// #define TRACKPAD_Y_RES 1000  // Y resolution

#define SPACE_DOT_TERM 200
static uint16_t space_timer = 0;
static bool space_pending = false;

#define CAPS_TERM 200
#define BASE_LAYER 0

static uint16_t caps_timer = 0;
static bool caps_pending = false;

enum custom_keycodes {
    CAPS_SFT = SAFE_RANGE,
};

void keyboard_post_init_user(void) {
  ws2812_init();
  debug_enable=true;
  debug_matrix=true;
  debug_keyboard=true;
  debug_mouse=true;

  print("string\n");
}

void matrix_scan_user(void) {
    if (space_pending &&
        timer_elapsed(space_timer) > SPACE_DOT_TERM &&
        get_highest_layer(layer_state) == BASE_LAYER) {
        tap_code(KC_SPC);
        space_pending = false;
    }
    if (caps_pending && timer_elapsed(caps_timer) > CAPS_TERM) {
        caps_pending = false;
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == CAPS_SFT) {
        if (record->event.pressed) {
            // HOLD → act as Shift
            register_mods(MOD_BIT(KC_LSFT));

            // DOUBLE TAP → Caps Lock
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

    if (keycode == KC_SPC) {
        if (record->event.pressed) {
            if (space_pending && timer_elapsed(space_timer) < 200) {
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
        get_highest_layer(layer_state) == BASE_LAYER) {
        tap_code(KC_SPC);
        space_pending = false;
    }

    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_ESC,
        KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_BSPC, KC_MS_BTN1,
        CAPS_SFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_UP, KC_ENT, KC_MS_BTN2 ,
        MO(1), KC_LCTL, KC_LALT, KC_SPC, KC_NO, KC_NO, KC_DOT, KC_LEFT, KC_DOWN, KC_RGHT, KC_LGUI
    ),
    [1] = LAYOUT(
        KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_ESC,
        KC_AT, KC_HASH, KC_DLR, KC_UNDS, KC_AMPR, KC_BSLS, KC_PLUS, KC_LPRN, KC_RPRN, KC_DEL, KC_MS_BTN1,
        KC_CAPS, KC_ASTR, KC_DQUO, KC_QUOT, KC_COLN, KC_SCLN, KC_SLSH, KC_QUES, KC_UP, KC_ENT, KC_MS_BTN2,
        _______, KC_LCTL, KC_LALT, KC_SPC, KC_NO, KC_NO, KC_DOT, LSFT(KC_TAB), KC_DOWN, KC_TAB, KC_LGUI
    ),

};

