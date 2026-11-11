/*
 * Copyright (c) 2026 Gustavo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

#include "senna_telemetry.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define SHIFT_LEDS_COUNT 15

/* LEDs Fisicos Enderecaveis WS2812D (Total: 21) */
#define PHYSICAL_LEDS_TOTAL  21
#define PHYSICAL_LEDS_TOP    15
#define PHYSICAL_LEDS_SIDE   3
#define SHIFT_BLINK_RPM      14400U

static const struct device *strip_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(led_strip));
static struct led_rgb strip_pixels[PHYSICAL_LEDS_TOTAL];

/* Elementos visuais da UI */
static lv_obj_t *shift_leds[SHIFT_LEDS_COUNT];
static lv_obj_t *gear_label;
static lv_obj_t *speed_label;
static lv_obj_t *rpm_label;
static lv_obj_t *rpm_bar;
static lv_obj_t *lap_time_label;
static lv_obj_t *corner_label;
static lv_obj_t *s1_pill;
static lv_obj_t *s2_pill;
static lv_obj_t *s3_pill;
static lv_obj_t *start_btn;
static lv_obj_t *start_btn_label;

/* Tracado de Interlagos */
static lv_obj_t *track_box;
static lv_obj_t *track_bg_line;
static lv_obj_t *track_s1_line;
static lv_obj_t *track_s2_line;
static lv_obj_t *track_s3_line;
static lv_obj_t *track_progress_line;
static lv_obj_t *car_marker;

/* Controle da animacao */
static lv_timer_t *telemetry_timer;
static uint32_t anim_start_uptime_ms;
static bool anim_running;

/* Limites de rotacao para a barra de 15 LEDs da F1 (10.700 a 14.500 RPM) */
static const uint16_t s_shift_rpm_limits[SHIFT_LEDS_COUNT] = {
	10700, 11000, 11300, 11600, 12000, /* 5 Verdes */
	12400, 12800, 13100, 13400, 13800, /* 5 Vermelhos */
	14100, 14250, 14380, 14450, 14500  /* 5 Roxos / Shift point */
};

static void update_physical_ws2812_leds(uint16_t rpm, uint32_t now_ms)
{
	static uint32_t last_led_update_ms;

	if (strip_dev == NULL || !device_is_ready(strip_dev)) {
		return;
	}

	/* Taxa de atualizacao estavel e suave para os LEDs WS2812D (~33 Hz / 30 ms).
	 * Garante intervalo ocioso e tempo de latch/reset (> 280 us) impecaveis entre frames */
	if ((now_ms - last_led_update_ms) < 30U) {
		return;
	}
	last_led_update_ms = now_ms;

	/* 1. LEDs Superiores 0..14 (Shift Lights) */
	if (rpm >= SHIFT_BLINK_RPM) {
		/* Limiar de troca: todas as 15 piscam em azul (~6 Hz) */
		bool blue_on = ((now_ms / 80U) % 2U) == 0;

		for (int i = 0; i < PHYSICAL_LEDS_TOP; i++) {
			strip_pixels[i].r = 0;
			strip_pixels[i].g = 0;
			strip_pixels[i].b = blue_on ? 150 : 0;
		}
	} else {
		for (int i = 0; i < PHYSICAL_LEDS_TOP; i++) {
			bool on = (rpm >= s_shift_rpm_limits[i]);

			if (i < 5) {
				/* 5 Verdes */
				strip_pixels[i].r = 0;
				strip_pixels[i].g = on ? 130 : 0;
				strip_pixels[i].b = 0;
			} else if (i < 10) {
				/* 5 Vermelhos */
				strip_pixels[i].r = on ? 150 : 0;
				strip_pixels[i].g = 0;
				strip_pixels[i].b = 0;
			} else {
				/* 5 Roxos */
				strip_pixels[i].r = on ? 90 : 0;
				strip_pixels[i].g = 0;
				strip_pixels[i].b = on ? 140 : 0;
			}
		}
	}

	/* 2. LEDs Laterais 15..20 (Bandeira Verde de Pista Livre em Fade 0..100%..0 em 2s) */
	/* 15..17: Lateral Direita (baixo -> cima), 18..20: Lateral Esquerda (baixo -> cima) */
	uint32_t phase = now_ms % 2000U;
	uint32_t ramp = (phase < 1000U) ? phase : (2000U - phase); /* 0 a 1000 */
	/* Curva quadratica suave com brilho maximo calibrado (120 de 255) */
	uint8_t green_fade = (uint8_t)((ramp * ramp * 120ULL) / (1000ULL * 1000ULL));

	for (int i = PHYSICAL_LEDS_TOP; i < PHYSICAL_LEDS_TOTAL; i++) {
		strip_pixels[i].r = 0;
		strip_pixels[i].g = green_fade;
		strip_pixels[i].b = 0;
	}

	led_strip_update_rgb(strip_dev, strip_pixels, PHYSICAL_LEDS_TOTAL);
}

