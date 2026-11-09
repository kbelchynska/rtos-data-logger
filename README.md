# RTOS Data Logger: модуль 6.8 (гілка `kbelchynska`)

ESP32-S3 + FreeRTOS. Пристрій прокидається щосекунди, читає BME280 і DS1307, оновлює SSD1306 і знову засинає в **Light Sleep**.
Раз на N хвилин додає бінарний запис у RAM-буфер розміром із сектор flash. Повний буфер записується в окремий розділ `logdata`.

```
 Sensor Task ──sensor_queue──►                ──display_queue──► Display Task ──► SSD1306
                              Manager Task ◄──flash_result_q───┐
 UART Task ───command_queue──► (Light Sleep) ──flash_queue─────► Flash Writer ──► partition "logdata"

 BME280 + DS1307 + SSD1306 на одній шині I2C  ──►  g_i2c_mutex
```

## Відповідність завданню

| Вимога                                                             | Де                                                                                 |
| ------------------------------------------------------------------ | ---------------------------------------------------------------------------------- |
| Sensor Task: BME280 + DS1307                                       | `task_sensor.c`                                                                    |
| UART Task: прийом команд                                           | `task_uart.c`                                                                      |
| Display Task                                                       | `task_display.c`                                                                   |
| Manager Task: логіка + sleep                                       | `task_manager.c`                                                                   |
| Flash Writer Task                                                  | `task_flash.c` + `log_storage.c`                                                   |
| Queue між задачами                                                 | 5 черг, `rtos_objects.c`                                                           |
| Mutex для I2C                                                      | `g_i2c_mutex`: Sensor, Display і команда `time` беруть його навколо роботи з шиною |
| Пробудження щосекунди, Light Sleep                                 | `sleep_until()` у `task_manager.c`                                                 |
| Запис раз на N хвилин через `#define`                              | `LOG_INTERVAL_MIN` в `app_config.h`                                                |
| Бінарний формат                                                    | `log_record_t`, 10 байт                                                            |
| Окремий partition `logdata`                                        | `partitions.csv`, 256 КБ = 64 сектори                                              |
| Один RAM-буфер = 1 сектор                                          | `s_buf[4096]` у `task_manager.c`                                                   |
| Писати лише повний буфер                                           | `append_record()` → `request_write()` при 409/409                                  |
| Без wear leveling і без метаданих                                  | `esp_partition_erase_range` + `esp_partition_write`, позиція шукається скануванням |
| Partition заповнився → стоп + UART + дисплей                       | `announce_full()` + банер `! LOG FULL !`                                           |
| Команди `status`, `log on`, `log off`, `flush`                     | `task_uart.c` → `handle_command()`                                                 |
| _Опційно:_ `erase`, читання логів (`dump`), модулі, кращий дисплей | є все                                                                              |

## Формат запису (10 байт, little-endian, без вирівнювання)

| Зсув | Тип        | Поле                                      |
| ---- | ---------- | ----------------------------------------- |
| 0    | `uint32_t` | timestamp: Unix-час (UTC) з DS1307        |
| 4    | `int16_t`  | температура, °C × 100 (`2345` = 23.45 °C) |
| 6    | `uint16_t` | вологість, % × 100 (`5678` = 56.78 %)     |
| 8    | `uint16_t` | тиск, мм рт. ст.                          |

В один сектор влазить 4096 / 10 = **409** записів. Останні 6 байт сектора лишаються `0xFF`.
Весь розділ вміщує 64 × 409 = 26 176 записів. При записі раз на хвилину це приблизно 18 діб.

### Як обходимось без метаданих

Стерта flash = `0xFF`, тому запис з `timestamp == 0xFFFFFFFF` вважається порожнім.
Сектори пишуться строго по черзі. При старті Flash Writer шукає перший сектор, у якого порожній перший запис: це і є позиція запису.
Якщо порожніх секторів нема, розділ повний. `flush` пише неповний буфер, і хвіст сектора лишається `0xFF`. `dump` такі записи пропускає.

## Один цикл (1 секунда)

