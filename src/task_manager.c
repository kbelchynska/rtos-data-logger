// Manager Task: серце системи.
//
// Один цикл = 1 секунда:
//   1. будить Sensor Task і чекає дані з sensor_queue
//   2. обробляє команди з command_queue і відповіді Flash Writer
//   3. шле знімок у display_queue і чекає, поки кадр піде на екран
//   4. раз на LOG_INTERVAL_S додає запис у RAM-буфер; повний буфер -> Flash Writer
//   5. засинає в Light Sleep до кінця секунди

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app_types.h"
#include "driver/uart.h"
#include "ds1307.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "rtos_objects.h"
#include "tasks.h"

static const char *TAG = "MANAGER";

// Єдиний RAM-буфер розміром рівно 1 сектор flash.
// Пишемо записи один за одним, хвіст лишається 0xFF (як стерта flash).
static uint8_t s_buf[FLASH_SECTOR_SIZE];
static uint16_t s_buf_count;
static bool s_buf_busy; // буфер віддано Flash Writer, поки не відповість, не чіпаємо

static bool s_logging_on = true;
static bool s_full;
static bool s_flash_ready;
static uint16_t s_sectors_used, s_sectors_total;
static uint32_t s_since_log_s = LOG_INTERVAL_S; // перший запис одразу після старту

static sensor_sample_t s_last;
static uint32_t s_wakeups, s_records_total;
static char s_msg[22];
static int s_msg_ttl; // скільки секунд ще показувати s_msg

static void set_msg(const char *m)
{
    strlcpy(s_msg, m, sizeof(s_msg));
    s_msg_ttl = 3;
}

static void buf_clear(void)
{
    memset(s_buf, 0xFF, sizeof(s_buf));
    s_buf_count = 0;
}

static void request_write(void)
{
    flash_request_t req = {.type = FLASH_REQ_WRITE, .buffer = s_buf, .count = s_buf_count};
    s_buf_busy = true;
    xQueueSend(g_flash_queue, &req, portMAX_DELAY);
}

static void announce_full(void)
{
    s_full = true;
    s_logging_on = false;
    printf("\n!!! LOG PARTITION FULL (%u/%u sectors): logging stopped. Use 'dump' to read, 'erase' to clear.\n\n",
           s_sectors_used, s_sectors_total);
}

static void append_record(const log_record_t *rec)
{
    if (s_buf_busy)
    {
        ESP_LOGW(TAG, "Buffer is being written, record skipped");
        return;
    }
    memcpy(&s_buf[s_buf_count * sizeof(log_record_t)], rec, sizeof(*rec));
    s_buf_count++;
    s_records_total++;
    ESP_LOGI(TAG, "Record %u/%u buffered", s_buf_count, (unsigned)RECORDS_PER_SECTOR);
    if (s_buf_count >= RECORDS_PER_SECTOR)
        request_write(); // пишемо лише повний буфер
}

static void print_status(void)
{
    time_t ts = s_last.rec.timestamp;
    struct tm t;
    gmtime_r(&ts, &t);
    printf("\n- status -\n");
    printf("time      : %04d-%02d-%02d %02d:%02d:%02d UTC%s\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
           t.tm_hour, t.tm_min, t.tm_sec, s_last.rtc_ok ? "" : " (RTC ERROR)");
    printf("sensor    : %.2f C, %.2f %%, %u mmHg%s\n", s_last.rec.temp_c_x100 / 100.0,
           s_last.rec.hum_pct_x100 / 100.0, s_last.rec.pres_mmhg, s_last.bme_ok ? "" : " (BME280 ERROR)");
    printf("logging   : %s%s, every %d s, next in %lu s\n", s_logging_on ? "ON" : "OFF", s_full ? " (FULL)" : "",
           LOG_INTERVAL_S, (unsigned long)(s_since_log_s >= LOG_INTERVAL_S ? 0 : LOG_INTERVAL_S - s_since_log_s));
    printf("RAM buffer: %u/%u records%s\n", s_buf_count, (unsigned)RECORDS_PER_SECTOR,
           s_buf_busy ? " (writing)" : "");
    printf("flash     : %u/%u sectors used (%u records max)%s\n", s_sectors_used, s_sectors_total,
           (unsigned)(s_sectors_total * RECORDS_PER_SECTOR), LOG_DEMO_MODE ? " [DEMO MODE]" : "");
    printf("records   : %lu logged since boot, %lu wakeups\n", (unsigned long)s_records_total,
           (unsigned long)s_wakeups);
    printf("\n\n");
}

