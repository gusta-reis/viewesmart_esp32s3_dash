/*
 * Copyright (c) 2026 Gustavo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "senna_telemetry.h"

/* Variaveis globais de telemetria */
volatile uint32_t g_senna_telemetry_rpm = 13950U;
volatile uint32_t g_senna_telemetry_speed_kmh = 288U;
volatile uint8_t  g_senna_telemetry_gear = 6U;
volatile uint32_t g_senna_telemetry_time_ms = 0U;
volatile bool     g_senna_telemetry_active = false;

/*
 * Perfil de telemetria da Pole Position de Ayrton Senna em Interlagos (1994)
 * Williams FW16 Renault RS6 3.5L V10 - 6 marchas
 * Tempo Total: 1:15.962 (75.962 ms)
 */
static const struct senna_telemetry_keyframe s_senna_telemetry[SENNA_KEYFRAMES_COUNT] = {
	{    0U, 288, 13950, 6,   0, "Reta Principal"},
	{ 1800U, 295, 14350, 6,   5, "Reta Principal"},
	{ 2800U, 298, 14500, 6,   8, "Frenagem S do Senna"},
	{ 3600U, 265, 13200, 5,  10, "S do Senna (T1)"},
	{ 4400U, 185, 11800, 3,  12, "S do Senna (T1)"},
	{ 5200U, 125, 10600, 2,  14, "S do Senna (T1)"},
	{ 6500U, 132, 12200, 2,  16, "S do Senna (T2)"},
	{ 7800U, 168, 13800, 3,  18, "S do Senna (T2)"},
	{ 9200U, 195, 12900, 4,  20, "Curva do Sol"},
	{11500U, 232, 13900, 4,  24, "Curva do Sol"},
	{13500U, 258, 13400, 5,  27, "Reta Oposta"},
	{15500U, 282, 14100, 6,  31, "Reta Oposta"},
	{17500U, 294, 14450, 6,  34, "Reta Oposta"},
	{18824U, 288, 13600, 5,  36, "Descida do Lago"},
	{20400U, 182, 11900, 3,  38, "Descida do Lago (T4)"},
	{22200U, 148, 12400, 3,  42, "Descida do Lago (T5)"},
	{24500U, 198, 13600, 4,  46, "Reta Ferradura"},
	{27000U, 235, 14100, 4,  50, "Ferradura (T6)"},
	{29800U, 205, 12800, 4,  54, "Ferradura (T7)"},
	{32200U, 175, 12200, 3,  58, "Curva da Laranjinha"},
	{34800U, 162, 13400, 3,  62, "Laranjinha"},
	{37500U, 145, 11500, 2,  65, "Pinheirinho"},
	{39800U,  98, 10200, 2,  68, "Pinheirinho"},
	{42200U,  76,  9100, 2,  72, "Bico de Pato"},
	{44500U, 115, 13400, 2,  74, "Bico de Pato"},
	{46800U, 168, 13800, 3,  77, "Mergulho"},
	{49500U, 212, 14200, 4,  80, "Mergulho"},
	{51500U, 145, 11600, 3,  82, "Entrada da Juncao"},
	{53004U, 124, 12800, 3,  84, "Juncao"},
	{55500U, 165, 13800, 3,  87, "Subida dos Boxes"},
	{58500U, 208, 13900, 4,  93, "Subida dos Boxes"},
	{62000U, 242, 13500, 5,  99, "Curva do Cafe"},
	{66000U, 268, 14100, 6, 105, "Arquibancadas"},
	{70000U, 285, 14250, 6, 111, "Reta Principal"},
	{73500U, 295, 14420, 6, 116, "Reta Principal"},
	{75962U, 298, 14500, 6, 120, "Linha de Chegada"},
};

