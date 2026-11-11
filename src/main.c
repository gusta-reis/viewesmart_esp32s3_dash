/*
 * Copyright (c) 2026 Gustavo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

static uint32_t click_count;
static lv_obj_t *counter_label;
static lv_obj_t *slider_label;
static lv_obj_t *arc_widget;

static void btn_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_CLICKED) {
		click_count++;
		lv_label_set_text_fmt(counter_label, "Toques: %u", click_count);
		lv_arc_set_value(arc_widget, (click_count * 10U) % 101U);
	}
}

static void slider_event_cb(lv_event_t *e)
{
	lv_obj_t *slider = lv_event_get_target(e);
	int32_t val = lv_slider_get_value(slider);

	lv_label_set_text_fmt(slider_label, "Nivel: %d%%", (int)val);
	lv_arc_set_value(arc_widget, val);
}

static void dash_create_ui(uint16_t width, uint16_t height)
{
	lv_obj_t *scr = lv_screen_active();
	int32_t pad = 10;
	int32_t header_h = 42;
	int32_t card_h = (int32_t)height - header_h - (pad * 3);
	int32_t left_w = ((int32_t)width - (pad * 3)) * 46 / 100;
	int32_t right_w = (int32_t)width - left_w - (pad * 3);
	int32_t arc_sz = MIN(left_w - 30, card_h - 56);

	lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

	/* Cabeçalho */
	lv_obj_t *header = lv_obj_create(scr);

	lv_obj_set_size(header, (int32_t)width - (pad * 2), header_h);
	lv_obj_align(header, LV_ALIGN_TOP_MID, 0, pad);
	lv_obj_set_style_bg_color(header, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_border_color(header, lv_color_hex(0x38BDF8), LV_PART_MAIN);
	lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
	lv_obj_set_style_pad_all(header, 6, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t *title = lv_label_create(header);

	lv_label_set_text_fmt(title, "VIEWE ESP32-S3 | %ux%u RGB565 (Zephyr 4.5.0)",
			      width, height);
	lv_obj_set_style_text_color(title, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
	lv_obj_center(title);

	/* Painel Esquerdo: Medidor Arc */
	lv_obj_t *left_card = lv_obj_create(scr);

	lv_obj_set_size(left_card, left_w, card_h);
	lv_obj_align(left_card, LV_ALIGN_BOTTOM_LEFT, pad, -pad);
	lv_obj_set_style_bg_color(left_card, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_border_width(left_card, 0, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(left_card, LV_SCROLLBAR_MODE_OFF);

	arc_widget = lv_arc_create(left_card);
	lv_obj_set_size(arc_widget, arc_sz, arc_sz);
	lv_arc_set_rotation(arc_widget, 135);
	lv_arc_set_bg_angles(arc_widget, 0, 270);
	lv_arc_set_value(arc_widget, 40);
	lv_obj_align(arc_widget, LV_ALIGN_TOP_MID, 0, 4);

	counter_label = lv_label_create(left_card);
	lv_label_set_text(counter_label, "Toques: 0");
	lv_obj_set_style_text_color(counter_label, lv_color_hex(0x38BDF8), LV_PART_MAIN);
	lv_obj_align(counter_label, LV_ALIGN_BOTTOM_MID, 0, -2);

	/* Painel Direito: Controles Touch GT911 */
	lv_obj_t *right_card = lv_obj_create(scr);

	lv_obj_set_size(right_card, right_w, card_h);
	lv_obj_align(right_card, LV_ALIGN_BOTTOM_RIGHT, -pad, -pad);
	lv_obj_set_style_bg_color(right_card, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_border_width(right_card, 0, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(right_card, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t *info_label = lv_label_create(right_card);

	lv_label_set_text(info_label, "ESP32-S3 N16R8 (8MB OPI)\nLCD_CAM RGB + GT911 Touch");
	lv_obj_set_style_text_color(info_label, lv_color_hex(0x94A3B8), LV_PART_MAIN);
	lv_obj_align(info_label, LV_ALIGN_TOP_LEFT, 4, 2);

	lv_obj_t *btn = lv_button_create(right_card);

	lv_obj_set_size(btn, right_w - 40, 42);
	lv_obj_align(btn, LV_ALIGN_CENTER, 0, -6);
	lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

	lv_obj_t *btn_lbl = lv_label_create(btn);

	lv_label_set_text(btn_lbl, "Testar Touch GT911");
	lv_obj_center(btn_lbl);

	lv_obj_t *slider = lv_slider_create(right_card);

	lv_obj_set_width(slider, right_w - 50);
	lv_slider_set_range(slider, 0, 100);
	lv_slider_set_value(slider, 40, LV_ANIM_OFF);
	lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -26);
	lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

	slider_label = lv_label_create(right_card);
	lv_label_set_text(slider_label, "Nivel: 40%");
	lv_obj_set_style_text_color(slider_label, lv_color_hex(0xE2E8F0), LV_PART_MAIN);
	lv_obj_align(slider_label, LV_ALIGN_BOTTOM_MID, 0, -2);
}

int main(void)
{
	const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	struct display_capabilities caps;

	if (!device_is_ready(display_dev)) {
		LOG_ERR("Dispositivo de display nao esta pronto!");
		return -ENODEV;
	}

	display_get_capabilities(display_dev, &caps);
	LOG_INF("Display pronto: %ux%u (formato=0x%x)", caps.x_resolution, caps.y_resolution,
		caps.current_pixel_format);

	dash_create_ui(caps.x_resolution, caps.y_resolution);
	lv_timer_handler();

	display_blanking_off(display_dev);

	while (1) {
		uint32_t sleep_ms = lv_timer_handler();

		k_msleep(MIN(sleep_ms, 20U));
	}

	return 0;
}
