// UART Task: збирає рядок, розбирає команду, кладе в command_queue.

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "app_types.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "rtos_objects.h"
#include "tasks.h"

static const char *TAG = "UART";

void uart_cmd_init(void)
{
    // UART0 вже налаштований як консоль; ставимо драйвер, щоб приймати байти
    ESP_ERROR_CHECK(uart_driver_install(CMD_UART_PORT, 256, 0, 0, NULL, 0));

    // Пробудження з Light Sleep по UART: перші символи "з'їдаються" на пробудження,
    // тому далі система не спить UART_AWAKE_WINDOW_S секунд і команду можна набрати.
    ESP_ERROR_CHECK(uart_set_wakeup_threshold(CMD_UART_PORT, 3));
    ESP_ERROR_CHECK(esp_sleep_enable_uart_wakeup(CMD_UART_PORT));
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1]))
        s[n] = '\0';
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i]))
        i++;
    if (i)
        memmove(s, s + i, n - i + 1);
}

static bool parse(char *line, command_t *cmd)
{
    trim(line);
    for (char *p = line; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    memset(cmd, 0, sizeof(*cmd));

    if (!strcmp(line, "help") || !strcmp(line, "?"))
        cmd->type = CMD_HELP;
    else if (!strcmp(line, "status"))
        cmd->type = CMD_STATUS;
    else if (!strcmp(line, "log on"))
        cmd->type = CMD_LOG_ON;
    else if (!strcmp(line, "log off"))
        cmd->type = CMD_LOG_OFF;
    else if (!strcmp(line, "flush"))
        cmd->type = CMD_FLUSH;
    else if (!strcmp(line, "erase"))
        cmd->type = CMD_ERASE;
    else if (!strcmp(line, "dump") || !strcmp(line, "read"))
        cmd->type = CMD_DUMP;
    else if (!strncmp(line, "time ", 5))
    {
        cmd->type = CMD_TIME_SET;
        strlcpy(cmd->arg, line + 5, sizeof(cmd->arg));
    }
    else
    {
        return false;
    }
    return true;
}

void uart_task(void *arg)
{
    char line[64];
    size_t len = 0;

    while (1)
    {
        uint8_t ch;
        if (uart_read_bytes(CMD_UART_PORT, &ch, 1, portMAX_DELAY) != 1)
            continue;
        g_last_uart_activity_us = esp_timer_get_time();

        if (ch == '\r' || ch == '\n')
        {
            if (len == 0)
                continue;
            line[len] = '\0';
            len = 0;
            printf("\n");

            command_t cmd;
            if (parse(line, &cmd))
            {
                xQueueSend(g_command_queue, &cmd, pdMS_TO_TICKS(100));
            }
            else if (line[0])
            {
                ESP_LOGW(TAG, "Unknown command '%s', type 'help'", line);
            }
        }
        else if (ch == 0x08 || ch == 0x7F)
        { // backspace
            if (len)
            {
                len;
                printf("\b \b");
                fflush(stdout);
            }
        }
        else if (isprint(ch) && len < sizeof(line) - 1)
        {
            line[len++] = (char)ch;
            putchar(ch); // луна, щоб бачити, що друкуєш
            fflush(stdout);
        }
    }
}