/* Coordenadas 2D de Interlagos perfeitamente escaladas para 214x146 (offset +10) */
const lv_point_precise_t s_interlagos_track[INTERLAGOS_TRACK_POINTS_COUNT] = {
	{150, 109}, /* 0 */
	{156, 108}, /* 1 */
	{162, 106}, /* 2 */
	{168, 104}, /* 3 */
	{175, 102}, /* 4 */
	{181, 101}, /* 5 */
	{187,  99}, /* 6 */
	{192,  96}, /* 7 */
	{196,  91}, /* 8 */
	{196,  85}, /* 9 */
	{198,  79}, /* 10 */
	{201,  73}, /* 11 */
	{202,  67}, /* 12 */
	{201,  60}, /* 13 */
	{198,  55}, /* 14 */
	{195,  49}, /* 15 */
	{190,  45}, /* 16 */
	{184,  42}, /* 17 */
	{178,  40}, /* 18 */
	{172,  39}, /* 19 */
	{166,  37}, /* 20 */
	{160,  36}, /* 21 */
	{153,  34}, /* 22 */
	{147,  33}, /* 23 */
	{141,  31}, /* 24 */
	{135,  30}, /* 25 */
	{128,  29}, /* 26 */
	{122,  27}, /* 27 */
	{116,  26}, /* 28 */
	{110,  24}, /* 29 */
	{103,  23}, /* 30 */
	{ 97,  21}, /* 31 */
	{ 91,  20}, /* 32 */
	{ 85,  18}, /* 33 */
	{ 79,  17}, /* 34 */
	{ 72,  16}, /* 35 */
	{ 67,  20}, /* 36 - Fim Setor 1 */
	{ 65,  26}, /* 37 */
	{ 64,  32}, /* 38 */
	{ 63,  38}, /* 39 */
	{ 64,  45}, /* 40 */
	{ 66,  50}, /* 41 */
	{ 70,  56}, /* 42 */
	{ 75,  59}, /* 43 */
	{ 80,  63}, /* 44 */
	{ 86,  67}, /* 45 */
	{ 91,  70}, /* 46 */
	{ 96,  74}, /* 47 */
	{102,  77}, /* 48 */
	{107,  81}, /* 49 */
	{112,  85}, /* 50 */
	{118,  88}, /* 51 */
	{123,  92}, /* 52 */
	{125,  98}, /* 53 */
	{125, 105}, /* 54 */
	{123, 111}, /* 55 */
	{120, 116}, /* 56 */
	{114, 119}, /* 57 */
	{108, 120}, /* 58 */
	{101, 122}, /* 59 */
	{ 95, 123}, /* 60 */
	{ 89, 121}, /* 61 */
	{ 89, 115}, /* 62 */
	{ 93, 110}, /* 63 */
	{ 94, 104}, /* 64 */
	{ 90,  99}, /* 65 */
	{ 84,  97}, /* 66 */
	{ 79,  99}, /* 67 */
	{ 75, 106}, /* 68 */
	{ 70, 110}, /* 69 */
	{ 65, 114}, /* 70 */
	{ 59, 116}, /* 71 */
	{ 52, 117}, /* 72 */
	{ 47, 115}, /* 73 */
	{ 48, 109}, /* 74 */
	{ 53, 105}, /* 75 */
	{ 57, 101}, /* 76 */
	{ 62,  96}, /* 77 */
	{ 65,  90}, /* 78 */
	{ 66,  84}, /* 79 */
	{ 64,  78}, /* 80 */
	{ 60,  73}, /* 81 */
	{ 55,  69}, /* 82 */
	{ 49,  66}, /* 83 */
	{ 44,  63}, /* 84 - Fim Setor 2 */
	{ 38,  60}, /* 85 */
	{ 33,  57}, /* 86 */
	{ 27,  55}, /* 87 */
	{ 23,  60}, /* 88 */
	{ 21,  66}, /* 89 */
	{ 19,  72}, /* 90 */
	{ 18,  79}, /* 91 */
	{ 19,  85}, /* 92 */
	{ 20,  91}, /* 93 */
	{ 22,  97}, /* 94 */
	{ 25, 103}, /* 95 */
	{ 28, 109}, /* 96 */
	{ 31, 114}, /* 97 */
	{ 36, 119}, /* 98 */
	{ 41, 122}, /* 99 */
	{ 48, 124}, /* 100 */
	{ 54, 125}, /* 101 */
	{ 60, 127}, /* 102 */
	{ 66, 128}, /* 103 */
	{ 72, 129}, /* 104 */
	{ 79, 131}, /* 105 */
	{ 85, 132}, /* 106 */
	{ 91, 133}, /* 107 */
	{ 98, 134}, /* 108 */
	{104, 134}, /* 109 */
	{110, 133}, /* 110 */
	{117, 131}, /* 111 */
	{123, 129}, /* 112 */
	{129, 128}, /* 113 */
	{135, 126}, /* 114 */
	{141, 124}, /* 115 */
	{147, 122}, /* 116 */
	{153, 121}, /* 117 */
	{159, 119}, /* 118 */
	{154, 114}, /* 119 */
	{150, 109}, /* 120 - Fim Setor 3 / Linha de Chegada */
};