1. Manager будить Sensor Task нотифікацією. Той бере м'ютекс I2C, читає DS1307 і BME280 і кладе запис у `sensor_queue`.
2. Manager без очікування забирає команди з `command_queue` і відповіді Flash Writer.
3. Якщо минуло `LOG_INTERVAL_S`, запис іде в RAM-буфер. Повний буфер (409) передається Flash Writer через `flash_queue`.
4. Знімок стану йде в `display_queue`. Display Task малює кадр у RAM, бере м'ютекс і відправляє кадр по I2C. Потім віддає `g_display_done`, щоб Manager не заснув посеред передачі.
5. `esp_light_sleep_start()` до кінця секунди.

Manager **не спить**, поки Flash Writer зайнятий (запис, стирання, `dump`) або поки людина друкує в UART.
Інші задачі на Manager не чекають, тому система не блокується.

## UART-команди (115200, порт UART0)

```
status                      стан системи
log on | log off            увімкнути / вимкнути логування
flush                       записати RAM-буфер у flash зараз
erase                       стерти розділ logdata
dump                        вивести всі записи як CSV
time YYYY-MM-DD HH:MM:SS    встановити час DS1307 (UTC)
help
```

> **Light Sleep і UART.** Поки чип спить, перші символи з UART витрачаються на пробудження.
> Тому спочатку натисніть **Enter**. Після цього система не спить 15 с (`UART_AWAKE_WINDOW_S`), і за цей час можна набрати команду.
> Монітор підключайте до роз'єму **UART/COM**, а не USB: USB-Serial-JTAG відключається в Light Sleep.

## Дисплей

```
14:30:05        09.11.26
23.45C             56.8%
P 754 mmHg
next log in 42s            ← або SECTOR SAVED / ERASED / LOG OFF
LOG ON  B12/409  S3/64     ← записів у буфері, використано секторів
```

Коли розділ заповнений, у нижній частині екрана з'являється інвертований банер `! LOG FULL !`.

## Підключення

| Модуль            | Адреса | ESP32-S3                 |
| ----------------- | ------ | ------------------------ |
| BME280            | 0x76   | SDA = GPIO8, SCL = GPIO9 |
| DS1307 (Tiny RTC) | 0x68   | та сама шина             |
| SSD1306 128×64    | 0x3C   | та сама шина             |

Піни й адреси задаються в `src/app_config.h`.

## Збірка (PlatformIO)

```bash
pio run -t upload -t monitor
```

У `platformio.ini` стоїть плата `esp32-s3-devkitc-1` з `board_upload.flash_size = 16MB`. Плату `esp32-s3-devkitc-1-n16r8v` з espressif32 7.x прибрали. PSRAM у проєкті не використовується.

### Демо-режим для відео

У `app_config.h` поставте `#define LOG_DEMO_MODE 1`. Тоді запис іде щосекунди і використовується лише 1 сектор.
Через 409 секунд (близько 7 хв) з'являться `SECTOR SAVED`, повідомлення `LOG PARTITION FULL` в UART і банер на дисплеї.
Далі `dump` → `erase` → `log on`.

## Файли `src/`

| Файл                                                                               | Що робить                                                       |
| ---------------------------------------------------------------------------------- | --------------------------------------------------------------- |
| `main.c`                                                                           | ініціалізація, створення задач                                  |
| `app_config.h`                                                                     | усі `#define`: інтервали, піни, стеки, пріоритети               |
| `app_types.h`                                                                      | `log_record_t` і повідомлення черг                              |
| `rtos_objects.c/h`                                                                 | черги, м'ютекс, `i2c_lock()`/`i2c_unlock()`                     |
| `task_*.c`                                                                         | п'ять задач                                                     |
| `log_storage.c/h`                                                                  | робота з розділом: пошук позиції, запис сектора, стирання, dump |
| `i2c_bus.c`, `bme280.c`, `bme280_compensate.c`, `ds1307.c`, `oled.c`, `u8g2_hal.c` | драйвери                                                        |

Загальний опис ідеї та UML-діаграми від викладача лежать у `doc/`.

## Демонстрація

<! TODO: посилання на відео >
_Відео буде додано._
