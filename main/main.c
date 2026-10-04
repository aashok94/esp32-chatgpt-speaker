#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hardeware_driver/bsp_board.h"


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

    size_t offset = 12;

    while (offset + 8 <= wav_size) {
        const uint8_t *chunk = wav_start + offset;
        uint32_t chunk_size = read_u32_le(chunk + 4);

        if (memcmp(chunk, "data", 4) == 0) {
            audio_data = chunk + 8;
            audio_size = chunk_size;
            break;
        }

        offset += 8 + chunk_size;

        /* WAV chunks are aligned to even byte boundaries. */
        if (chunk_size % 2 != 0) {
            offset++;
        }
    }

    if (audio_data == NULL) {
        ESP_LOGE(TAG, "No audio data found in WAV");
        return;
    }

    ESP_ERROR_CHECK(esp_board_init(16000, 1, 16));
    ESP_ERROR_CHECK(esp_audio_set_play_vol(60));

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