#include "ui.h"
#include "led.h"
#include "touch.h"
#include "wifi.h"
#include <time.h>

enum screen_order
{
    SCREEN_LOGO,
    SCREEN_EVENT,
    SCREEN_NYAN,
    SCREEN_RADAR,
    SCREEN_RSSI,
    SCREEN_ADMIN,
    SCREEN_SNAKE,
    NUM_SCREENS
};

enum ctf_screen_order
{
    CTF_SCREEN_NYAN,
    CTF_SCREEN_CRILIN,
    CTF_SCREEN_OTP,
    CTF_NUM_SCREENS
};

static lv_obj_t *screens[NUM_SCREENS];
static int8_t current_screen = SCREEN_LOGO;

static lv_obj_t *ctf_screens[CTF_NUM_SCREENS];
static int8_t current_ctf_screen = CTF_SCREEN_NYAN;

static uint32_t last_trigger = -1;

static lv_obj_t *radar_node[MAX_NEARBY_NODE] = {0};
static lv_obj_t *radar_node_number[MAX_NEARBY_NODE] = {0};
static lv_obj_t *table_rssi, *table_event;

// Admin screen
static lv_obj_t *hotspot_switch, *sta_switch;
static lv_obj_t *hotspot_switch_text, *sta_switch_text;
static lv_obj_t *hotspot_ssid, *hotspot_ip;
static lv_obj_t *sta_client_ip, *sta_gateway_ip;

static bool ap_started = false;
static bool sta_connected = false;

static uint8_t admin_state = ADMIN_STATE_OFF;

static uint8_t up_button_press_counter = 0;
static uint8_t down_button_press_counter = 0;
static int8_t counter_screen = -1; // Initialize to invalid screen index
static lv_obj_t *otp_label;
static lv_obj_t *debug_label;
static lv_obj_t *otp_box;
static lv_obj_t *debug_box;

// Forward declarations
void ui_update_ip_info(void);
void ui_list_all_netifs(void);

// Nyan images
LV_IMG_DECLARE(saiyancat_only);
LV_IMG_DECLARE(saiyan_tail_sprite);
LV_IMG_DECLARE(nyan_star_sprite);
LV_IMG_DECLARE(crilitrunks_8bit);
LV_IMG_DECLARE(display_otp);
static lv_obj_t *tail;

static void anim_tail_cb(void *var, lv_anim_value_t v)
{
    lv_img_set_offset_x(tail, -v * 27);
    lv_img_set_offset_y(tail, v * 10);
}

static void anim_star_cb(void *var, lv_anim_value_t v)
{
    lv_obj_t *star = (lv_obj_t *)var;
    lv_img_set_offset_x(star, -v * 32);
}

void restore_current_task()
{
    if (current_screen == SCREEN_RSSI)
    {
        lv_task_set_prio(rssi_task_handle, LV_TASK_PRIO_LOW);
    }
    else if (current_screen == SCREEN_RADAR)
    {
        lv_task_set_prio(radar_task_handle, LV_TASK_PRIO_LOW);
    }
    else if ((current_screen == SCREEN_NYAN) && (current_ctf_screen == CTF_SCREEN_OTP))
    {
        ESP_LOGI(__FILE__, "ATTIVO TASK GENERAZIONE OTP");
        lv_task_set_prio(otp_task_handle, LV_TASK_PRIO_HIGHEST);
    }
}

void pause_current_task()
{
    if (current_screen == SCREEN_RSSI)
    {
        lv_task_set_prio(rssi_task_handle, LV_TASK_PRIO_OFF);
    }
    else if (current_screen == SCREEN_RADAR)
    {
        lv_task_set_prio(radar_task_handle, LV_TASK_PRIO_OFF);
    }
}

/*
 * Update the screen backlight status
 * returns status of backlight (true if backlight off)
 */

static bool ui_update_backlight(bool trigger)
{
    uint32_t span = lv_tick_get() - last_trigger;

    if (trigger)
    {
        set_screen_led_backlight(badge_obj.brightness_max);
        last_trigger = lv_tick_get();

        restore_current_task();
    }
    else
    {
        if (span > BRIGHT_OFF_TIMEOUT_MS)
        {
            set_screen_led_backlight(badge_obj.brightness_off);
            pause_current_task();
        }
        else if (span > BRIGHT_MID_TIMEOUT_MS)
        {
            set_screen_led_backlight(badge_obj.brightness_mid);
        }
    }

    /* Avoid doing action when backlight off */
    if (span > BRIGHT_OFF_TIMEOUT_MS)
    {
        return true;
    }
    return false;
}

void ui_send_wifi_event(int event)
{
    xQueueSend(wifi_queue, &event, portMAX_DELAY);
}

void scroll_up(lv_obj_t *screen)
{
    lv_obj_t *page = lv_obj_get_child(screen, NULL);
    lv_page_scroll_ver(page, 80);
}

void scroll_down(lv_obj_t *screen)
{
    lv_obj_t *page = lv_obj_get_child(screen, NULL);
    lv_page_scroll_ver(page, -80);
}

void ui_button_up()
{
    // Check if this is the first button press or if we've changed screens
    if (counter_screen != current_screen)
    {
        // Reset counters when screen changes
        up_button_press_counter = 0;
        down_button_press_counter = 0;
        // Update counter_screen to current screen
        counter_screen = current_screen;
        // printf("DEBUG: Started counting UP presses on screen index: %d\n", current_screen);
    }

    // Increment counter for UP button presses
    up_button_press_counter++;
    ESP_LOGI("UI", "UP button press count: %d on screen index: %d", up_button_press_counter, current_screen);

    // Check if we've reached 7 presses
    if (up_button_press_counter == 7)
    {
        // printf("DEBUG: UP button pressed 7 times on screen %s (index: %d)\n", current_screen);

        // Call set_completed() function when on SCREEN_LOGO (index 0)
        if (current_screen == SCREEN_LOGO)
        {
            ESP_LOGI("UI", "Summoning sequence activated!");
            set_completed();
        }

        // Reset the counter after reaching 7
        up_button_press_counter = 0;
    }

    // Check if the backlight update is needed
    if (ui_update_backlight(true))
    {
        return;
    }

    switch (current_screen)
    {
    case SCREEN_SNAKE:
        lv_task_set_prio(snake_task_handle, LV_TASK_PRIO_HIGHEST);
        snake_set_dir(1);
        break;
    case SCREEN_NYAN:
        if (current_ctf_screen == CTF_SCREEN_OTP)
        {
            bool otp_hidden = lv_obj_get_hidden(otp_box);
            bool debug_hidden = lv_obj_get_hidden(debug_box);

            lv_obj_set_hidden(otp_box, !otp_hidden);
            lv_obj_set_hidden(debug_box, !debug_hidden);

            char formattedtime[20];
            get_formatted_time(formattedtime, sizeof(formattedtime));

            char mac_str[18] = {0};
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     badge_obj.mac[0], badge_obj.mac[1], badge_obj.mac[2],
                     badge_obj.mac[3], badge_obj.mac[4], badge_obj.mac[5]);

            lv_label_set_text_fmt(debug_label, "DEBUG MODE ACTIVATED.\nLOCAL TIME:%s\nMAC ADDRESS:%s\nTimeSync Function at %p\n\nPowered by CapsCorp\nhttps://capscorp.cybersaiyan.it", formattedtime, mac_str, (void *)obtain_time);
        }
        break;
    case SCREEN_ADMIN:
        switch (admin_state)
        {
        case ADMIN_STATE_OFF:
            // AP and STA disabled: enable AP
            ESP_LOGI("UI", "Enabling AP mode (UP button in OFF mode)");
            ui_send_wifi_event(EVENT_HOTSPOT_START);
            admin_state = ADMIN_STATE_AP;
            // ui_update_ip_info();
            break;
        case ADMIN_STATE_AP:
            // AP enabled: disable AP
            ESP_LOGI("UI", "Disabling AP mode (UP button in AP mode)");
            ui_send_wifi_event(EVENT_HOTSPOT_STOP);
            admin_state = ADMIN_STATE_OFF;
            // ui_update_ip_info();
            break;
        case ADMIN_STATE_STA:
            // STA connected: enable STA and AP
            ESP_LOGI("UI", "Enabling AP mode (UP button in STA mode)");
            ui_send_wifi_event(EVENT_HOTSPOT_START);
            admin_state = ADMIN_STATE_APSTA;
            // Also test forcing labels to be visible for debugging
            // ui_update_ip_info();
            break;
        case ADMIN_STATE_APSTA:
            ESP_LOGI("UI", "Disabling AP mode (UP button in APSTA mode)");
            ui_send_wifi_event(EVENT_HOTSPOT_STOP);
            admin_state = ADMIN_STATE_STA;
            // ui_update_ip_info();
            break;
        }
        break;
    case SCREEN_EVENT:
    case SCREEN_RSSI:
        scroll_up(screens[current_screen]);
        break;
    default:
        ESP_LOGI(__FILE__, "Button up, no actions");
    }
}

