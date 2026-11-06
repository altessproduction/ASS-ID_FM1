/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Javier (Animatek) */
/* ATEK303 on fm1-base: the app's half of the contract with the platform (fm1-base's template, adapted). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "ATEK303"
#define APP_SPLASH_RGB RGB(44, 127, 255)       /* Animatek blue */
#define APP_BLOCK 256u                         /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                              /* velocity from the keys: a hard press is an accent */
#define APP_SPLASH                             /* our own first screen: the acid smiley (app_splash) */
void app_splash(void);

/* the store: the app's settings, the panel calibration (the platform's), and their flash offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
