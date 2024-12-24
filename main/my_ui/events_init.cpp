/*
* Copyright 2024 NXP
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>
#include "lvgl.h"

#include "event_task.h"
#include "http_client.h"

#if LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif

#include "esp_log.h"

static uint8_t is_flash_begin = 0;

static const char *TAG = "lvgl_event";

uint8_t get_flash_begin_status(void)
{
	return is_flash_begin;
}

void set_flash_begin_status(uint8_t status)
{
	is_flash_begin = status;
}

static lv_timer_t *daplink_update_data_task;
static void screen_event_handler (lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	switch (code) {
	case LV_EVENT_SCREEN_LOADED:
	{
		daplink_update_data_task = lv_timer_create(screen_timer_cb, 50, &guider_ui);
		break;
	}
	case LV_EVENT_SCREEN_UNLOADED:
	{
		lv_timer_del(daplink_update_data_task);
		break;
	}
	default:
		break;
	}
}
static void screen_btn_1_event_handler (lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	switch (code) {
	case LV_EVENT_CLICKED:
	{
		// start program
		ESP_LOGI(TAG, "hello");
		start_swd_flash();
		set_flash_begin_status(1);
		lv_label_set_text(guider_ui.screen_btn_1_label, "Idle");
		lv_bar_set_value(guider_ui.screen_bar_1, 0, LV_ANIM_OFF);
		break;
	}
	default:
		break;
	}
}
void events_init_screen(lv_ui *ui)
{
	lv_obj_add_event_cb(ui->screen, screen_event_handler, LV_EVENT_ALL, NULL);
	lv_obj_add_event_cb(ui->screen_btn_1, screen_btn_1_event_handler, LV_EVENT_ALL, NULL);
}

void events_init(lv_ui *ui)
{

}