static void print_help(void)
{
    printf("\nCommands:\n"
           "  status                      system status\n"
           "  log on | log off            enable / disable logging\n"
           "  flush                       write RAM buffer to flash now\n"
           "  erase                       erase log partition\n"
           "  dump                        print all logs as CSV\n"
           "  time YYYY-MM-DD HH:MM:SS    set DS1307 (UTC)\n"
           "Tip: press Enter first to wake the board, then type.\n\n");
}

static void set_time(const char *arg)
{
    struct tm t = {0};
    if (sscanf(arg, "%d-%d-%d %d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
    {
        printf("Format: time 2026-11-09 14:30:00\n");
        return;
    }
    t.tm_year -= 1900;
    t.tm_mon -= 1;
    time_t unix_time = ds1307_tm_to_unix(&t);

    i2c_lock();
    bool ok = ds1307_write(unix_time);
    i2c_unlock();
    printf("RTC set: %s\n", ok ? "OK" : "ERROR");
    set_msg(ok ? "TIME SET" : "TIME ERROR");
}

static void handle_command(const command_t *c)
{
    switch (c->type)
    {
    case CMD_HELP:
        print_help();
        break;
    case CMD_STATUS:
        print_status();
        break;
    case CMD_LOG_ON:
        if (s_full)
        {
            printf("Partition is full, run 'erase' first\n");
            break;
        }
        s_logging_on = true;
        printf("Logging ON\n");
        set_msg("LOG ON");
        break;
    case CMD_LOG_OFF:
        s_logging_on = false;
        printf("Logging OFF\n");
        set_msg("LOG OFF");
        break;
    case CMD_FLUSH:
        if (s_buf_busy)
            printf("Write already in progress\n");
        else if (s_buf_count == 0)
            printf("Buffer is empty, nothing to flush\n");
        else if (s_full)
            printf("Partition is full\n");
        else
        {
            printf("Flushing %u records (rest of sector stays empty)\n", s_buf_count);
            request_write();
        }
        break;
    case CMD_ERASE:
        if (s_buf_busy)
        {
            printf("Write in progress, try again\n");
            break;
        }
        buf_clear();
        s_buf_busy = true; // поки стираємо, у буфер не пишемо
        xQueueSend(g_flash_queue, &(flash_request_t){.type = FLASH_REQ_ERASE}, portMAX_DELAY);
        break;
    case CMD_DUMP:
        xQueueSend(g_flash_queue, &(flash_request_t){.type = FLASH_REQ_DUMP}, portMAX_DELAY);
        break;
    case CMD_TIME_SET:
        set_time(c->arg);
        break;
    }
}

static void handle_flash_result(const flash_result_t *r)
{
    s_sectors_used = r->sectors_used;
    s_sectors_total = r->sectors_total;

    switch (r->type)
    {
    case FLASH_RES_INIT:
        s_flash_ready = r->ok;
        if (!r->ok)
        {
            s_logging_on = false;
            printf("Log partition missing, logging disabled\n");
        }
        else if (r->full)
            announce_full();
        break;
    case FLASH_RES_WRITE:
        s_buf_busy = false;
        if (r->ok)
        {
            buf_clear();
            set_msg("SECTOR SAVED");
        }
        else
        {
            printf("Flash write failed, records kept in RAM\n");
        }
        if (r->full)
            announce_full();
        break;
    case FLASH_RES_ERASE:
        s_buf_busy = false;
        s_full = false;
        printf("Erase %s, logging %s\n", r->ok ? "done" : "FAILED", s_logging_on ? "ON" : "OFF");
        set_msg(r->ok ? "ERASED" : "ERASE ERROR");
        break;
    case FLASH_RES_DUMP:
        break;
    }
}

// Коли можна засинати: Flash Writer нічого не робить і людина не друкує в UART
static bool may_sleep(void)
{
    if (g_flash_busy || uxQueueMessagesWaiting(g_flash_queue) > 0)
        return false;
    if (esp_timer_get_time() - g_last_uart_activity_us < UART_AWAKE_WINDOW_S * 1000000LL)
        return false;
    return true;
}

static void sleep_until(int64_t wake_at_us)
{
    int64_t left = wake_at_us - esp_timer_get_time();
    if (left <= 0)
        return;

    if (!may_sleep())
    {
        vTaskDelay(pdMS_TO_TICKS(left / 1000));
        return;
    }

    // Дочекатися, поки консоль допише все в UART, інакше хвіст рядка загубиться
    fflush(stdout);
    uart_wait_tx_idle_polling(CMD_UART_PORT);

    esp_sleep_enable_timer_wakeup(left);
    esp_light_sleep_start();

    if (esp_sleep_get_wakeup_causes() & BIT(ESP_SLEEP_WAKEUP_UART))
    {
        g_last_uart_activity_us = esp_timer_get_time(); // далі не спимо, поки людина друкує
        printf("\n[woken by UART: awake for %d s, type a command]\n> ", UART_AWAKE_WINDOW_S);
        fflush(stdout);
    }
}

void manager_task(void *arg)
{
    buf_clear();
    print_help();

    while (1)
    {
        int64_t cycle_start = esp_timer_get_time();
        s_wakeups++;

        // 1. Сенсори
        xTaskNotifyGive(g_sensor_task);
        sensor_sample_t sample;
        if (xQueueReceive(g_sensor_queue, &sample, pdMS_TO_TICKS(500)) == pdTRUE)
            s_last = sample;

        // 2. Команди та відповіді Flash Writer (без очікування)
        command_t cmd;
        while (xQueueReceive(g_command_queue, &cmd, 0) == pdTRUE)
            handle_command(&cmd);
        flash_result_t fr;
        while (xQueueReceive(g_flash_result_queue, &fr, 0) == pdTRUE)
            handle_flash_result(&fr);

        // 3. Логування раз на LOG_INTERVAL_S
        if (s_since_log_s < LOG_INTERVAL_S)
            s_since_log_s++;
        if (s_logging_on && s_flash_ready && !s_full && s_since_log_s >= LOG_INTERVAL_S)
        {
            if (s_last.rtc_ok && s_last.bme_ok)
            {
                append_record(&s_last.rec);
                s_since_log_s = 0;
            }
            else
            {
                ESP_LOGW(TAG, "Sensor error, record skipped");
            }
        }

        // 4. Дисплей
        display_snapshot_t snap = {
            .last = s_last.rec,
            .rtc_ok = s_last.rtc_ok,
            .bme_ok = s_last.bme_ok,
            .logging_on = s_logging_on,
            .partition_full = s_full,
            .buffered = s_buf_count,
            .sectors_used = s_sectors_used,
            .sectors_total = s_sectors_total,
            .next_log_in_s = LOG_INTERVAL_S - s_since_log_s,
        };
        if (s_msg_ttl > 0)
        {
            strlcpy(snap.message, s_msg, sizeof(snap.message));
            s_msg_ttl;
        }
        xQueueOverwrite(g_display_queue, &snap);
        // Не засинати посеред передачі кадру по I2C
        xSemaphoreTake(g_display_done, pdMS_TO_TICKS(300));

        // 5. Light Sleep до кінця секунди
        sleep_until(cycle_start + WAKE_PERIOD_MS * 1000LL);
    }
}
