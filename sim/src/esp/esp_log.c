
// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include <FreeRTOS.h>
#include <semphr.h>

#define LOG_COLOR_HEAD      "\033[0;%dm"
#define LOG_BOLD_HEAD       "\033[1;%dm"
#define LOG_COLOR_END       "\033[0m"

static const uint32_t s_log_color[ESP_LOG_MAX] = {
    0,  //  ESP_LOG_NONE
    31, //  ESP_LOG_ERROR
    33, //  ESP_LOG_WARN
    34, //  ESP_LOG_INFO
    0,  //  ESP_LOG_DEBUG
    0,  //  ESP_LOG_VERBOSE
};

static const char s_log_prefix[ESP_LOG_MAX] = {
    'N', //  ESP_LOG_NONE
    'E', //  ESP_LOG_ERROR
    'W', //  ESP_LOG_WARN
    'I', //  ESP_LOG_INFO
    'D', //  ESP_LOG_DEBUG
    'V', //  ESP_LOG_VERBOSE
};

static SemaphoreHandle_t semaphore = NULL;

static int log_putchar(int c) {
	if (semaphore == NULL) {
		semaphore = xSemaphoreCreateMutex();
	}
	xSemaphoreTake(semaphore, portMAX_DELAY);
	fputc(c, stderr);
	xSemaphoreGive(semaphore);
	return 1;
}

static putchar_like_t s_putchar_func = &log_putchar;

static int esp_log_write_str(const char *s)
{
    int ret;

    do {
        ret = s_putchar_func(*s);
    } while (ret != EOF && *++s);

    return ret;
}

static uint32_t fake_Timer = 0;


uint32_t esp_log_timestamp() {
	return fake_Timer++;
}

/**
 * @brief Write message into the log
 */
void esp_log_write(esp_log_level_t level, const char *tag,  const char *fmt, ...)
{
    int ret;
    va_list va;
    char *pbuf;
    char prefix;

    static char buf[16];
    uint32_t color = level >= ESP_LOG_MAX ? 0 : s_log_color[level];

    if (color) {
        sprintf(buf, LOG_COLOR_HEAD, color);
        ret = esp_log_write_str(buf);
        if (ret == EOF)
            goto exit;
    }

    prefix = level >= ESP_LOG_MAX ? 'N' : s_log_prefix[level];
    ret = asprintf(&pbuf, "%c (%d) %s: ", prefix, esp_log_early_timestamp(), tag);
    if (ret < 0)
        goto out;
    ret = esp_log_write_str(pbuf);
    free(pbuf);
    if (ret == EOF)
        goto exit;

    va_start(va, fmt);
    ret = vasprintf(&pbuf, fmt, va);
    va_end(va);
    if (ret < 0)
        goto out;
    ret = esp_log_write_str(pbuf);
    free(pbuf);
    if (ret == EOF)
        goto exit;

out:
    if (color) {
        ret = esp_log_write_str(LOG_COLOR_END);
        if (ret == EOF)
            goto exit;
    }

    if (ret > 0)
        s_putchar_func('\n');

exit:
	;
}

/**
 * @brief Set function used to output log entries
 */
putchar_like_t esp_log_set_putchar(putchar_like_t func)
{
    putchar_like_t tmp;

    tmp = s_putchar_func;
    s_putchar_func = func;

    return tmp;
}