void ui_button_down()
{
    // Check if this is the first button press or if we've changed screens
    if (counter_screen != current_screen)
    {
        // Reset counters when screen changes
        up_button_press_counter = 0;
        down_button_press_counter = 0;
        // Update counter_screen to current screen
        counter_screen = current_screen;
    }

    // Increment counter for DOWN button presses
    down_button_press_counter++;
    ESP_LOGI("UI", "DOWN button press count: %d on screen index: %d", down_button_press_counter, current_screen);

    // Check if we've reached 7 presses
    if (down_button_press_counter == 7)
    {
        // Call rainbow() function when on SCREEN_LOGO (index 0)
        if (current_screen == SCREEN_LOGO)
        {
            ESP_LOGI("UI", "Rainbow sequence activated!");
            rainbow();
        }

        // Reset the counter after reaching 7
        down_button_press_counter = 0;
    }

    if (ui_update_backlight(true))
    {
        return;
    }

    switch (current_screen)
    {
    case SCREEN_SNAKE:
        lv_task_set_prio(snake_task_handle, LV_TASK_PRIO_HIGHEST);
        snake_set_dir(-1);
        break;
    case SCREEN_NYAN:
        ui_ctf_switch_page_down();
        break;
    case SCREEN_ADMIN:
        switch (admin_state)
        {
        case ADMIN_STATE_OFF:
            // AP and STA disabled: enable STA
            ESP_LOGI("UI", "Enabling STA mode (DOWN button in OFF mode)");
            ui_send_wifi_event(EVENT_STA_START);
            admin_state = ADMIN_STATE_STA;
            ui_update_ip_info();
            break;
        case ADMIN_STATE_AP: // AP enabled: test showing IP labels
            ESP_LOGI("UI", "Enabling APSTA mode (DOWN button in AP mode)");
            ui_send_wifi_event(EVENT_STA_START);
            admin_state = ADMIN_STATE_APSTA;
            ui_update_ip_info();
            break;
        case ADMIN_STATE_STA: // STA mode: test showing IP labels
            ESP_LOGI("UI", "Force show IP labels test (DOWN button in STA mode)");
            ui_send_wifi_event(EVENT_STA_STOP);
            admin_state = ADMIN_STATE_OFF;
            ui_update_ip_info();
            break;
        case ADMIN_STATE_APSTA:
            ui_send_wifi_event(EVENT_STA_STOP);
            admin_state = ADMIN_STATE_AP;
            ui_update_ip_info();
            break;
        }
        break;
    case SCREEN_EVENT:
    case SCREEN_RSSI:
        scroll_down(screens[current_screen]);
        break;
    default:
        ESP_LOGI(__FILE__, "Button down, no actions");
    }
}

void ui_event_load()
{
    const char *buf = load_schedule_from_file();
    if (buf == NULL)
    {
        ESP_LOGI(__FILE__, "Failed to load schedule from file");
        return;
    }
    cJSON *schedule_json = cJSON_Parse(buf);
    free((char *)buf);

    cJSON *schedule_array = cJSON_GetObjectItem(schedule_json, "schedule");

    int size = cJSON_GetArraySize(schedule_array);
    lv_table_set_row_cnt(table_event, size);
    // lv_table_set_col_cnt(table_event, 2);

    char buff[256];
    for (int i = 0; i < size; i++)
    {
        cJSON *item = cJSON_GetArrayItem(schedule_array, i);

        // cJSON *index = cJSON_GetObjectItem(item, "index");
        cJSON *title = cJSON_GetObjectItem(item, "title");
        cJSON *day = cJSON_GetObjectItem(item, "day");
        cJSON *hour = cJSON_GetObjectItem(item, "hour");
        cJSON *speaker = cJSON_GetObjectItem(item, "speaker");
        cJSON *location = cJSON_GetObjectItem(item, "location");
        cJSON *duration = cJSON_GetObjectItem(item, "duration");

        // ESP_LOGI(__FILE__, "Index %d: %s", i, speaker->valuestring);

        if (day && strlen(day->valuestring) > 0)
        {
            snprintf(buff, sizeof(buff), "Day: %s\n", day->valuestring);
        }
        if (hour && strlen(hour->valuestring) > 0)
        {
            snprintf(buff + strlen(buff), sizeof(buff) - strlen(buff), "Time: %s\n", hour->valuestring);
        }
        if (location && strlen(location->valuestring) > 0)
        {
            snprintf(buff + strlen(buff), sizeof(buff) - strlen(buff), "Where: %s\n", location->valuestring);
        }
        if (duration && strlen(duration->valuestring) > 0)
        {
            snprintf(buff + strlen(buff), sizeof(buff) - strlen(buff), "How long: %s", duration->valuestring);
        }

        lv_table_set_cell_value(table_event, i, 0, buff);

        snprintf(buff, sizeof(buff), "%s\nby %s",
                 title->valuestring, speaker->valuestring);
        lv_table_set_cell_value(table_event, i, 1, buff);
    }

    cJSON_Delete(schedule_json);
}

static void ui_rssi_task(lv_task_t *arg)
{
    if (lv_scr_act() != screen_rssi)
    {
        lv_task_set_prio(rssi_task_handle, LV_TASK_PRIO_OFF);
        return;
    }

    const uint8_t count = count_ble_nodes();
    lv_table_set_row_cnt(table_rssi, count + 1);

    char buf[BADGE_BUF_SIZE] = {0};

    uint8_t pos = 1;

    for (int i = 0; i < MAX_NEARBY_NODE; i++)
    {
        if (!ble_nodes[i].active)
            continue;

        lv_table_set_cell_value(table_rssi, pos, 0, ble_nodes[i].name);
        lv_table_set_cell_align(table_rssi, pos, 0, LV_LABEL_ALIGN_CENTER);

        snprintf(buf, sizeof(buf), "%d dBm", ble_nodes[i].rssi);
        lv_table_set_cell_value(table_rssi, pos, 1, buf);
        lv_table_set_cell_align(table_rssi, pos, 1, LV_LABEL_ALIGN_CENTER);

        snprintf(buf, sizeof(buf), "0x0%d", ble_nodes[i].id);
        lv_table_set_cell_value(table_rssi, pos, 2, buf);
        lv_table_set_cell_align(table_rssi, pos, 2, LV_LABEL_ALIGN_CENTER);

        pos++;
    }
}

