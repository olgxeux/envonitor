#include "app.h"

#include "clock.h"
#include "adc1.h"
#include "aht20.h"
#include "bmp280.h"
#include "format.h"
#include "i2c1.h"
#include "ssd1306.h"
#include "time.h"
#include "usart1.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define COMMAND_BUFFER_CAPACITY 64U

#define ADC_SAMPLE_PERIOD_MS 50U
#define SENSOR_SAMPLE_PERIOD_MS 1000U
#define AHT20_MEASUREMENT_TIME_MS 80U
#define DISPLAY_UPDATE_PERIOD_MS 500U
#define LOG_PERIOD_MS 1000U

#define ADC_MAX_VALUE 4095U

#define PARAM_T 0U
#define PARAM_H 1U
#define PARAM_P 2U
#define PARAM_COUNT 3U

typedef struct
{
    char name;
    int32_t minimum;
    int32_t maximum;
} ParameterInfo;

typedef struct
{
    bool enabled;
    bool use_potentiometer;
    char comparison;
    int32_t threshold;
} Alarm;

typedef struct
{
    float aht_temperature;
    float bmp_temperature;
    float humidity;
    float pressure;

    uint16_t potentiometer_raw;

    bool aht_valid;
    bool bmp_valid;
    bool adc_pending;
    bool aht_pending;

    uint32_t aht_start_ms;
    uint32_t last_adc_sample_ms;
    uint32_t last_sensor_sample_ms;
    uint32_t last_display_update_ms;
    uint32_t last_log_ms;
} AppState;

static const ParameterInfo parameters[PARAM_COUNT] =
    {[PARAM_T] = {'T', -20, 60}, [PARAM_H] = {'H', 0, 100}, [PARAM_P] = {'P', 900, 1100}};

static Alarm alarms[PARAM_COUNT];
static AppState app_state;

static char command_buffer[COMMAND_BUFFER_CAPACITY];
static uint32_t command_length = 0U;
static bool command_overflow = false;

static int32_t parameter_index(char name)
{
    if (name == 't')
    {
        return PARAM_T;
    }

    if (name == 'h')
    {
        return PARAM_H;
    }

    if (name == 'p')
    {
        return PARAM_P;
    }

    return -1;
}

static bool parameter_value(uint32_t parameter, float *value)
{
    if (value == 0)
    {
        return false;
    }

    if (parameter == PARAM_T)
    {
        if (!app_state.aht_valid || !app_state.bmp_valid)
        {
            return false;
        }

        *value = (app_state.aht_temperature + app_state.bmp_temperature) / 2.0f;

        return true;
    }

    if (parameter == PARAM_H)
    {
        if (!app_state.aht_valid)
        {
            return false;
        }

        *value = app_state.humidity;

        return true;
    }

    if (parameter == PARAM_P)
    {
        if (!app_state.bmp_valid)
        {
            return false;
        }

        *value = app_state.pressure;

        return true;
    }

    return false;
}

static int32_t alarm_threshold(uint32_t parameter)
{
    Alarm *alarm = &alarms[parameter];

    if (!alarm->use_potentiometer)
    {
        return alarm->threshold;
    }

    const ParameterInfo *info = &parameters[parameter];
    uint32_t range = (uint32_t)(info->maximum - info->minimum);

    return info->minimum + (int32_t)(((uint32_t)app_state.potentiometer_raw * range) / ADC_MAX_VALUE);
}

static bool alarm_triggered(uint32_t parameter)
{
    Alarm *alarm = &alarms[parameter];

    if (!alarm->enabled)
    {
        return false;
    }

    float value;

    if (!parameter_value(parameter, &value))
    {
        return false;
    }

    int32_t threshold = alarm_threshold(parameter);

    if (alarm->comparison == '<')
    {
        return value < (float)threshold;
    }

    return value > (float)threshold;
}

static const char *system_state(void)
{
    if (!app_state.aht_valid || !app_state.bmp_valid)
    {
        return "ERR";
    }

    for (uint32_t i = 0U; i < PARAM_COUNT; ++i)
    {
        if (alarm_triggered(i))
        {
            return "ALERT";
        }
    }

    return "OK";
}

static bool potentiometer_used_by_other_alarm(uint32_t parameter)
{
    for (uint32_t i = 0U; i < PARAM_COUNT; ++i)
    {
        if (i != parameter && alarms[i].enabled && alarms[i].use_potentiometer)
        {
            return true;
        }
    }

    return false;
}

