#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hardeware_driver/bsp_board.h"
#include "speech_det_driver/mic_speech.h"


static const char *TAG = "main";

/* These symbols are created by ESP-IDF because hiashita.wav is embedded. */
extern const uint8_t wav_start[] asm("_binary_hiashita_wav_start");
extern const uint8_t wav_end[]   asm("_binary_hiashita_wav_end");

static uint32_t read_u32_le(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return ((uint16_t)data[0]) |
           ((uint16_t)data[1] << 8);
}

static void speech_event_callback(
    esp_sr_rec_event_t event,
    esp_sr_evt_data_t evt_data,
    void *user_data
)
{
    if (event == ESP_SR_EVT_AWAKEN) {
        ESP_LOGI(TAG, "Wake word detected");
    }

    if (event == ESP_SR_EVT_CMD) {
        ESP_LOGI(TAG, "Command detected: %u", evt_data.sr_cmd);
    }
}

void app_main(void)
{
    const size_t wav_size = wav_end - wav_start;

    ESP_LOGI(TAG, "Embedded WAV size: %u bytes", (unsigned)wav_size);

    /* WAV starts with: RIFF .... WAVE */
    if (wav_size < 12 ||
        memcmp(wav_start, "RIFF", 4) != 0 ||
        memcmp(wav_start + 8, "WAVE", 4) != 0) {

        ESP_LOGE(TAG, "Invalid WAV file");
        return;
    }

    const uint8_t *audio_data = NULL;
    size_t audio_size = 0;

    uint16_t audio_format = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;

    size_t offset = 12;

    while (offset + 8 <= wav_size) {
        const uint8_t *chunk = wav_start + offset;
        uint32_t chunk_size = read_u32_le(chunk + 4);

        if (memcmp(chunk, "fmt ", 4) == 0 && chunk_size >= 16) {
            const uint8_t *fmt = chunk + 8;

            audio_format = read_u16_le(fmt);
            channels = read_u16_le(fmt + 2);
            sample_rate = read_u32_le(fmt + 4);
            bits_per_sample = read_u16_le(fmt + 14);
        }

        if (memcmp(chunk, "data", 4) == 0) {
            audio_data = chunk + 8;
            audio_size = chunk_size;
        }

        offset += 8 + chunk_size;

        if (chunk_size % 2 != 0) {
            offset++;
        }
    }

    if (audio_data == NULL) {
        ESP_LOGE(TAG, "No audio data found in WAV");
        return;
    }

    ESP_LOGI(TAG, "WAV format: %u", audio_format);
    ESP_LOGI(TAG, "Channels: %u", channels);
    ESP_LOGI(TAG, "Sample rate: %lu Hz", sample_rate);
    ESP_LOGI(TAG, "Bits per sample: %u", bits_per_sample);

    ESP_ERROR_CHECK(esp_board_init(16000, 2, 16));
    ESP_ERROR_CHECK(esp_audio_set_play_vol(60));

    ESP_ERROR_CHECK(Speech_register_callback(speech_event_callback));
    Speech_Init();

    ESP_LOGI(TAG, "Playing hiashita.wav");

    const size_t chunk_size = 2048;

    for (size_t offset = 0; offset < audio_size; offset += chunk_size) {
        size_t remaining = audio_size - offset;
        size_t current_size = remaining < chunk_size ? remaining : chunk_size;

        ESP_ERROR_CHECK(
            esp_audio_play(
                (const int16_t *)(audio_data + offset),
                current_size,
                portMAX_DELAY
            )
        );
    }

    ESP_LOGI(TAG, "Playback finished");
}