static void ui_backlight_task(lv_task_t *arg)
{
    ui_update_backlight(false);
}

static void ui_radar_task(lv_task_t *arg)
{

    if (lv_scr_act() != screen_radar)
    {
        lv_task_set_prio(radar_task_handle, LV_TASK_PRIO_OFF);
        return;
    }

    if (lv_scr_act() == screen_radar)
    {
        static int mode;
        if (mode)
        {
            for (int i = 0; i < MAX_NEARBY_NODE; i++)
            {
                if (ble_nodes[i].active)
                {
                    lv_coord_t x = rand() % 80, y = rand() % 60;

                    if (ble_nodes[i].rssi > -70)
                    {
                        // near range.
                        if (x < 50)
                            x = (LV_HOR_RES - x) / 2 - 20;
                        else
                            x = (LV_HOR_RES + x) / 2 - 20;
                        if (y < 40)
                            y = (LV_VER_RES - y) / 2 - 20;
                        else
                            y = (LV_VER_RES + y) / 2 - 20;
                    }
                    else if (ble_nodes[i].rssi > -90)
                    {
                        // middle range.
                        if (x < 50)
                            x = (LV_HOR_RES - 100 - x) / 2;
                        else
                            x = (LV_HOR_RES + 100 + x) / 2 - 20;
                        if (y < 40)
                            y = (LV_VER_RES - 80 - y) / 2;
                        else
                            y = (LV_VER_RES + 80 - y) / 2 - 20;
                    }
                    else
                    {
                        // long range
                        if (x < 50)
                            x = (LV_HOR_RES - 200 - x) / 2;
                        else
                            x = (LV_HOR_RES + 200 + x) / 2 - 20;
                        if (y < 40)
                            y = (LV_VER_RES - 160 - y) / 2;
                        else
                            y = (LV_VER_RES + 160 + y) / 2 - 20;
                    }

                    lv_obj_set_pos(radar_node[i], x, y);
                    lv_label_set_text_fmt(radar_node_number[i], "%d", ble_nodes[i].id);
                    lv_obj_set_hidden(radar_node[i], false);
                    lv_obj_fade_in(radar_node[i], 1000, 0);
                }
                else
                {
                    lv_obj_set_hidden(radar_node[i], true);
                }
            }
        }
        else
        {
            for (int i = 0; i < MAX_NEARBY_NODE; i++)
            {
                if (lv_obj_is_visible(radar_node[i]))
                    lv_obj_fade_out(radar_node[i], 1000, 0);
            }
        }

        mode = !mode;
    }
}

static void otp_task(lv_task_t *arg)
{
    if (current_screen != SCREEN_NYAN || current_ctf_screen != CTF_SCREEN_OTP)
    {
        lv_task_set_prio(otp_task_handle, LV_TASK_PRIO_OFF);
        lv_label_set_text(otp_label, "000000");
        return;
    }
    char otp[7];
    generate_otp("U55E3MN265I63UDLUTVOV2PKUL5PSTKJ", otp, sizeof(otp));
    lv_label_set_text(otp_label, otp);
}

