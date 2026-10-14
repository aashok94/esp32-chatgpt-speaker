#include "mic_speech.h"

#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"

// MultiNet is intentionally disabled.
// We only need WakeNet ("Hi ESP") to start the future ChatGPT session.
// #include "esp_mn_iface.h"
// #include "esp_mn_models.h"

#include "model_path.h"

// Only required for loading MultiNet commands from menuconfig.
// #include "esp_process_sdkconfig.h"

#include "bsp_board.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

static const char *TAG = "App/Speech";

static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
srmodel_list_t *models = NULL;
static esp_sr_event_callback_t spench_callback = NULL;


void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;

    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_feed_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();

    assert(nch == feed_channel);

    int16_t *i2s_buff = heap_caps_malloc(
        audio_chunksize * sizeof(int16_t) * feed_channel,
        MALLOC_CAP_SPIRAM
    );

    assert(i2s_buff);

    esp_task_wdt_add(NULL);

    while (task_flag) {
        esp_get_feed_data(
            true,
            i2s_buff,
            audio_chunksize * sizeof(int16_t) * feed_channel
        );

        afe_handle->feed(afe_data, i2s_buff);
        esp_task_wdt_reset();
    }

    free(i2s_buff);
    vTaskDelete(NULL);
}


void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;

    /*
     * MultiNet command recognition is intentionally disabled.
     *
     * Previously the flow was:
     *   "Hi ESP" -> WakeNet -> MultiNet command recognition
     *
     * MultiNet caused the task watchdog to trigger during inference.
     * Our application does not need local command recognition anyway.
     *
     * New flow:
     *   "Hi ESP" -> WakeNet -> callback -> future ChatGPT session
     */

    /*
    char *mn_name = esp_srmodel_filter(
        models,
        ESP_MN_PREFIX,
        ESP_MN_ENGLISH
    );

    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);

    esp_mn_commands_update_from_sdkconfig(multinet, model_data);
    */

    esp_task_wdt_add(NULL);

    while (task_flag) {

        afe_fetch_result_t *res = afe_handle->fetch(afe_data);

        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "AFE fetch failed");
            break;
        }

        /*
         * Single-channel case:
         * WakeNet detection itself is enough to trigger the callback.
         */
        if (res->raw_data_channels == 1 &&
            res->wakeup_state == WAKENET_DETECTED) {

            esp_sr_evt_data_t evtdata = {0};
            evtdata.awaken_channel = 0;

            if (spench_callback != NULL) {
                spench_callback(
                    ESP_SR_EVT_AWAKEN,
                    evtdata,
                    NULL
                );
            }

            // MultiNet used to run after wake detection.
            // Since MultiNet is disabled, re-enable WakeNet immediately
            // so the device can listen for "Hi ESP" again.
            afe_handle->enable_wakenet(afe_data);
        }

        /*
         * Multi-microphone case:
         * wait until the AFE verifies which microphone/channel detected
         * the wake word before firing the callback.
         */
        else if (res->raw_data_channels > 1 &&
                 res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {

            esp_sr_evt_data_t evtdata = {0};
            evtdata.awaken_channel = res->trigger_channel_id;

            if (spench_callback != NULL) {
                spench_callback(
                    ESP_SR_EVT_AWAKEN,
                    evtdata,
                    NULL
                );
            }

            // No MultiNet stage anymore, so return directly to
            // listening for the next wake word.
            afe_handle->enable_wakenet(afe_data);
        }

        /*
         * MultiNet command detection intentionally disabled.
         *
         * This previously called:
         *
         *   multinet->detect(...)
         *   multinet->get_results(...)
         *
         * and generated ESP_SR_EVT_CMD / ESP_SR_EVT_CMD_TIMEOUT events.
         *
         * We do not need those events because speech after "Hi ESP"
         * will eventually be handled by ChatGPT instead.
         */

        esp_task_wdt_reset();
    }

    /*
     * MultiNet cleanup is no longer required because no MultiNet
     * model is created.
     *
     * multinet->destroy(model_data);
     */

    ESP_LOGI(TAG, "Detect task exiting");
    vTaskDelete(NULL);
}


void Speech_Init(void)
{
    models = esp_srmodel_init("model");

    afe_config_t *afe_config = afe_config_init(
        esp_get_input_format(),
        models,
        AFE_TYPE_SR,
        AFE_MODE_LOW_COST
    );

    afe_config->ns_init = false;
    afe_config->vad_init = false;

    afe_handle = esp_afe_handle_from_config(afe_config);

    esp_afe_sr_data_t *afe_data =
        afe_handle->create_from_config(afe_config);

    afe_config_free(afe_config);

    task_flag = 1;

    xTaskCreatePinnedToCore(
        &detect_Task,
        "detect",
        8 * 1024,
        (void *)afe_data,
        5,
        NULL,
        1
    );

    xTaskCreatePinnedToCore(
        &feed_Task,
        "feed",
        8 * 1024,
        (void *)afe_data,
        5,
        NULL,
        0
    );
}


esp_err_t Speech_register_callback(
    esp_sr_event_callback_t callback
)
{
    if (!callback) {
        return ESP_ERR_INVALID_ARG;
    }

    spench_callback = callback;
    return ESP_OK;
}