static void update_shift_leds(uint16_t rpm, uint32_t now_ms)
{
	bool shift_blink = (rpm >= SHIFT_BLINK_RPM);
	bool blue_on = ((now_ms / 60U) % 2U) == 0;

	for (int i = 0; i < SHIFT_LEDS_COUNT; i++) {
		if (shift_blink) {
			/* Limiar de troca de marcha: todas piscam em azul */
			lv_obj_set_style_bg_color(shift_leds[i],
				blue_on ? lv_color_hex(0x3B82F6) : lv_color_hex(0x0F172A),
				LV_PART_MAIN);
		} else {
			bool on = (rpm >= s_shift_rpm_limits[i]);

			if (i < 5) {
				/* LEDs Verdes */
				lv_obj_set_style_bg_color(shift_leds[i],
					on ? lv_color_hex(0x22C55E) : lv_color_hex(0x064E3B),
					LV_PART_MAIN);
			} else if (i < 10) {
				/* LEDs Vermelhos */
				lv_obj_set_style_bg_color(shift_leds[i],
					on ? lv_color_hex(0xEF4444) : lv_color_hex(0x450A0A),
					LV_PART_MAIN);
			} else {
				/* LEDs Roxos */
				lv_obj_set_style_bg_color(shift_leds[i],
					on ? lv_color_hex(0xC084FC) : lv_color_hex(0x2E1065),
					LV_PART_MAIN);
			}
		}
	}
}