static bool parse_number(const char *text, int32_t *value)
{
    if (text == 0 || value == 0 || *text == '\0')
    {
        return false;
    }

    bool negative = false;

    if (*text == '-')
    {
        negative = true;
        ++text;
    }

    if (*text == '\0')
    {
        return false;
    }

    int64_t number = 0;

    while (*text != '\0')
    {
        if (*text < '0' || *text > '9')
        {
            return false;
        }

        number = number * 10 + (*text - '0');

        if (number > (int64_t)INT32_MAX + 1LL)
        {
            return false;
        }

        ++text;
    }

    if (!negative && number > INT32_MAX)
    {
        return false;
    }

    if (negative)
    {
        number = -number;
    }

    *value = (int32_t)number;

    return true;
}

static void process_command(char *command)
{
    char *parameter_text = strtok(command, " \t");
    char *operation = strtok(0, " \t");
    char *value_text = strtok(0, " \t");
    char *extra = strtok(0, " \t");

    if (parameter_text == 0 || operation == 0 || parameter_text[1] != '\0')
    {
        usart1_queue_string("ERR syntax\r\n");

        return;
    }

    int32_t parameter = parameter_index(parameter_text[0]);

    if (parameter < 0)
    {
        usart1_queue_string("ERR parameter\r\n");

        return;
    }

    uint32_t parameter_id = (uint32_t)parameter;

    Alarm *alarm = &alarms[parameter_id];

    if (strcmp(operation, "off") == 0)
    {
        if (value_text != 0 || extra != 0)
        {
            usart1_queue_string("ERR syntax\r\n");

            return;
        }

        alarm->enabled = false;
        alarm->use_potentiometer = false;

        usart1_queue_string("OK\r\n");

        return;
    }

    if (operation[1] != '\0' || (operation[0] != '<' && operation[0] != '>') || value_text == 0 || extra != 0)
    {
        usart1_queue_string("ERR syntax\r\n");

        return;
    }

    if (strcmp(value_text, "pot") == 0)
    {
        if (potentiometer_used_by_other_alarm(parameter_id))
        {
            usart1_queue_string("ERR pot used\r\n");

            return;
        }

        alarm->enabled = true;
        alarm->use_potentiometer = true;
        alarm->comparison = operation[0];

        usart1_queue_string("OK\r\n");

        return;
    }

    int32_t threshold;

    if (!parse_number(value_text, &threshold))
    {
        usart1_queue_string("ERR value\r\n");

        return;
    }

    const ParameterInfo *info = &parameters[parameter_id];

    if (threshold < info->minimum || threshold > info->maximum)
    {
        usart1_queue_string("ERR range\r\n");

        return;
    }

    alarm->enabled = true;
    alarm->use_potentiometer = false;
    alarm->comparison = operation[0];
    alarm->threshold = threshold;

    usart1_queue_string("OK\r\n");
}

static void process_uart(void)
{
    char value;

    while (usart1_try_read_char(&value))
    {
        if (value == '\r' || value == '\n')
        {
            if (command_overflow)
            {
                command_overflow = false;
                command_length = 0U;

                usart1_queue_string("ERR command too long\r\n");

                continue;
            }

            if (command_length == 0U)
            {
                continue;
            }

            command_buffer[command_length] = '\0';
            process_command(command_buffer);

            command_length = 0U;

            continue;
        }

        if (command_overflow)
        {
            continue;
        }

        if (value == '\b' || value == 0x7F)
        {
            if (command_length > 0U)
            {
                --command_length;
            }

            continue;
        }

        if (command_length >= COMMAND_BUFFER_CAPACITY - 1U)
        {
            command_overflow = true;
            command_length = 0U;

            continue;
        }

        command_buffer[command_length++] = value;
    }
}

static void process_adc(void)
{
    uint32_t now = time_get_ms();

    if (!app_state.adc_pending && now - app_state.last_adc_sample_ms >= ADC_SAMPLE_PERIOD_MS)
    {
        adc1_start_conversion();

        app_state.adc_pending = true;
        app_state.last_adc_sample_ms = now;
    }

    if (!app_state.adc_pending)
    {
        return;
    }

    uint16_t value;

    if (!adc1_try_read_result(&value))
    {
        return;
    }

    app_state.potentiometer_raw = value;
    app_state.adc_pending = false;
}

