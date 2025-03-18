/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <esp_matter.h>
#include "bsp/esp-bsp.h"

#include <app_priv.h>
#include <nvs_flash.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t g_light_endpoint_id;

// ========================================== NVS 键值 ==========================================
#define NVS_NAMESPACE "light_config"
#define BRIGHTNESS_KEY "brightness"
#define POWER_KEY "power"

void led_indicator_save_brightness(uint8_t brightness)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_u8(handle, BRIGHTNESS_KEY, brightness));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

void led_indicator_load_brightness(uint8_t &brightness)
{
    esp_err_t err;
    nvs_handle_t handle;
    brightness = DEFAULT_BRIGHTNESS;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "nvs_open failed");
        return;
    }

    uint8_t val = DEFAULT_BRIGHTNESS; // 默认值
    esp_err_t ret = nvs_get_u8(handle, BRIGHTNESS_KEY, &val);
    if (ret != ESP_OK)
    {
        ESP_LOGI("NVS", "No saved brightness, using default");
    }
    nvs_close(handle);
    brightness = val;
}

void led_indicator_save_power(bool power)
{
    nvs_handle_t handle;
    uint8_t val = static_cast<uint8_t>(power);
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_u8(handle, POWER_KEY, val));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

void led_indicator_load_power(bool &power)
{
    esp_err_t err;
    nvs_handle_t handle;
    power = DEFAULT_POWER;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "nvs_open failed");
        return;
    }

    uint8_t val = static_cast<uint8_t>(DEFAULT_POWER); // 默认值
    esp_err_t ret = nvs_get_u8(handle, BRIGHTNESS_KEY, &val);
    if (ret != ESP_OK)
    {
        ESP_LOGI("NVS", "No saved power, using default");
    }
    nvs_close(handle);
    power = static_cast<bool>(val);
}

//===================================================================================================

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(led_indicator_handle_t handle, esp_matter_attr_val_t *val)
{
#if CONFIG_BSP_LEDS_NUM > 0
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "---------------> LED set power: %d (%s) <---------------", val->val.b, val->val.b ? "ON" : "OFF");
    if (val->val.b)
    {
        err = led_indicator_start(handle, BSP_LED_ON);
    }
    else
    {
        err = led_indicator_start(handle, BSP_LED_OFF);
    }
    led_indicator_save_power(val->val.b);
    return err;
#else
    return ESP_OK;
#endif
}

static esp_err_t app_driver_light_set_brightness(led_indicator_handle_t handle, esp_matter_attr_val_t *val)
{
    uint8_t value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
#if CONFIG_BSP_LEDS_NUM > 0
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "---------------> LED set brightness: %d <---------------", value);
    err = led_indicator_set_brightness(handle, value);
    led_indicator_save_brightness(value); // 将亮度值保存到硬盘
    return err;
#else
    return ESP_OK;
#endif
}

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = g_light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    if (endpoint_id == g_light_endpoint_id)
    {
        led_indicator_handle_t handle = (led_indicator_handle_t)driver_handle;
        if (cluster_id == OnOff::Id)
        {
            if (attribute_id == OnOff::Attributes::OnOff::Id)
            {
                err = app_driver_light_set_power(handle, val);
            }
        }
        else if (cluster_id == LevelControl::Id)
        {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id)
            {
                err = app_driver_light_set_brightness(handle, val);
            }
        }
    }
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_indicator_handle_t handle = (led_indicator_handle_t)priv_data;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting brightness */
    attribute_t *attribute = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);

    /* Setting power */
    attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    return err;
}

app_driver_handle_t app_driver_light_init()
{
#if CONFIG_BSP_LEDS_NUM > 0
    /* Initialize led */
    led_indicator_handle_t leds[CONFIG_BSP_LEDS_NUM];
    ESP_ERROR_CHECK(bsp_led_indicator_create(leds, NULL, CONFIG_BSP_LEDS_NUM));
    led_indicator_set_hsv(leds[0], SET_HSV(DEFAULT_HUE, DEFAULT_SATURATION, DEFAULT_BRIGHTNESS));

    return (app_driver_handle_t)leds[0];
#else
    return NULL;
#endif
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    ESP_ERROR_CHECK(iot_button_register_cb(btns[0], BUTTON_PRESS_DOWN, app_driver_button_toggle_cb, NULL));

    return (app_driver_handle_t)btns[0];
}
