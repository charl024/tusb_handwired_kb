#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "bsp/board_api.h"

#include "class/hid/hid.h"

#define HEIGHT 4
#define WIDTH 12

// TOTAL PINS: HEIGHT + WIDTH = 4 + 12 = 14
const uint8_t ROWS[HEIGHT] = {9, 10, 11, 12};
const uint8_t COLS[WIDTH] = {13, 14, 15, 16, 17, 18, 
                          19, 20, 21, 22, 23, 24};

const uint8_t keys_l1[HEIGHT*WIDTH] = 
{
    HID_KEY_ESCAPE,       HID_KEY_Q,    HID_KEY_W,        HID_KEY_E,    HID_KEY_R,    HID_KEY_T,                       HID_KEY_Y,      HID_KEY_U,                      HID_KEY_I,                   HID_KEY_O,                   HID_KEY_P,             HID_KEY_BACKSPACE,
    HID_KEY_TAB,          HID_KEY_A,    HID_KEY_S,        HID_KEY_D,    HID_KEY_F,    HID_KEY_G,                       HID_KEY_H,      HID_KEY_J,                      HID_KEY_K,                   HID_KEY_L,                   HID_KEY_SEMICOLON,     HID_KEY_ENTER,
    HID_KEY_SHIFT_LEFT,   HID_KEY_Z,    HID_KEY_X,        HID_KEY_C,    HID_KEY_V,    HID_KEY_B,                       HID_KEY_N,      HID_KEY_M,                      HID_KEY_COMMA,               HID_KEY_PERIOD,              HID_KEY_ARROW_UP,      HID_KEY_SLASH,
    HID_KEY_CONTROL_LEFT, HID_KEY_NONE, HID_KEY_ALT_LEFT, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE /* raise layer */,  HID_KEY_SPACE,  HID_KEY_NONE /* lower layer */, HID_KEY_NONE /* function */, HID_KEY_ARROW_LEFT,          HID_KEY_ARROW_DOWN,    HID_KEY_ARROW_RIGHT
};

uint8_t keycodes[6] = {0};

// Blink pattern
enum
{
    BLINK_NOT_MOUNTED = 250, // device not mounted
    BLINK_MOUNTED = 1000,    // device mounted
    BLINK_SUSPENDED = 2500,  // device is suspended
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void pin_setup() {
    // set rows as INPUT, no PULL-UP initially
    for (size_t row = 0; row < HEIGHT; ++row) {
        uint8_t curr_row = ROWS[row];
        gpio_init(curr_row);
        gpio_set_dir(curr_row, GPIO_IN);
        gpio_pull_up(curr_row);
    }

    // set cols as INPUT, with PULL-UP initially
    for (size_t col = 0; col < WIDTH; ++col) {
        uint8_t curr_col = COLS[col];
        gpio_init(curr_col);
        gpio_set_dir(curr_col, GPIO_IN);
    }
}

int key_pin_reset() {
    for (size_t y = 0; y < HEIGHT y++) {
        for (uint8_t x = 0; x < WIDTH; x++) {
            gpio_set_dir(curr_col, GPIO_IN);
        }
        gpio_set_dir(curr_row, GPIO_IN);
    }
    return 0;
}

// general idea is a nested for-loop, looping over cols first then row inside
int key_scan() {
    for (size_t i = 0; i < 6; i++) {
        keycodes[i] = HID_KEY_NONE;
    }

    uint8_t pressed = 0;
    uint8_t mult_key_idx = 0;

    for (size_t y = 0; y < HEIGHT y++) {
        uint8_t curr_row = ROWS[y];
        gpio_set_dir(curr_row, GPIO_OUT);
        gpio_put(curr_row, 0);

        for (uint8_t x = 0; x < WIDTH; x++) {
            uint8_t curr_col = COLS[x];
            gpio_set_dir(curr_col, GPIO_IN);
            uint8_t curr_state = gpio_get(curr_col);

            if (!curr_state) {
                uint8_t idx = y * WIDTH + x;
                keycodes[mult_key_idx] = keys_l1[idx];
                mult_key_idx++;
                pressed = 1;
                if (mult_key_idx >= 6) {
                    key_pin_reset();
                    return pressed;
                }
            }

            gpio_set_dir(curr_col, GPIO_IN);
        }
        gpio_set_dir(curr_row, GPIO_IN);
    }

    // // for each col...
    // for (size_t col = 0; col < WIDTH; ++col) {
    //     // set current col from INPUT (with pull-up) to OUTPUT, drive low
    //     uint8_t curr_col = COLS[col];
    //     gpio_set_dir(curr_col, GPIO_OUT);
    //     gpio_put(curr_col, 1);

    //     // for each row pin in curr col...
    //     for (size_t row = 0; row < HEIGHT; ++row) {
    //         // set current row s INPUT, with PULL-UP
    //         uint8_t curr_row = ROWS[row];
    //         gpio_set_dir(curr_row, GPIO_IN);
    //         // gpio_pull_up(curr_row);

    //         // Check if key is pressed
    //         uint8_t curr_state = gpio_get(curr_row);

    //         if (!curr_state) {
    //             uint8_t idx = row * WIDTH + col;
    //             keycodes[mult_key_idx] = keys_l1[idx];
    //             mult_key_idx++;
    //             pressed = 1;
    //             if (mult_key_idx >= 6) {
    //                 key_pin_reset();
    //                 return pressed;
    //             }
    //         }

    //         // reset row pin
    //         // gpio_disable_pulls(curr_row);
    //         gpio_set_dir(curr_row, GPIO_IN);
    //     }
    //     // reset col pin
    //     gpio_set_dir(curr_col, GPIO_IN);
    //     // gpio_pull_up(curr_col);
    // }

    return pressed;
}

void blink_led() {
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    sleep_ms(50);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    sleep_ms(50);
}

static void send_hid_report(uint32_t btn) {
    if (!tud_hid_ready()) return;

    static uint8_t activated = 0;

    if (btn) {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycodes);
        // blink_led();

        activated = 1;
        sleep_ms(10);

    } else {
        if (activated) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        activated = 0;
    }
}

void hid_task() {
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    uint8_t btn = key_scan();

    if (tud_suspended() && btn) {
        tud_remote_wakeup();
    } else {
        send_hid_report(btn);
    }
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len) {
    (void) instance;
    (void) len;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void)instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
        // Set keyboard LED e.g Capslock, Numlock etc...
        if (report_id == REPORT_ID_KEYBOARD)
        {
            // bufsize should be (at least) 1
            if (bufsize < 1)
                return;

            uint8_t const kbd_leds = buffer[0];

            if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
            {
                // Capslock On: disable blink, turn led on
                blink_interval_ms = 0;
                board_led_write(true);
            }
            else
            {
                // Caplocks Off: back to normal blink
                board_led_write(false);
                blink_interval_ms = BLINK_MOUNTED;
            }
        }

    }
}

void tud_mount_cb(void)
{
blink_interval_ms = BLINK_MOUNTED;
}

void tud_umount_cb(void)
{
blink_interval_ms = BLINK_NOT_MOUNTED;
}


void tud_suspend_cb(bool remote_wakeup_en)
{
(void) remote_wakeup_en;
blink_interval_ms = BLINK_SUSPENDED;
}

void tud_resume_cb(void)
{
blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}
 

int main()
{
    board_init();
    tud_init(BOARD_TUD_RHPORT);

    pin_setup();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    if (board_init_after_tusb) {
        board_init_after_tusb();
    }

    while (true) {
        tud_task();
        hid_task();

        sleep_ms(10);
    }
}