static void update_dashboard_ui(const struct senna_telemetry_sample *sample, uint32_t now_ms)
{
	/* 1. Marcha */
	lv_label_set_text_fmt(gear_label, "%u", (unsigned int)sample->gear);

	/* 2. Velocidade e RPM */
	lv_label_set_text_fmt(speed_label, "%3u", (unsigned int)sample->speed_kmh);
	lv_label_set_text_fmt(rpm_label, "%5u", (unsigned int)sample->rpm);
	lv_bar_set_value(rpm_bar, (int32_t)sample->rpm, LV_ANIM_OFF);

	if (sample->rpm >= 13800) {
		lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0xEF4444), LV_PART_INDICATOR);
	} else if (sample->rpm >= 12000) {
		lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0xFACC15), LV_PART_INDICATOR);
	} else {
		lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0x0284C7), LV_PART_INDICATOR);
	}

	/* 3. Shift LEDs na tela */
	update_shift_leds(sample->rpm, now_ms);

	/* 4. Tempo de volta cronometrado */
	uint32_t t = sample->time_ms;
	uint32_t m = t / 60000U;
	uint32_t s = (t % 60000U) / 1000U;
	uint32_t ms = t % 1000U;

	lv_label_set_text_fmt(lap_time_label, "%02u:%02u.%03u",
			      (unsigned int)m, (unsigned int)s, (unsigned int)ms);

	/* 5. Nome da curva / trecho */
	lv_label_set_text(corner_label, sample->corner_name);

	/* 6. Atualizacao dos trechos e linhas do circuito */
	if (sample->time_ms >= SENNA_SECTOR1_TIME_MS) {
		lv_obj_set_hidden(track_s1_line, false);
		lv_obj_set_style_bg_color(s1_pill, lv_color_hex(0x7E22CE), LV_PART_MAIN);
		lv_obj_set_style_text_color(s1_pill, lv_color_hex(0xF3E8FF), LV_PART_MAIN);
	} else {
		lv_obj_set_hidden(track_s1_line, true);
		lv_obj_set_style_bg_color(s1_pill, lv_color_hex(0x064E3B), LV_PART_MAIN);
		lv_obj_set_style_text_color(s1_pill, lv_color_hex(0x22C55E), LV_PART_MAIN);
	}

	if (sample->time_ms >= SENNA_SECTOR2_TIME_MS) {
		lv_obj_set_hidden(track_s2_line, false);
		lv_obj_set_style_bg_color(s2_pill, lv_color_hex(0x7E22CE), LV_PART_MAIN);
		lv_obj_set_style_text_color(s2_pill, lv_color_hex(0xF3E8FF), LV_PART_MAIN);
	} else {
		lv_obj_set_hidden(track_s2_line, true);
		if (sample->time_ms >= SENNA_SECTOR1_TIME_MS) {
			lv_obj_set_style_bg_color(s2_pill, lv_color_hex(0x064E3B), LV_PART_MAIN);
			lv_obj_set_style_text_color(s2_pill, lv_color_hex(0x22C55E), LV_PART_MAIN);
		} else {
			lv_obj_set_style_bg_color(s2_pill, lv_color_hex(0x1E293B), LV_PART_MAIN);
			lv_obj_set_style_text_color(s2_pill, lv_color_hex(0x64748B), LV_PART_MAIN);
		}
	}

	if (sample->time_ms >= SENNA_SECTOR3_TIME_MS) {
		lv_obj_set_hidden(track_s3_line, false);
		lv_obj_set_hidden(track_progress_line, true);
		lv_obj_set_style_bg_color(s3_pill, lv_color_hex(0x7E22CE), LV_PART_MAIN);
		lv_obj_set_style_text_color(s3_pill, lv_color_hex(0xF3E8FF), LV_PART_MAIN);
	} else {
		lv_obj_set_hidden(track_s3_line, true);
		if (sample->time_ms >= SENNA_SECTOR2_TIME_MS) {
			lv_obj_set_style_bg_color(s3_pill, lv_color_hex(0x064E3B), LV_PART_MAIN);
			lv_obj_set_style_text_color(s3_pill, lv_color_hex(0x22C55E), LV_PART_MAIN);
		} else {
			lv_obj_set_style_bg_color(s3_pill, lv_color_hex(0x1E293B), LV_PART_MAIN);
			lv_obj_set_style_text_color(s3_pill, lv_color_hex(0x64748B), LV_PART_MAIN);
		}

		/* Linha ativa verde no setor corrente */
		uint8_t cur_idx = sample->track_idx;
		if (sample->sector == 1) {
			if (cur_idx >= 1) {
				lv_line_set_points(track_progress_line,
						   &s_interlagos_track[0],
						   cur_idx + 1);
				lv_obj_set_hidden(track_progress_line, false);
			} else {
				lv_obj_set_hidden(track_progress_line, true);
			}
		} else if (sample->sector == 2) {
			if (cur_idx >= INTERLAGOS_SECTOR1_END_IDX + 1) {
				lv_line_set_points(track_progress_line,
						   &s_interlagos_track[INTERLAGOS_SECTOR1_END_IDX],
						   cur_idx - INTERLAGOS_SECTOR1_END_IDX + 1);
				lv_obj_set_hidden(track_progress_line, false);
			} else {
				lv_obj_set_hidden(track_progress_line, true);
			}
		} else if (sample->sector == 3) {
			if (cur_idx >= INTERLAGOS_SECTOR2_END_IDX + 1) {
				lv_line_set_points(track_progress_line,
						   &s_interlagos_track[INTERLAGOS_SECTOR2_END_IDX],
						   cur_idx - INTERLAGOS_SECTOR2_END_IDX + 1);
				lv_obj_set_hidden(track_progress_line, false);
			} else {
				lv_obj_set_hidden(track_progress_line, true);
			}
		}
	}

	/* 7. Marcador do carro de Senna */
	uint8_t car_pt = sample->track_idx;
	if (car_pt >= INTERLAGOS_TRACK_POINTS_COUNT) {
		car_pt = INTERLAGOS_TRACK_POINTS_COUNT - 1;
	}
	lv_obj_set_pos(car_marker,
		       s_interlagos_track[car_pt].x - 5,
		       s_interlagos_track[car_pt].y - 5);
}

