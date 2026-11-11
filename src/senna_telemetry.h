/*
 * Copyright (c) 2026 Gustavo
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SENNA_TELEMETRY_H_
#define SENNA_TELEMETRY_H_

#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Total lap duration: 1m15.962s (Williams FW16 Renault RS6 V10 - Pole Position GP Brasil 1994) */
#define SENNA_LAP_TOTAL_TIME_MS      75962U

/* Sector Split Times */
#define SENNA_SECTOR1_TIME_MS        18824U  /* S1: 18.824s */
#define SENNA_SECTOR2_TIME_MS        53004U  /* S2 split: 34.180s (acumulado: 53.004s) */
#define SENNA_SECTOR3_TIME_MS        75962U  /* S3 split: 22.958s (acumulado: 75.962s) */

#define INTERLAGOS_TRACK_POINTS_COUNT 121
#define INTERLAGOS_SECTOR1_END_IDX   36
#define INTERLAGOS_SECTOR2_END_IDX   84
#define INTERLAGOS_SECTOR3_END_IDX   120

#define SENNA_KEYFRAMES_COUNT        36

struct senna_telemetry_keyframe {
	uint32_t time_ms;
	uint16_t speed_kmh;
	uint16_t rpm;
	uint8_t  gear;
	uint8_t  track_idx;
	const char *corner_name;
};

struct senna_telemetry_sample {
	uint32_t time_ms;
	uint16_t speed_kmh;
	uint16_t rpm;
	uint8_t  gear;
	uint8_t  track_idx;
	uint8_t  sector; /* 1, 2, ou 3 */
	const char *corner_name;
};

/*
 * Variaveis de telemetria globais requisitadas:
 * Qualquer modulo externo pode ler g_senna_telemetry_rpm em tempo real.
 */
extern volatile uint32_t g_senna_telemetry_rpm;
extern volatile uint32_t g_senna_telemetry_speed_kmh;
extern volatile uint8_t  g_senna_telemetry_gear;
extern volatile uint32_t g_senna_telemetry_time_ms;
extern volatile bool     g_senna_telemetry_active;

/* Funcoes de acesso seguro */
uint32_t senna_telemetry_get_rpm(void);
uint32_t senna_telemetry_get_speed_kmh(void);
uint8_t  senna_telemetry_get_gear(void);
uint32_t senna_telemetry_get_time_ms(void);
bool     senna_telemetry_is_active(void);

/* Interpolacao e amostragem */
void senna_telemetry_sample_at(uint32_t time_ms, struct senna_telemetry_sample *out_sample);
void senna_telemetry_update(uint32_t time_ms, struct senna_telemetry_sample *out_sample);

/* Tracado de Interlagos */
extern const lv_point_precise_t s_interlagos_track[INTERLAGOS_TRACK_POINTS_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* SENNA_TELEMETRY_H_ */