void ui_screen_event_init()
{
    // page for event
    static lv_style_t style;
    lv_style_init(&style);

    screen_event = lv_obj_create(NULL, NULL);
    lv_obj_t *screen_event_page = lv_page_create(screen_event, NULL);

    lv_obj_t *scrollable = lv_page_get_scrollable(screen_event_page);
    lv_cont_set_layout(scrollable, LV_LAYOUT_PRETTY_TOP);
    lv_obj_set_style_local_pad_all(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(screen_event_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(screen_event_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_page_set_scrollbar_mode(screen_event_page, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_size(screen_event_page, LV_HOR_RES, LV_VER_RES);
    lv_coord_t content_w = lv_obj_get_width_grid(screen_event_page, 2, 1);

    lv_style_set_text_font(&style, LV_OBJ_PART_MAIN, &lv_font_montserrat_14);

    table_event = lv_table_create(screen_event_page, NULL);
    lv_obj_clean_style_list(table_event, LV_TABLE_PART_BG);
    lv_obj_set_drag_parent(table_event, true);
    lv_table_set_col_cnt(table_event, 2);
    lv_table_set_col_width(table_event, 0, 4 * (2 * content_w) / 10);
    lv_table_set_col_width(table_event, 1, 6 * (2 * content_w) / 10);
    lv_obj_add_style(table_event, LV_OBJ_PART_MAIN, &style);

    lv_obj_align(table_event, screen_event_page, LV_ALIGN_OUT_TOP_LEFT, 0, 0);

    ui_event_load();

    screens[SCREEN_EVENT] = screen_event;
}

void ui_screen_splash_init()
{
    LV_IMG_DECLARE(romhack);

    screen_logo = lv_obj_create(NULL, NULL);
    lv_obj_t *logo = lv_img_create(screen_logo, NULL);
    lv_img_set_src(logo, &romhack);
    lv_obj_align(logo, NULL, LV_ALIGN_CENTER, 0, 0);
    /*Change the logo's background color*/
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_opa(&style, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_bg_color(&style, LV_STATE_DEFAULT, LV_COLOR_MAKE(0xe8, 0x0b, 0x60));
    lv_obj_add_style(logo, LV_OBJ_PART_MAIN, &style);

    screens[SCREEN_LOGO] = screen_logo;
}

static void set_rainbow_y(void *bar, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)bar, v);
}
static void create_rainbow_bar(lv_obj_t *parent, lv_color_t color, int y_offset)
{
    lv_obj_t *bar1 = lv_obj_create(parent, NULL);
    lv_obj_set_size(bar1, 40, 10);
    lv_obj_set_y(bar1, y_offset);

    lv_obj_t *bar2 = lv_obj_create(parent, NULL);
    lv_obj_set_size(bar2, 40, 10);
    lv_obj_set_y(bar2, y_offset - 5);
    lv_obj_set_x(bar2, 40);

    lv_obj_t *bar3 = lv_obj_create(parent, NULL);
    lv_obj_set_size(bar3, 40, 10);
    lv_obj_set_y(bar3, y_offset);
    lv_obj_set_x(bar3, 80);

    lv_obj_t *bar4 = lv_obj_create(parent, NULL);
    lv_obj_set_size(bar4, 40, 10);
    lv_obj_set_y(bar4, y_offset + 5);
    lv_obj_set_x(bar4, 120);

    // Creazione style
    lv_style_t *style = lv_mem_alloc(sizeof(lv_style_t));
    lv_style_init(style);
    lv_style_set_bg_opa(style, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_bg_color(style, LV_STATE_DEFAULT, color);
    lv_style_set_border_width(style, LV_STATE_DEFAULT, 0);
    lv_style_set_radius(style, LV_STATE_DEFAULT, 0);
    lv_obj_add_style(bar1, LV_OBJ_PART_MAIN, style);
    lv_obj_add_style(bar2, LV_OBJ_PART_MAIN, style);
    lv_obj_add_style(bar3, LV_OBJ_PART_MAIN, style);
    lv_obj_add_style(bar4, LV_OBJ_PART_MAIN, style);

    lv_anim_t a1_y;
    lv_anim_init(&a1_y);
    lv_anim_set_var(&a1_y, bar1);
    lv_anim_set_exec_cb(&a1_y, set_rainbow_y);
    lv_anim_set_values(&a1_y, y_offset, y_offset + 10);
    lv_anim_set_time(&a1_y, 300);
    lv_anim_set_playback_time(&a1_y, 300);
    lv_anim_set_repeat_count(&a1_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a1_y);

    lv_anim_t a2_y;
    lv_anim_init(&a2_y);
    lv_anim_set_var(&a2_y, bar2);
    lv_anim_set_exec_cb(&a2_y, set_rainbow_y);
    lv_anim_set_values(&a2_y, y_offset - 5, y_offset + 15);
    lv_anim_set_time(&a2_y, 300);
    lv_anim_set_playback_time(&a2_y, 300);
    lv_anim_set_repeat_count(&a2_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a2_y);

    lv_anim_t a3_y;
    lv_anim_init(&a3_y);
    lv_anim_set_var(&a3_y, bar3);
    lv_anim_set_exec_cb(&a3_y, set_rainbow_y);
    lv_anim_set_values(&a3_y, y_offset, y_offset + 10);
    lv_anim_set_time(&a3_y, 300);
    lv_anim_set_playback_time(&a3_y, 300);
    lv_anim_set_repeat_count(&a3_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a3_y);

    lv_anim_t a4_y;
    lv_anim_init(&a4_y);
    lv_anim_set_var(&a4_y, bar4);
    lv_anim_set_exec_cb(&a4_y, (lv_anim_exec_xcb_t)set_rainbow_y);
    lv_anim_set_values(&a4_y, y_offset - 5, y_offset + 15);
    lv_anim_set_time(&a4_y, 300);
    lv_anim_set_playback_time(&a4_y, 300);
    lv_anim_set_repeat_count(&a4_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a4_y);
}

void create_star(lv_obj_t *parent, int x_offset, int y_offset, int index)
{
    lv_obj_t *star = lv_img_create(parent, NULL);
    lv_img_set_src(star, &nyan_star_sprite);
    lv_obj_set_size(star, 32, 32); // finestra di visualizzazione
    lv_img_set_offset_x(star, 0);
    lv_img_set_offset_y(star, 0);
    lv_obj_align(star, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_pos(star, x_offset, y_offset);

    lv_anim_t s;
    lv_anim_init(&s);
    lv_anim_set_var(&s, star);
    lv_anim_set_exec_cb(&s, anim_star_cb);
    lv_anim_set_values(&s, 0, 3);
    lv_anim_set_time(&s, 900);
    lv_anim_set_repeat_count(&s, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&s, 1000 * index);
    lv_anim_start(&s);
}

void create_cat(lv_obj_t *parent)
{
    lv_obj_t *img = lv_img_create(parent, NULL);
    lv_img_set_src(img, &saiyancat_only);
    lv_obj_set_pos(img, 140, 45);

    static lv_style_t style_paw;
    lv_style_init(&style_paw);
    lv_style_set_bg_opa(&style_paw, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_bg_color(&style_paw, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x80, 0x80, 0x80));

    lv_obj_t *paw = lv_obj_create(parent, NULL);
    lv_obj_set_size(paw, 10, 10);
    lv_obj_set_pos(paw, 140 + 30, 123 + 45);
    lv_style_set_bg_color(&style_paw, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_style_set_border_color(&style_paw, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_obj_add_style(paw, LV_OBJ_PART_MAIN, &style_paw);

    lv_obj_t *paw2 = lv_obj_create(parent, NULL);
    lv_obj_set_size(paw2, 10, 10);
    lv_obj_set_pos(paw2, 140 + 90, 123 + 45);
    lv_style_set_bg_color(&style_paw, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(paw2, LV_OBJ_PART_MAIN, &style_paw);

    lv_anim_t a_y;
    lv_anim_init(&a_y);
    lv_anim_set_var(&a_y, img);
    lv_anim_set_exec_cb(&a_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a_y, 42, 46); // oscillazione di 3 px
    lv_anim_set_time(&a_y, 300);
    lv_anim_set_playback_time(&a_y, 300);
    lv_anim_set_repeat_count(&a_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a_y);

    lv_anim_t a_x;
    lv_anim_init(&a_x);
    lv_anim_set_var(&a_x, img);
    lv_anim_set_exec_cb(&a_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a_x, 138, 142); // scorrimento di 100 px
    lv_anim_set_time(&a_x, 300);
    lv_anim_set_repeat_count(&a_x, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a_x);

    lv_anim_t p1_x;
    lv_anim_init(&p1_x);
    lv_anim_set_var(&p1_x, paw);
    lv_anim_set_exec_cb(&p1_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&p1_x, 140 + 30, 140 + 34);
    lv_anim_set_time(&p1_x, 300);
    lv_anim_set_repeat_count(&p1_x, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&p1_x);

    lv_anim_t p2_x;
    lv_anim_init(&p2_x);
    lv_anim_set_var(&p2_x, paw2);
    lv_anim_set_exec_cb(&p2_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&p2_x, 140 + 90, 140 + 94);
    lv_anim_set_time(&p2_x, 300);
    lv_anim_set_repeat_count(&p2_x, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&p2_x);

    lv_anim_t p1_y;
    lv_anim_init(&p1_y);
    lv_anim_set_var(&p1_y, paw);
    lv_anim_set_exec_cb(&p1_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&p1_y, 123 + 38, 123 + 41);
    lv_anim_set_time(&p1_y, 300);
    lv_anim_set_repeat_count(&p1_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&p1_y);

    lv_anim_t p2_y;
    lv_anim_init(&p2_y);
    lv_anim_set_var(&p2_y, paw2);
    lv_anim_set_exec_cb(&p2_y, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&p2_y, 123 + 38, 123 + 41);
    lv_anim_set_time(&p2_y, 300);
    lv_anim_set_repeat_count(&p2_y, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&p2_y);

    tail = lv_img_create(parent, NULL);
    lv_img_set_src(tail, &saiyan_tail_sprite);
    lv_obj_set_size(tail, 27, 32); // finestra di visualizzazione
    lv_img_set_offset_x(tail, 0);
    lv_img_set_offset_y(tail, 0);
    lv_obj_align(tail, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_pos(tail, 140 - 27, 123);

    lv_anim_t t;
    lv_anim_init(&t);
    lv_anim_set_var(&t, tail);
    lv_anim_set_exec_cb(&t, anim_tail_cb);
    lv_anim_set_values(&t, 0, 3);
    lv_anim_set_time(&t, 400);
    lv_anim_set_repeat_count(&t, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&t);

    lv_anim_t t_x;
    lv_anim_init(&t_x);
    lv_anim_set_var(&t_x, tail);
    lv_anim_set_exec_cb(&t_x, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&t_x, 138 - 27, 142 - 27);
    lv_anim_set_time(&t_x, 300);
    lv_anim_set_repeat_count(&t_x, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&t_x);
}

void ui_screen_saiyancat_init()
{
    lv_obj_t *scr = lv_obj_create(NULL, NULL);
    lv_obj_clean(scr);

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_opa(&style_bg, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bg, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x00, 0x33, 0x66));
    lv_obj_add_style(scr, LV_OBJ_PART_MAIN, &style_bg);

    create_rainbow_bar(scr, LV_COLOR_RED, 50 + 42);
    create_rainbow_bar(scr, LV_COLOR_ORANGE, 60 + 42);
    create_rainbow_bar(scr, LV_COLOR_YELLOW, 70 + 42);
    create_rainbow_bar(scr, LV_COLOR_GREEN, 80 + 42);
    create_rainbow_bar(scr, LV_COLOR_BLUE, 90 + 42);
    create_rainbow_bar(scr, LV_COLOR_PURPLE, 100 + 42);
    create_cat(scr);
    create_star(scr, 50, 32, 1);
    create_star(scr, 38, 190, 2);
    create_star(scr, 240, 190, 3);
    create_star(scr, 160, 205, 4);
    create_star(scr, 160, 10, 5);
    create_star(scr, 250, 35, 6);

    static lv_obj_t *saiyan_intro_text;
    saiyan_intro_text = lv_label_create(scr, NULL);
    static lv_style_t fg_white;
    lv_style_init(&fg_white);

    lv_style_set_text_color(&fg_white, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&fg_white, LV_OBJ_PART_MAIN, &lv_font_montserrat_14);
    lv_obj_add_style(saiyan_intro_text, LV_LABEL_PART_MAIN, &fg_white);

    lv_label_set_text(saiyan_intro_text, "As Goku attempts to\nreturn to Earth... ▼");
    lv_obj_set_pos(saiyan_intro_text, 30, 200);

    static lv_point_t arrow_points[4] = {
        {0, 0},  // angolo in alto a sinistra
        {10, 0}, // angolo in alto a destra
        {5, 6},  // punta in basso al centro
        {0, 0}   // ritorno al vertice
    };

    // Crea linea (triangolo)
    lv_obj_t *arrow = lv_line_create(scr, NULL);
    lv_line_set_points(arrow, arrow_points, 4);

    // Stile bianco
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_line_color(&style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_line_width(&style, LV_STATE_DEFAULT, 2);

    lv_obj_add_style(arrow, LV_LINE_PART_MAIN, &style);

    // Posiziona in basso a destra con margine 5px
    lv_obj_set_pos(arrow, 320 - 10 - 5, 240 - 6 - 5);

    screens[SCREEN_NYAN] = scr;
    ctf_screens[CTF_SCREEN_NYAN] = scr;
}

void ui_screen_crilin_init()
{
    lv_obj_t *scr = lv_obj_create(NULL, NULL);
    lv_obj_clean(scr);

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_opa(&style_bg, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bg, LV_STATE_DEFAULT, LV_COLOR_MAKE(0xFF, 0xFF, 0xFF));
    lv_obj_add_style(scr, LV_OBJ_PART_MAIN, &style_bg);

    lv_obj_t *img = lv_img_create(scr, NULL);
    lv_img_set_src(img, &crilitrunks_8bit);
    lv_obj_set_pos(img, 0, 0);

    static lv_obj_t *trunks_text;
    trunks_text = lv_label_create(scr, NULL);
    static lv_style_t trunks_style_text;
    lv_style_init(&trunks_style_text);

    lv_style_set_text_color(&trunks_style_text, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_text_font(&trunks_style_text, LV_OBJ_PART_MAIN, &lv_font_montserrat_16);
    lv_obj_add_style(trunks_text, LV_LABEL_PART_MAIN, &trunks_style_text);
    lv_label_set_text(trunks_text, "A guy landed on Earth, all the way from the future!");
    lv_label_set_long_mode(trunks_text, LV_LABEL_LONG_BREAK);
    lv_label_set_align(trunks_text, LV_LABEL_ALIGN_RIGHT);
    lv_obj_set_width(trunks_text, 145);
    lv_obj_set_pos(trunks_text, 160, 20);

    static lv_obj_t *crilin_text;
    crilin_text = lv_label_create(scr, NULL);
    static lv_style_t fg_black;
    lv_style_init(&fg_black);

    lv_style_set_text_color(&fg_black, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_text_font(&fg_black, LV_OBJ_PART_MAIN, &lv_font_montserrat_14);
    lv_obj_add_style(crilin_text, LV_LABEL_PART_MAIN, &fg_black);
    lv_label_set_text(crilin_text, "\"I don't even know his name, he gave me a weird device!\"");
    lv_label_set_long_mode(crilin_text, LV_LABEL_LONG_BREAK);
    lv_label_set_align(crilin_text, LV_LABEL_ALIGN_RIGHT);
    lv_obj_set_width(crilin_text, 145);
    lv_obj_set_pos(crilin_text, 145, 135);

    static lv_point_t arrow_points[4] = {
        {0, 0},  // angolo in alto a sinistra
        {10, 0}, // angolo in alto a destra
        {5, 6},  // punta in basso al centro
        {0, 0}   // ritorno al vertice
    };

    // Crea linea (triangolo)
    lv_obj_t *arrow = lv_line_create(scr, NULL);
    lv_line_set_points(arrow, arrow_points, 4);

    // Stile nero
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_line_color(&style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_line_width(&style, LV_STATE_DEFAULT, 2);

    lv_obj_add_style(arrow, LV_LINE_PART_MAIN, &style);

    // Posiziona in basso a destra con margine 5px
    lv_obj_set_pos(arrow, 320 - 10 - 5, 240 - 6 - 5);

    screen_crilin = scr;
    ctf_screens[CTF_SCREEN_CRILIN] = scr;
}

void get_formatted_time(char *buffer, size_t size)
{
    time_t now;
    struct tm timeinfo;

    time(&now);                   // ottieni timestamp corrente
    localtime_r(&now, &timeinfo); // converte in struttura tm

    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

void ui_screen_otp_init()
{
    lv_obj_t *scr = lv_obj_create(NULL, NULL);
    lv_obj_clean(scr);

    static lv_style_t style_screen;
    lv_style_init(&style_screen);
    lv_style_set_bg_opa(&style_screen, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_add_style(scr, LV_OBJ_PART_MAIN, &style_screen);

    lv_obj_t *img = lv_img_create(scr, NULL);
    lv_img_set_src(img, &display_otp);
    lv_obj_set_pos(img, 0, 0);

    otp_box = lv_obj_create(scr, NULL);
    lv_obj_set_size(otp_box, 200, 60);
    lv_obj_align(otp_box, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -100);
    lv_obj_set_style_local_bg_opa(otp_box, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_opa(otp_box, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);

    static lv_style_t style_label;
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_style_set_text_font(&style_label, LV_STATE_DEFAULT, &lv_font_montserrat_36);

    otp_label = lv_label_create(otp_box, NULL);
    lv_obj_add_style(otp_label, LV_LABEL_PART_MAIN, &style_label);
    lv_label_set_text(otp_label, "000000");
    lv_obj_align(otp_label, NULL, LV_ALIGN_CENTER, 0, 0);

    debug_box = lv_obj_create(scr, NULL);
    lv_obj_set_size(debug_box, 200, 90);
    lv_obj_align(debug_box, NULL, LV_ALIGN_IN_TOP_LEFT, 80, 50);
    lv_obj_set_style_local_bg_opa(debug_box, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_opa(debug_box, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);

    static lv_style_t style_debug;
    lv_style_init(&style_debug);
    lv_style_set_text_color(&style_debug, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_style_set_text_font(&style_debug, LV_STATE_DEFAULT, &lv_font_montserrat_10);

    debug_label = lv_label_create(debug_box, NULL);
    lv_obj_add_style(debug_label, LV_LABEL_PART_MAIN, &style_debug);
    lv_label_set_text(debug_label, "DEBUG MODE ACTIVATED");
    lv_obj_align(debug_label, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    lv_obj_set_hidden(debug_box, true);

    screen_otp = scr;
    ctf_screens[CTF_SCREEN_OTP] = scr;
}

void ui_screen_radar_init()
{
    // Page for radar
    LV_IMG_DECLARE(img_radar);

    screen_radar = lv_obj_create(NULL, NULL);
    lv_obj_t *img = lv_img_create(screen_radar, NULL);
    lv_img_set_src(img, &img_radar);
    lv_obj_align(img, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

    for (int i = 0; i < sizeof(radar_node) / sizeof(lv_obj_t *); i++)
    {
        radar_node[i] = lv_btn_create(img, NULL);
        lv_obj_set_size(radar_node[i], 20, 20);
        lv_btn_toggle(radar_node[i]); // set to solid color.
        lv_obj_set_hidden(radar_node[i], true);
        radar_node_number[i] = lv_label_create(radar_node[i], NULL);
        lv_label_set_text(radar_node_number[i], "X");
    }

    screens[SCREEN_RADAR] = screen_radar;
}

void ui_screen_rssi_init()
{
    // page for rssi
    static lv_style_t style;
    lv_style_init(&style);

    screen_rssi = lv_obj_create(NULL, NULL);
    lv_obj_t *screen_rssi_page = lv_page_create(screen_rssi, NULL);

    lv_obj_t *scrollable = lv_page_get_scrollable(screen_rssi_page);
    lv_cont_set_layout(scrollable, LV_LAYOUT_PRETTY_TOP);
    lv_obj_set_style_local_pad_all(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(scrollable, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(screen_rssi_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(screen_rssi_page, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_page_set_scrollbar_mode(screen_rssi_page, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_size(screen_rssi_page, LV_HOR_RES, LV_VER_RES);
    lv_coord_t content_w = lv_obj_get_width_grid(screen_rssi_page, 3, 1);

    lv_style_set_text_font(&style, LV_OBJ_PART_MAIN, &lv_font_montserrat_14);

    table_rssi = lv_table_create(screen_rssi_page, NULL);
    lv_obj_clean_style_list(table_rssi, LV_TABLE_PART_BG);
    lv_obj_set_drag_parent(table_rssi, true);
    lv_table_set_col_cnt(table_rssi, 3);
    lv_table_set_col_width(table_rssi, 0, content_w);
    lv_table_set_col_width(table_rssi, 1, content_w);
    lv_table_set_col_width(table_rssi, 2, content_w);
    lv_obj_add_style(table_rssi, LV_OBJ_PART_MAIN, &style);

    lv_obj_align(table_rssi, screen_rssi_page, LV_ALIGN_OUT_TOP_LEFT, 0, 0);

    lv_table_set_cell_value(table_rssi, 0, 0, "NAME");
    lv_table_set_cell_value(table_rssi, 0, 1, "RSSI");
    lv_table_set_cell_value(table_rssi, 0, 2, "ID");

    lv_table_set_cell_align(table_rssi, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table_rssi, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table_rssi, 0, 2, LV_LABEL_ALIGN_CENTER);

    screens[SCREEN_RSSI] = screen_rssi;
}

void ui_screen_admin_init()
{
    // page for admin
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, LV_OBJ_PART_MAIN, &lv_font_montserrat_14);

    static lv_style_t style2;
    lv_style_init(&style2);
    lv_style_set_text_font(&style2, LV_OBJ_PART_MAIN, &lv_font_montserrat_10);

    screen_admin = lv_obj_create(NULL, NULL);
    hotspot_switch = lv_btn_create(screen_admin, NULL);
    lv_obj_set_size(hotspot_switch, 200, 50);
    lv_obj_set_pos(hotspot_switch, 60, 35);
    hotspot_switch_text = lv_label_create(hotspot_switch, NULL);
    lv_label_set_text(hotspot_switch_text, "TURN ON AP");
    lv_obj_add_style(hotspot_switch_text, LV_LABEL_PART_MAIN, &style);

    // These are to be shown when AP is on . Starting hidden
    hotspot_ssid = lv_label_create(screen_admin, NULL);
    lv_obj_align(hotspot_ssid, hotspot_switch, LV_ALIGN_OUT_BOTTOM_MID, -50, 10);
    lv_obj_set_hidden(hotspot_ssid, true);
    lv_obj_add_style(hotspot_ssid, LV_LABEL_PART_MAIN, &style2);
    hotspot_ip = lv_label_create(screen_admin, NULL);
    lv_obj_align(hotspot_ip, hotspot_switch, LV_ALIGN_OUT_BOTTOM_MID, -50, 26);
    lv_obj_set_hidden(hotspot_ip, true);
    lv_obj_add_style(hotspot_ip, LV_LABEL_PART_MAIN, &style2);

    // These are to be shown when STA is on. Starting hidden.
    sta_client_ip = lv_label_create(screen_admin, NULL);
    lv_obj_align(sta_client_ip, hotspot_switch, LV_ALIGN_OUT_BOTTOM_MID, -50, 42);
    lv_label_set_text(sta_client_ip, "Client IP: [Not Connected]");
    lv_obj_set_hidden(sta_client_ip, true);
    lv_obj_add_style(sta_client_ip, LV_LABEL_PART_MAIN, &style2);
    sta_gateway_ip = lv_label_create(screen_admin, NULL);
    lv_obj_align(sta_gateway_ip, hotspot_switch, LV_ALIGN_OUT_BOTTOM_MID, -50, 58);
    lv_label_set_text(sta_gateway_ip, "Gateway: [Not Available]");
    lv_obj_set_hidden(sta_gateway_ip, true);
    lv_obj_add_style(sta_gateway_ip, LV_LABEL_PART_MAIN, &style2);

    sta_switch = lv_btn_create(screen_admin, NULL);
    lv_obj_set_size(sta_switch, 200, 50);
    lv_obj_set_pos(sta_switch, 60, 180);
    sta_switch_text = lv_label_create(sta_switch, NULL);
    lv_label_set_text(sta_switch_text, "CONNECT TO INTERNET");
    lv_obj_add_style(sta_switch_text, LV_LABEL_PART_MAIN, &style);

    screens[SCREEN_ADMIN] = screen_admin;
}

void ui_screen_snake_init()
{
    // page for snake
    screen_snake = lv_obj_create(NULL, NULL);
    snake_reset(screen_snake);

    screens[SCREEN_SNAKE] = screen_snake;
}

void ui_ap_start_handler()
{
    ap_started = true;

    ESP_LOGI("UI", "AP started handler called");
    lv_label_set_text(hotspot_switch_text, "TURN OFF AP");

    char buf[50] = {0};
    snprintf(buf, sizeof(buf), "SSID: %s | PASS: %s", badge_obj.ap_ssid, badge_obj.ap_password);
    lv_label_set_text(hotspot_ssid, buf);
    lv_obj_set_hidden(hotspot_ssid, false);

    ui_update_ip_info();
    // xTaskCreate(ui_delayed_ip_update_task, "delayed_ip_update", 2048, NULL, 5, NULL);

    lv_btn_set_state(hotspot_switch, LV_BTN_STATE_CHECKED_PRESSED);
    admin_state = ADMIN_STATE_AP;
}

void ui_ap_stop_handler()
{
    ap_started = false;

    lv_label_set_text(hotspot_switch_text, "TURN ON AP");
    lv_obj_set_hidden(hotspot_ssid, true);
    // lv_obj_set_hidden(hotspot_ip, true);

    lv_btn_set_state(hotspot_switch, LV_BTN_STATE_RELEASED); // enabl(admin_switch);
    admin_state = ADMIN_STATE_OFF;
}

void ui_sta_connected_handler()
{
    sta_connected = true;

    ESP_LOGI("UI", "STA connected handler called");
    ESP_LOGI("UI", "Current admin_state: %d", admin_state);
    ESP_LOGI("UI", "Current screen: %d", current_screen);

    lv_btn_set_state(sta_switch, LV_BTN_STATE_CHECKED_PRESSED);
    lv_label_set_text(sta_switch_text, "Connected to wifi");

    // Update IP information when connected as station immediately
    ESP_LOGI("UI", "About to call ui_update_ip_info from STA connected handler");
    ui_update_ip_info();
    ESP_LOGI("UI", "ui_update_ip_info call completed from STA connected handler");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ui_update_ip_info();
    // TODO: Also create a delayed task to retry getting IP info
    // xTaskCreate(ui_delayed_ip_update_task, "delayed_ip_update", 2048, NULL, 5, NULL);

    admin_state = ADMIN_STATE_STA;
}

void ui_sta_disconnected_handler()
{
    sta_connected = false;
    lv_btn_set_state(sta_switch, LV_BTN_STATE_RELEASED);
    lv_obj_set_hidden(sta_client_ip, true);
    lv_obj_set_hidden(sta_gateway_ip, true);
    ui_update_ip_info();
    admin_state = ADMIN_STATE_OFF;
}

void ui_sta_stop_handler()
{
    sta_connected = false;
    lv_label_set_text(sta_switch_text, "CONNECT TO INTERNET");
    lv_obj_set_hidden(sta_client_ip, true);
    lv_obj_set_hidden(sta_gateway_ip, true);
    admin_state = ADMIN_STATE_OFF;
}

void ui_connection_progress(uint8_t cur, uint8_t max)
{
    if (cur != max)
    {
        char buf[48] = {0}; // Increase the size of buf to accommodate the entire formatted string
        snprintf(buf, sizeof(buf), "Connecting to %s... (%d/%d)", badge_obj.sta_ssid, cur, max);
        lv_label_set_text(sta_switch_text, buf);
    }
    else
    {
        lv_label_set_text(sta_switch_text, "Connection failed!");
    }
}

void ui_update_ip_info()
{
    char buf[BADGE_BUF_SIZE + 40] = {0};

    ESP_LOGI("UI", "=== IP INFO DEBUG ===");
    ESP_LOGI("UI", "sta_connected: %s, ap_started: %s, admin_state: %d",
             sta_connected ? "true" : "false",
             ap_started ? "true" : "false",
             admin_state);

    // First, list all network interfaces for debugging
    ui_list_all_netifs();

    // Get AP interface and show AP IP when AP is started
    if (ap_started)
    {
        esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        ESP_LOGI("UI", "AP netif handle (WIFI_AP_DEF): %p", ap_netif);

        if (ap_netif)
        {
            esp_netif_ip_info_t ip_info;
            esp_err_t ret = esp_netif_get_ip_info(ap_netif, &ip_info);
            ESP_LOGI("UI", "AP esp_netif_get_ip_info returned: %s", esp_err_to_name(ret));
            ESP_LOGI("UI", "AP IP: " IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI("UI", "AP Gateway: " IPSTR, IP2STR(&ip_info.gw));
            ESP_LOGI("UI", "AP Netmask: " IPSTR, IP2STR(&ip_info.netmask));

            if (ret == ESP_OK && ip_info.ip.addr != 0)
            {
                // Show AP IP information
                snprintf(buf, sizeof(buf), "AP IP: " IPSTR " Listening on HTTP port\n.", IP2STR(&ip_info.ip));
                lv_label_set_text(hotspot_ip, buf);
                lv_obj_set_hidden(hotspot_ip, false);
                ESP_LOGI("UI", "Set hotspot_ip text to: %s", buf);
                return;
            }
        }
    }

    ESP_LOGI("UI", "sta_connected: %s, ap_started: %s, admin_state: %d",
             sta_connected ? "true" : "false",
             ap_started ? "true" : "false",
             admin_state);
    // Get STA interface and show STA IP when connected as station
    if (sta_connected)
    {
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        ESP_LOGI("UI", "STA netif handle (WIFI_STA_DEF): %p", sta_netif);

        if (sta_netif)
        {
            esp_netif_ip_info_t ip_info;
            esp_err_t ret = esp_netif_get_ip_info(sta_netif, &ip_info);
            ESP_LOGI("UI", "STA esp_netif_get_ip_info returned: %s", esp_err_to_name(ret));
            ESP_LOGI("UI", "STA IP: " IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI("UI", "STA Gateway: " IPSTR, IP2STR(&ip_info.gw));
            ESP_LOGI("UI", "STA Netmask: " IPSTR, IP2STR(&ip_info.netmask));

            if (ret == ESP_OK && ip_info.ip.addr != 0)
            {
                ESP_LOGI("UI", "STA IP is valid, updating UI labels...");
                snprintf(buf, sizeof(buf), "Client IP: " IPSTR, IP2STR(&ip_info.ip));
                lv_label_set_text(sta_client_ip, buf);
                lv_obj_set_hidden(sta_client_ip, false);
                ESP_LOGI("UI", "Set sta_client_ip text to: %s", buf);

                snprintf(buf, sizeof(buf), "Gateway: " IPSTR, IP2STR(&ip_info.gw));
                lv_label_set_text(sta_gateway_ip, buf);
                lv_obj_set_hidden(sta_gateway_ip, false);
                ESP_LOGI("UI", "Set admin_gateway_ip text to: %s", buf);
                ESP_LOGI("UI", "Successfully displayed STA IP info");
                return;
            }
            else
            {
                ESP_LOGW("UI", "STA IP is not valid or error occurred. ret=%s, ip.addr=0x%08x",
                         esp_err_to_name(ret), ip_info.ip.addr);
            }
        }
        else
        {
            ESP_LOGW("UI", "Could not get STA netif handle");
        }
    }

    // If we reach here, we couldn't get IP info through normal methods
    // Try iterating through all interfaces as fallback
    ESP_LOGI("UI", "Primary methods failed, trying to iterate through all interfaces...");

    esp_netif_t *netif = NULL;
    esp_netif_t *temp_netif = esp_netif_next(netif);
    bool ip_found = false;

    while (temp_netif != NULL && !ip_found)
    {
        esp_netif_ip_info_t ip_info;
        esp_err_t ret = esp_netif_get_ip_info(temp_netif, &ip_info);

        if (ret == ESP_OK && ip_info.ip.addr != 0)
        {
            const char *desc = esp_netif_get_desc(temp_netif);
            ESP_LOGI("UI", "Found valid IP on interface %s: " IPSTR,
                     desc ? desc : "unknown", IP2STR(&ip_info.ip));

            // Use interface description to determine type instead of IP range heuristic
            if (desc && strstr(desc, "ap"))
            {
                // AP interface
                snprintf(buf, sizeof(buf), "AP IP: " IPSTR, IP2STR(&ip_info.ip));
                lv_label_set_text(hotspot_ip, buf);
                lv_obj_set_hidden(hotspot_ip, false);

                /*snprintf(buf, sizeof(buf), "\nConnect to\nhttp://" IPSTR, IP2STR(&ip_info.gw));
                lv_label_set_text(admin_gateway_ip, buf);
                lv_obj_set_hidden(admin_gateway_ip, false);*/
                ip_found = true;
            }
            else if (desc && strstr(desc, "sta"))
            {
                // STA interface
                snprintf(buf, sizeof(buf), "Client IP: " IPSTR, IP2STR(&ip_info.ip));
                lv_label_set_text(sta_client_ip, buf);
                lv_obj_set_hidden(sta_client_ip, false);

                snprintf(buf, sizeof(buf), "Gateway: " IPSTR, IP2STR(&ip_info.gw));
                lv_label_set_text(sta_gateway_ip, buf);
                lv_obj_set_hidden(sta_gateway_ip, false);
                ip_found = true;
            }
            ESP_LOGI("UI", "Successfully displayed IP info from interface iteration");
        }

        temp_netif = esp_netif_next(temp_netif);
    }

    if (!ip_found)
    {
        ESP_LOGW("UI", "No valid IP information found to display");
        lv_obj_set_hidden(sta_client_ip, true);
        lv_obj_set_hidden(sta_gateway_ip, true);
        lv_obj_set_hidden(hotspot_ip, true);
    }

    ESP_LOGI("UI", "=== END IP INFO DEBUG ===");
}

static void ui_init(void)
{
    ui_screen_splash_init();

    ui_screen_event_init();

    ui_screen_radar_init();

    ui_screen_rssi_init();

    ui_screen_admin_init();

    ui_screen_snake_init();

    ui_screen_saiyancat_init();

    ui_screen_crilin_init();

    ui_screen_otp_init();

    radar_task_handle = lv_task_create(ui_radar_task, 2000, LV_TASK_PRIO_OFF, NULL);
    rssi_task_handle = lv_task_create(ui_rssi_task, 2000, LV_TASK_PRIO_OFF, NULL);
    snake_task_handle = lv_task_create(snake_task, 50, LV_TASK_PRIO_OFF, NULL);
    otp_task_handle = lv_task_create(otp_task, 50, LV_TASK_PRIO_OFF, NULL);

    // show first page.
    lv_scr_load(screens[current_screen]);

    // Turn on backlight and run backlight management task
    ui_update_backlight(true);
    backlight_task_handle = lv_task_create(ui_backlight_task, 1000, LV_TASK_PRIO_LOW, NULL);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_START, &ui_ap_start_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STOP, &ui_ap_stop_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ui_sta_connected_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &ui_sta_disconnected_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, &ui_sta_stop_handler, NULL));
}

static void ui_tick_task(void *arg)
{
    lv_tick_inc(1);
}

void ui_task(void *arg)
{
    SemaphoreHandle_t xGuiSemaphore;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();
    lvgl_driver_init();

    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);

    static lv_disp_buf_t disp_buf;
    uint32_t size_in_px = DISP_BUF_SIZE;

    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);
    // touch_init();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &ui_tick_task,
        .name = "ui_tick_task",
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000));

    ui_init();

    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY))
        {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    free(buf1);
    free(buf2);
    vTaskDelete(NULL);
}

void ui_switch_page_down()
{
    ui_update_backlight(true);

    current_ctf_screen = CTF_SCREEN_NYAN;

    current_screen++;
    current_screen %= NUM_SCREENS;
    ESP_LOGI("DISPLAY", "DISPLAY COUNTER: %d/%d", current_screen + 1, NUM_SCREENS);

    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_OVER_TOP, 300, 0, false);

    restore_current_task();
}

void ui_ctf_switch_page_down()
{
    ui_update_backlight(true);
    current_ctf_screen++;
    current_ctf_screen %= CTF_NUM_SCREENS;
    lv_scr_load_anim(ctf_screens[current_ctf_screen], LV_SCR_LOAD_ANIM_OVER_TOP, 300, 0, false);

    restore_current_task();
}

void ui_switch_page_up()
{
    ui_update_backlight(true);

    current_screen--;
    current_screen = (NUM_SCREENS + (current_screen % NUM_SCREENS)) % NUM_SCREENS;
    ESP_LOGI("DISPLAY", "DISPLAY COUNTER: %d/%d", current_screen + 1, NUM_SCREENS);

    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_OVER_BOTTOM, 300, 0, false);

    restore_current_task();
}

void button_task(void *arg)
{
    static button_event_t curr_ev;
    static button_event_t prev_ev[2];
    static QueueHandle_t button_events;
    button_events = button_init(PIN_BIT(BUTTON_1) | PIN_BIT(BUTTON_2));

    while (true)
    {
        if (xQueueReceive(button_events, &curr_ev, 1000 / portTICK_PERIOD_MS))
        {
            uint8_t btn_id = curr_ev.pin - 0x08;
            if (curr_ev.event == BUTTON_HELD)
            {
                set_screen_led_backlight(badge_obj.brightness_mid);
            }
            if (curr_ev.pin == BUTTON_1) // DOWN button event
            {
                if ((prev_ev[btn_id].event == BUTTON_HELD) && (curr_ev.event == BUTTON_UP))
                {
                    ui_switch_page_down();
                }
                else if ((prev_ev[btn_id].event == BUTTON_DOWN) && (curr_ev.event == BUTTON_UP))
                {
                    ui_button_down();
                }
            }

            if (curr_ev.pin == BUTTON_2) // UP button event
            {
                if ((prev_ev[btn_id].event == BUTTON_HELD) && (curr_ev.event == BUTTON_UP))
                {
                    ui_switch_page_up();
                }
                else if ((prev_ev[btn_id].event == BUTTON_DOWN) && (curr_ev.event == BUTTON_UP))
                {
                    ui_button_up();
                }
            }
            prev_ev[btn_id] = curr_ev;
        }
    }
}

void ui_list_all_netifs()
{
    ESP_LOGI("UI", "=== LISTING ALL NETWORK INTERFACES ===");

    // Try to iterate through all available network interfaces
    esp_netif_t *netif = NULL;
    esp_netif_t *temp_netif = esp_netif_next(netif);
    int count = 0;

    while (temp_netif != NULL)
    {
        count++;
        ESP_LOGI("UI", "Found netif %d: %p", count, temp_netif);

        // Get interface description
        const char *desc = esp_netif_get_desc(temp_netif);
        ESP_LOGI("UI", "Interface %d description: %s", count, desc ? desc : "unknown");

        // Get IP info for this interface
        esp_netif_ip_info_t ip_info;
        esp_err_t ret = esp_netif_get_ip_info(temp_netif, &ip_info);
        ESP_LOGI("UI", "Interface %d IP info (ret: %s):", count, esp_err_to_name(ret));
        ESP_LOGI("UI", "  IP: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI("UI", "  Gateway: " IPSTR, IP2STR(&ip_info.gw));
        ESP_LOGI("UI", "  Netmask: " IPSTR, IP2STR(&ip_info.netmask));

        temp_netif = esp_netif_next(temp_netif);
    }

    ESP_LOGI("UI", "Total network interfaces found: %d", count);
    ESP_LOGI("UI", "=== END NETIF LISTING ===");
}

void ui_manual_ip_update()
{
    ESP_LOGI("UI", "Manual IP update triggered from admin screen");
    ui_update_ip_info();
}

void ui_force_show_ip_labels()
{
    ESP_LOGI("UI", "Force showing IP labels for testing - calling ui_update_ip_info instead");
    ui_update_ip_info();
}