static void process_sensors(void)
{
    uint32_t now = time_get_ms();

    if (app_state.aht_pending && now - app_state.aht_start_ms >= AHT20_MEASUREMENT_TIME_MS)
    {
        float temperature;
        float humidity;

        app_state.aht_valid = aht20_read_measurement(&temperature, &humidity);

        if (app_state.aht_valid)
        {
            app_state.aht_temperature = temperature;
            app_state.humidity = humidity;
        }

        app_state.aht_pending = false;
    }

    if (now - app_state.last_sensor_sample_ms < SENSOR_SAMPLE_PERIOD_MS)
    {
        return;
    }

    app_state.last_sensor_sample_ms = now;

    if (aht20_start_measurement())
    {
        app_state.aht_pending = true;
        app_state.aht_start_ms = now;
    }
    else
    {
        app_state.aht_valid = false;
    }

    float temperature;
    float pressure;

    app_state.bmp_valid = bmp280_read(&temperature, &pressure);

    if (app_state.bmp_valid)
    {
        app_state.bmp_temperature = temperature;
        app_state.pressure = pressure;
    }
}

static void draw_parameter(uint32_t parameter, uint8_t y)
{
    const ParameterInfo *info = &parameters[parameter];

    char text[16];
    char name[] = {info->name, '\0'};

    ssd1306_draw_string(0U, y, name);

    float value;

    if (parameter_value(parameter, &value) && format_float_1dp(value, text, sizeof(text)))
    {
        ssd1306_draw_string(12U, y, text);
    }
    else
    {
        ssd1306_draw_string(12U, y, "ERR");
    }

    Alarm *alarm = &alarms[parameter];

    if (!alarm->enabled)
    {
        ssd1306_draw_string(60U, y, "OFF");
    }
    else
    {
        char comparison[] = {alarm->comparison, '\0'};

        ssd1306_draw_string(54U, y, comparison);

        if (format_int32(alarm_threshold(parameter), text, sizeof(text)))
        {
            ssd1306_draw_string(66U, y, text);
        }

        if (alarm->use_potentiometer)
        {
            ssd1306_draw_string(102U, y, "*");
        }
    }

    if (alarm_triggered(parameter))
    {
        ssd1306_draw_string(120U, y, "!");
    }
}

static void process_display(void)
{
    uint32_t now = time_get_ms();

    if (now - app_state.last_display_update_ms < DISPLAY_UPDATE_PERIOD_MS)
    {
        return;
    }

    app_state.last_display_update_ms = now;

    ssd1306_clear();

    draw_parameter(PARAM_T, 0U);
    draw_parameter(PARAM_H, 16U);
    draw_parameter(PARAM_P, 32U);

    ssd1306_draw_string(0U, 48U, "SYS ");
    ssd1306_draw_string(24U, 48U, system_state());

    if (!ssd1306_update())
    {
        usart1_queue_string("SSD1306 ERR\r\n");
    }
}

static void log_parameter(uint32_t parameter)
{
    char text[16];

    char name[] = {parameters[parameter].name, '=', '\0'};

    usart1_queue_string(name);

    float value;

    if (!parameter_value(parameter, &value))
    {
        usart1_queue_string("ERR");
        return;
    }

    if (format_float_1dp(value, text, sizeof(text)))
    {
        usart1_queue_string(text);
    }
}

static void process_log(void)
{
    uint32_t now = time_get_ms();
    if (now - app_state.last_log_ms < LOG_PERIOD_MS)
    {
        return;
    }

    app_state.last_log_ms = now;

    usart1_queue_string("DATA ");
    log_parameter(PARAM_T);
    usart1_queue_string(" ");
    log_parameter(PARAM_H);
    usart1_queue_string(" ");
    log_parameter(PARAM_P);

    usart1_queue_string(" STATE=");
    usart1_queue_string(system_state());
    usart1_queue_string("\r\n");
}

void app_init(void)
{
    clock_init();
    time_init();
    usart1_init();
    adc1_init();
    i2c1_init();

    usart1_queue_string("\r\nBOOT\r\n");
    usart1_queue_string(aht20_init() ? "AHT20 OK\r\n" : "AHT20 ERR\r\n");
    usart1_queue_string(bmp280_init() ? "BMP280 OK\r\n" : "BMP280 ERR\r\n");
    usart1_queue_string(ssd1306_init() ? "SSD1306 OK\r\n" : "SSD1306 ERR\r\n");

    uint32_t now = time_get_ms();

    app_state.last_adc_sample_ms = now - ADC_SAMPLE_PERIOD_MS;
    app_state.last_sensor_sample_ms = now - SENSOR_SAMPLE_PERIOD_MS;
    app_state.last_display_update_ms = now;
    app_state.last_log_ms = now;

    process_adc();
    process_sensors();

    usart1_queue_string("READY\r\n");
}

void app_process(void)
{
    process_uart();
    process_adc();
    process_sensors();
    process_display();
    process_log();
}