uint32_t senna_telemetry_get_rpm(void)
{
	return g_senna_telemetry_rpm;
}

uint32_t senna_telemetry_get_speed_kmh(void)
{
	return g_senna_telemetry_speed_kmh;
}

uint8_t senna_telemetry_get_gear(void)
{
	return g_senna_telemetry_gear;
}

uint32_t senna_telemetry_get_time_ms(void)
{
	return g_senna_telemetry_time_ms;
}

bool senna_telemetry_is_active(void)
{
	return g_senna_telemetry_active;
}

void senna_telemetry_sample_at(uint32_t time_ms, struct senna_telemetry_sample *out_sample)
{
	if (out_sample == NULL) {
		return;
	}

	if (time_ms >= SENNA_LAP_TOTAL_TIME_MS) {
		const struct senna_telemetry_keyframe *last =
			&s_senna_telemetry[SENNA_KEYFRAMES_COUNT - 1];

		out_sample->time_ms = SENNA_LAP_TOTAL_TIME_MS;
		out_sample->speed_kmh = last->speed_kmh;
		out_sample->rpm = last->rpm;
		out_sample->gear = last->gear;
		out_sample->track_idx = last->track_idx;
		out_sample->sector = 3;
		out_sample->corner_name = last->corner_name;
		return;
	}

	int i = 0;
	for (i = 0; i < (SENNA_KEYFRAMES_COUNT - 1); i++) {
		if (time_ms < s_senna_telemetry[i + 1].time_ms) {
			break;
		}
	}

	const struct senna_telemetry_keyframe *k0 = &s_senna_telemetry[i];
	const struct senna_telemetry_keyframe *k1 = &s_senna_telemetry[i + 1];

	uint32_t dt = k1->time_ms - k0->time_ms;
	uint32_t elapsed = time_ms - k0->time_ms;

	out_sample->time_ms = time_ms;
	out_sample->speed_kmh = (uint16_t)((int32_t)k0->speed_kmh +
		((int32_t)(k1->speed_kmh - k0->speed_kmh) * (int32_t)elapsed) / (int32_t)dt);
	out_sample->rpm = (uint16_t)((int32_t)k0->rpm +
		((int32_t)(k1->rpm - k0->rpm) * (int32_t)elapsed) / (int32_t)dt);
	out_sample->track_idx = (uint8_t)((int32_t)k0->track_idx +
		((int32_t)(k1->track_idx - k0->track_idx) * (int32_t)elapsed) / (int32_t)dt);

	if (k1->gear != k0->gear) {
		if (k1->gear < k0->gear) {
			/* Reducao na frenagem */
			out_sample->gear = (elapsed * 3U >= dt) ? k1->gear : k0->gear;
		} else {
			/* Troca ascendente */
			out_sample->gear = (elapsed * 4U >= dt * 3U) ? k1->gear : k0->gear;
		}
	} else {
		out_sample->gear = k0->gear;
	}

	if (time_ms < SENNA_SECTOR1_TIME_MS) {
		out_sample->sector = 1;
	} else if (time_ms < SENNA_SECTOR2_TIME_MS) {
		out_sample->sector = 2;
	} else {
		out_sample->sector = 3;
	}

	out_sample->corner_name = (elapsed * 2U < dt) ? k0->corner_name : k1->corner_name;
}

void senna_telemetry_update(uint32_t time_ms, struct senna_telemetry_sample *out_sample)
{
	senna_telemetry_sample_at(time_ms, out_sample);

	g_senna_telemetry_rpm = out_sample->rpm;
	g_senna_telemetry_speed_kmh = out_sample->speed_kmh;
	g_senna_telemetry_gear = out_sample->gear;
	g_senna_telemetry_time_ms = out_sample->time_ms;
}