static void telemetry_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);

	if (!anim_running) {
		return;
	}

	uint32_t now = k_uptime_get_32();
	uint32_t elapsed = now - anim_start_uptime_ms;
	struct senna_telemetry_sample sample;

	if (elapsed >= SENNA_LAP_TOTAL_TIME_MS) {
		elapsed = SENNA_LAP_TOTAL_TIME_MS;
		anim_running = false;
		g_senna_telemetry_active = false;
		lv_timer_pause(telemetry_timer);
		lv_label_set_text(start_btn_label, "Reiniciar volta do Senna");
		lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x0284C7), LV_PART_MAIN);
		lv_obj_set_style_text_color(lap_time_label, lv_color_hex(0xA855F7), LV_PART_MAIN);
	}

	senna_telemetry_update(elapsed, &sample);
	update_dashboard_ui(&sample, now);
}

static void start_lap_btn_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (code == LV_EVENT_CLICKED) {
		anim_start_uptime_ms = k_uptime_get_32();
		anim_running = true;
		g_senna_telemetry_active = true;

		lv_label_set_text(start_btn_label, "Volta em andamento...");
		lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x15803D), LV_PART_MAIN);
		lv_obj_set_style_text_color(lap_time_label, lv_color_hex(0x22C55E), LV_PART_MAIN);

		/* Reseta trechos roxos para uma nova volta limpa */
		lv_obj_set_hidden(track_s1_line, true);
		lv_obj_set_hidden(track_s2_line, true);
		lv_obj_set_hidden(track_s3_line, true);
		lv_obj_set_hidden(track_progress_line, true);

		/* Reinicia timer de telemetria */
		lv_timer_resume(telemetry_timer);

		/* Dispara amostragem inicial */
		struct senna_telemetry_sample sample;
		senna_telemetry_update(0U, &sample);
		update_dashboard_ui(&sample, anim_start_uptime_ms);
	}
}

static void build_cockpit_ui(void)
{
	lv_obj_t *scr = lv_screen_active();

	lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0F1D), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

	/* ------------------------------------------------------------------ */
	/* TOP BAR: F1 LED Shift Lights (15 LEDs) + Identificacao             */
	/* ------------------------------------------------------------------ */
	lv_obj_t *car_tag = lv_label_create(scr);
	lv_label_set_text(car_tag, "WILLIAMS FW16");
	lv_obj_set_style_text_color(car_tag, lv_color_hex(0x64748B), LV_PART_MAIN);
	lv_obj_set_style_text_font(car_tag, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_pos(car_tag, 8, 4);

	lv_obj_t *driver_tag = lv_label_create(scr);
	lv_label_set_text(driver_tag, "SENNA #2");
	lv_obj_set_style_text_color(driver_tag, lv_color_hex(0xFACC15), LV_PART_MAIN);
	lv_obj_set_style_text_font(driver_tag, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_align(driver_tag, LV_ALIGN_TOP_RIGHT, -8, 4);

	/* 15 LEDs centralizados no topo */
	int led_w = 12;
	int led_h = 7;
	int led_gap = 3;
	int total_leds_w = (SHIFT_LEDS_COUNT * led_w) + ((SHIFT_LEDS_COUNT - 1) * led_gap);
	int leds_start_x = (480 - total_leds_w) / 2;

	for (int i = 0; i < SHIFT_LEDS_COUNT; i++) {
		shift_leds[i] = lv_obj_create(scr);
		lv_obj_set_size(shift_leds[i], led_w, led_h);
		lv_obj_set_pos(shift_leds[i], leds_start_x + (i * (led_w + led_gap)), 5);
		lv_obj_set_style_radius(shift_leds[i], 2, LV_PART_MAIN);
		lv_obj_set_style_border_width(shift_leds[i], 0, LV_PART_MAIN);
		lv_obj_set_style_pad_all(shift_leds[i], 0, LV_PART_MAIN);
		lv_obj_set_scrollbar_mode(shift_leds[i], LV_SCROLLBAR_MODE_OFF);
	}
	update_shift_leds(13950, k_uptime_get_32());

	/* ------------------------------------------------------------------ */
	/* PAINEL ESQUERDO (X=6, Y=18, W=214): Tracado de Interlagos          */
	/* ------------------------------------------------------------------ */
	track_box = lv_obj_create(scr);
	lv_obj_set_size(track_box, 214, 146);
	lv_obj_set_pos(track_box, 6, 18);
	lv_obj_set_style_bg_color(track_box, lv_color_hex(0x0F172A), LV_PART_MAIN);
	lv_obj_set_style_border_color(track_box, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_border_width(track_box, 1, LV_PART_MAIN);
	lv_obj_set_style_radius(track_box, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(track_box, 0, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(track_box, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t *circuit_tag = lv_label_create(track_box);
	lv_label_set_text(circuit_tag, "INTERLAGOS (4.309 km)");
	lv_obj_set_style_text_color(circuit_tag, lv_color_hex(0x64748B), LV_PART_MAIN);
	lv_obj_set_style_text_font(circuit_tag, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_pos(circuit_tag, 6, 4);

	/* Tracado completo em BRANCO (requisito: traçado do senna em branco) */
	track_bg_line = lv_line_create(track_box);
	lv_line_set_points(track_bg_line, s_interlagos_track, INTERLAGOS_TRACK_POINTS_COUNT);
	lv_obj_set_style_line_color(track_bg_line, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
	lv_obj_set_style_line_width(track_bg_line, 2, LV_PART_MAIN);
	lv_obj_set_style_line_rounded(track_bg_line, true, LV_PART_MAIN);

	/* Trecho 1 ROXO (ativado apos completar setor 1) */
	track_s1_line = lv_line_create(track_box);
	lv_line_set_points(track_s1_line, &s_interlagos_track[0],
			   INTERLAGOS_SECTOR1_END_IDX + 1);
	lv_obj_set_style_line_color(track_s1_line, lv_color_hex(0xA855F7), LV_PART_MAIN);
	lv_obj_set_style_line_width(track_s1_line, 4, LV_PART_MAIN);
	lv_obj_set_style_line_rounded(track_s1_line, true, LV_PART_MAIN);
	lv_obj_set_hidden(track_s1_line, true);

	/* Trecho 2 ROXO (ativado apos completar setor 2) */
	track_s2_line = lv_line_create(track_box);
	lv_line_set_points(track_s2_line, &s_interlagos_track[INTERLAGOS_SECTOR1_END_IDX],
			   INTERLAGOS_SECTOR2_END_IDX - INTERLAGOS_SECTOR1_END_IDX + 1);
	lv_obj_set_style_line_color(track_s2_line, lv_color_hex(0xA855F7), LV_PART_MAIN);
	lv_obj_set_style_line_width(track_s2_line, 4, LV_PART_MAIN);
	lv_obj_set_style_line_rounded(track_s2_line, true, LV_PART_MAIN);
	lv_obj_set_hidden(track_s2_line, true);

	/* Trecho 3 ROXO (ativado apos completar setor 3) */
	track_s3_line = lv_line_create(track_box);
	lv_line_set_points(track_s3_line, &s_interlagos_track[INTERLAGOS_SECTOR2_END_IDX],
			   INTERLAGOS_SECTOR3_END_IDX - INTERLAGOS_SECTOR2_END_IDX + 1);
	lv_obj_set_style_line_color(track_s3_line, lv_color_hex(0xA855F7), LV_PART_MAIN);
	lv_obj_set_style_line_width(track_s3_line, 4, LV_PART_MAIN);
	lv_obj_set_style_line_rounded(track_s3_line, true, LV_PART_MAIN);
	lv_obj_set_hidden(track_s3_line, true);

	/* Linha ativa em VERDE (requisito: vai ficando verde conforme o senna anda) */
	track_progress_line = lv_line_create(track_box);
	lv_obj_set_style_line_color(track_progress_line, lv_color_hex(0x22C55E), LV_PART_MAIN);
	lv_obj_set_style_line_width(track_progress_line, 4, LV_PART_MAIN);
	lv_obj_set_style_line_rounded(track_progress_line, true, LV_PART_MAIN);
	lv_obj_set_hidden(track_progress_line, true);

	/* Marcador do Carro do Senna (amarelo Senna com ponto central) */
	car_marker = lv_obj_create(track_box);
	lv_obj_set_size(car_marker, 10, 10);
	lv_obj_set_style_radius(car_marker, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_bg_color(car_marker, lv_color_hex(0xFACC15), LV_PART_MAIN);
	lv_obj_set_style_border_color(car_marker, lv_color_hex(0x000000), LV_PART_MAIN);
	lv_obj_set_style_border_width(car_marker, 1, LV_PART_MAIN);
	lv_obj_set_style_pad_all(car_marker, 0, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(car_marker, LV_SCROLLBAR_MODE_OFF);
	lv_obj_set_pos(car_marker, s_interlagos_track[0].x - 5, s_interlagos_track[0].y - 5);

	/* Nome do Trecho / Curva */
	corner_label = lv_label_create(scr);
	lv_label_set_text(corner_label, "Reta Principal");
	lv_obj_set_style_text_color(corner_label, lv_color_hex(0x38BDF8), LV_PART_MAIN);
	lv_obj_set_style_text_font(corner_label, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_pos(corner_label, 10, 168);

	/* Badges de Setores: S1 | S2 | S3 */
	s1_pill = lv_label_create(scr);
	lv_label_set_text(s1_pill, "S1: 18.8s");
	lv_obj_set_size(s1_pill, 68, 20);
	lv_obj_set_pos(s1_pill, 6, 188);
	lv_obj_set_style_bg_color(s1_pill, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s1_pill, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_text_color(s1_pill, lv_color_hex(0x64748B), LV_PART_MAIN);
	lv_obj_set_style_text_font(s1_pill, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_style_text_align(s1_pill, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_set_style_radius(s1_pill, 4, LV_PART_MAIN);
	lv_obj_set_style_pad_top(s1_pill, 2, LV_PART_MAIN);

	s2_pill = lv_label_create(scr);
	lv_label_set_text(s2_pill, "S2: 34.1s");
	lv_obj_set_size(s2_pill, 68, 20);
	lv_obj_set_pos(s2_pill, 79, 188);
	lv_obj_set_style_bg_color(s2_pill, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s2_pill, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_text_color(s2_pill, lv_color_hex(0x64748B), LV_PART_MAIN);
	lv_obj_set_style_text_font(s2_pill, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_style_text_align(s2_pill, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_set_style_radius(s2_pill, 4, LV_PART_MAIN);
	lv_obj_set_style_pad_top(s2_pill, 2, LV_PART_MAIN);

	s3_pill = lv_label_create(scr);
	lv_label_set_text(s3_pill, "S3: 22.9s");
	lv_obj_set_size(s3_pill, 68, 20);
	lv_obj_set_pos(s3_pill, 152, 188);
	lv_obj_set_style_bg_color(s3_pill, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s3_pill, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_text_color(s3_pill, lv_color_hex(0x64748B), LV_PART_MAIN);
	lv_obj_set_style_text_font(s3_pill, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_style_text_align(s3_pill, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
	lv_obj_set_style_radius(s3_pill, 4, LV_PART_MAIN);
	lv_obj_set_style_pad_top(s3_pill, 2, LV_PART_MAIN);

	/* Botao: Iniciar volta do Senna */
	start_btn = lv_button_create(scr);
	lv_obj_set_size(start_btn, 214, 46);
	lv_obj_set_pos(start_btn, 6, 216);
	lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x0284C7), LV_PART_MAIN);
	lv_obj_set_style_radius(start_btn, 6, LV_PART_MAIN);
	lv_obj_add_event_cb(start_btn, start_lap_btn_cb, LV_EVENT_ALL, NULL);

	start_btn_label = lv_label_create(start_btn);
	lv_label_set_text(start_btn_label, "Iniciar volta do Senna");
	lv_obj_set_style_text_font(start_btn_label, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(start_btn_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
	lv_obj_center(start_btn_label);

	/* ------------------------------------------------------------------ */
	/* PAINEL DIREITO: DASHBOARD F1 DE VOLANTE (X=226, W=248)             */
	/* ------------------------------------------------------------------ */

	/* Card da Marcha Atual (Gear) */
	lv_obj_t *gear_card = lv_obj_create(scr);
	lv_obj_set_size(gear_card, 78, 108);
	lv_obj_set_pos(gear_card, 226, 18);
	lv_obj_set_style_bg_color(gear_card, lv_color_hex(0x0B132B), LV_PART_MAIN);
	lv_obj_set_style_border_color(gear_card, lv_color_hex(0x0284C7), LV_PART_MAIN);
	lv_obj_set_style_border_width(gear_card, 2, LV_PART_MAIN);
	lv_obj_set_style_radius(gear_card, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(gear_card, 4, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(gear_card, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t *gear_tag = lv_label_create(gear_card);
	lv_label_set_text(gear_tag, "MARCHA");
	lv_obj_set_style_text_color(gear_tag, lv_color_hex(0x94A3B8), LV_PART_MAIN);
	lv_obj_set_style_text_font(gear_tag, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_align(gear_tag, LV_ALIGN_TOP_MID, 0, 0);

	gear_label = lv_label_create(gear_card);
	lv_label_set_text(gear_label, "6");
	lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_32, LV_PART_MAIN);
	lv_obj_set_style_text_color(gear_label, lv_color_hex(0xFACC15), LV_PART_MAIN);
	lv_obj_align(gear_label, LV_ALIGN_CENTER, 0, 8);

	/* Card de Velocidade e RPM Numerico */
	lv_obj_t *telemetry_card = lv_obj_create(scr);
	lv_obj_set_size(telemetry_card, 164, 108);
	lv_obj_set_pos(telemetry_card, 310, 18);
	lv_obj_set_style_bg_color(telemetry_card, lv_color_hex(0x0B132B), LV_PART_MAIN);
	lv_obj_set_style_border_color(telemetry_card, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_border_width(telemetry_card, 1, LV_PART_MAIN);
	lv_obj_set_style_radius(telemetry_card, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(telemetry_card, 6, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(telemetry_card, LV_SCROLLBAR_MODE_OFF);

	/* Velocidade */
	speed_label = lv_label_create(telemetry_card);
	lv_label_set_text(speed_label, "288");
	lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_32, LV_PART_MAIN);
	lv_obj_set_style_text_color(speed_label, lv_color_hex(0x38BDF8), LV_PART_MAIN);
	lv_obj_set_pos(speed_label, 4, 0);

	lv_obj_t *kmh_tag = lv_label_create(telemetry_card);
	lv_label_set_text(kmh_tag, "KM/H");
	lv_obj_set_style_text_font(kmh_tag, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_style_text_color(kmh_tag, lv_color_hex(0x64748B), LV_PART_MAIN);
	lv_obj_set_pos(kmh_tag, 92, 14);

	/* RPM Numerico */
	rpm_label = lv_label_create(telemetry_card);
	lv_label_set_text(rpm_label, "13950");
	lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(rpm_label, lv_color_hex(0xF8FAFC), LV_PART_MAIN);
	lv_obj_set_pos(rpm_label, 4, 44);

	lv_obj_t *rpm_tag = lv_label_create(telemetry_card);
	lv_label_set_text(rpm_tag, "RPM");
	lv_obj_set_style_text_font(rpm_tag, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_style_text_color(rpm_tag, lv_color_hex(0xEF4444), LV_PART_MAIN);
	lv_obj_set_pos(rpm_tag, 92, 50);

	/* Barra horizontal de RPM (8000 a 14500 RPM) */
	rpm_bar = lv_bar_create(telemetry_card);
	lv_obj_set_size(rpm_bar, 148, 8);
	lv_obj_set_pos(rpm_bar, 4, 76);
	lv_bar_set_range(rpm_bar, 8000, 14500);
	lv_bar_set_value(rpm_bar, 13950, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_bg_color(rpm_bar, lv_color_hex(0x0284C7), LV_PART_INDICATOR);

	/* Card de Cronometragem e Dados da Volta */
	lv_obj_t *timing_card = lv_obj_create(scr);
	lv_obj_set_size(timing_card, 248, 128);
	lv_obj_set_pos(timing_card, 226, 134);
	lv_obj_set_style_bg_color(timing_card, lv_color_hex(0x0F172A), LV_PART_MAIN);
	lv_obj_set_style_border_color(timing_card, lv_color_hex(0x1E293B), LV_PART_MAIN);
	lv_obj_set_style_border_width(timing_card, 1, LV_PART_MAIN);
	lv_obj_set_style_radius(timing_card, 6, LV_PART_MAIN);
	lv_obj_set_style_pad_all(timing_card, 8, LV_PART_MAIN);
	lv_obj_set_scrollbar_mode(timing_card, LV_SCROLLBAR_MODE_OFF);

	lv_obj_t *timing_tag = lv_label_create(timing_card);
	lv_label_set_text(timing_tag, "TEMPO DA VOLTA (F1 1994)");
	lv_obj_set_style_text_color(timing_tag, lv_color_hex(0x94A3B8), LV_PART_MAIN);
	lv_obj_set_style_text_font(timing_tag, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_align(timing_tag, LV_ALIGN_TOP_LEFT, 0, 0);

	lap_time_label = lv_label_create(timing_card);
	lv_label_set_text(lap_time_label, "00:00.000");
	lv_obj_set_style_text_font(lap_time_label, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_style_text_color(lap_time_label, lv_color_hex(0x22C55E), LV_PART_MAIN);
	lv_obj_align(lap_time_label, LV_ALIGN_TOP_LEFT, 0, 20);

	lv_obj_t *record_label = lv_label_create(timing_card);
	lv_label_set_text(record_label, "POLE 1994: 1'15\"962 (A. Senna)");
	lv_obj_set_style_text_font(record_label, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_style_text_color(record_label, lv_color_hex(0xFACC15), LV_PART_MAIN);
	lv_obj_align(record_label, LV_ALIGN_TOP_LEFT, 0, 56);

	lv_obj_t *engine_info = lv_label_create(timing_card);
	lv_label_set_text(engine_info, "Renault RS6 3.5L V10 | 790 HP\nCambio Transversal 6 Marchas");
	lv_obj_set_style_text_font(engine_info, &lv_font_montserrat_12, LV_PART_MAIN);
	lv_obj_set_style_text_color(engine_info, lv_color_hex(0x64748B), LV_PART_MAIN);
	lv_obj_align(engine_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);

	/* Cria o timer de telemetria (executa a cada 25 ms, ~40 FPS) */
	telemetry_timer = lv_timer_create(telemetry_timer_cb, 25, NULL);
	lv_timer_pause(telemetry_timer);

	/* Inicializa estado do display com amostra t=0 */
	struct senna_telemetry_sample sample0;
	senna_telemetry_sample_at(0U, &sample0);
	update_dashboard_ui(&sample0, k_uptime_get_32());
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

	if (strip_dev && device_is_ready(strip_dev)) {
		LOG_INF("Fita de LEDs WS2812D pronta (%d LEDs em GPIO17)!", PHYSICAL_LEDS_TOTAL);
	} else {
		LOG_WRN("Fita WS2812D nao pronta ou nao configurada!");
	}

	build_cockpit_ui();
	lv_timer_handler();
	display_blanking_off(display_dev);

	LOG_INF("Dashboard F1 Ayrton Senna pronto! RPM global: %u", g_senna_telemetry_rpm);

	while (1) {
		uint32_t sleep_ms = lv_timer_handler();
		uint32_t now = k_uptime_get_32();

		/* Atualiza efeito de breathing dos LEDs laterais e shift lights */
		update_physical_ws2812_leds((uint16_t)g_senna_telemetry_rpm, now);

		k_msleep(MIN(sleep_ms, 15U));
	}

	return 0;
}
