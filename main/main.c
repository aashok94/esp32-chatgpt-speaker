#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hardeware_driver/bsp_board.h"
#include "usb_speech_bridge.h"


static const char *TAG = "main";

/* These symbols are created by ESP-IDF because hiashita.wav is embedded. */
extern const uint8_t wav_start[] asm("_binary_hiashita_wav_start");
extern const uint8_t wav_end[]   asm("_binary_hiashita_wav_end");


/* Read a 32-bit little-endian integer from the WAV byte stream. */
static uint32_t read_u32_le(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}


/* Read a 16-bit little-endian integer from the WAV byte stream. */
static uint16_t read_u16_le(const uint8_t *data)
{
    return ((uint16_t)data[0]) |
           ((uint16_t)data[1] << 8);
}


void app_main(void)
{
    const size_t wav_size = wav_end - wav_start;

    ESP_LOGI(TAG, "Embedded WAV size: %u bytes", (unsigned)wav_size);

    /* Verify the basic RIFF/WAVE header. */
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

    /*
     * Walk through the WAV chunks.
     *
     * "fmt " describes the audio format.
     * "data" contains the actual PCM samples.
     */
    while (offset + 8 <= wav_size) {
        const uint8_t *chunk = wav_start + offset;
        uint32_t chunk_size = read_u32_le(chunk + 4);

        if (chunk_size > wav_size - offset - 8) {
            ESP_LOGE(TAG, "Truncated WAV chunk");
            return;
        }

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

        /* WAV chunks are padded to an even number of bytes. */
        if (chunk_size % 2 != 0) {
            offset++;
        }
    }

    if (audio_data == NULL || audio_size == 0 || audio_format != 1 ||
        sample_rate != 16000 || channels != 2 || bits_per_sample != 16 ||
        audio_size % 4 != 0) {
        ESP_LOGE(TAG, "Expected nonempty 16 kHz stereo 16-bit PCM WAV");
        return;
    }

    ESP_LOGI(TAG, "WAV format: %u", audio_format);
    ESP_LOGI(TAG, "Channels: %u", channels);
    ESP_LOGI(TAG, "Sample rate: %lu Hz", sample_rate);
    ESP_LOGI(TAG, "Bits per sample: %u", bits_per_sample);

    /*
     * Initialize the board audio hardware first.
     *
     * Use the format read from the WAV instead of duplicating
     * 16000 Hz / stereo / 16-bit as hard-coded values.
     */
    ESP_ERROR_CHECK(
        esp_board_init(
            sample_rate,
            channels,
            bits_per_sample
        )
    );

    ESP_ERROR_CHECK(esp_audio_set_play_vol(60));


    ESP_LOGI(TAG, "USB speech bridge ready; start tools/hi_kuba.py on the PC");
    ESP_ERROR_CHECK(usb_speech_bridge_run(audio_data, audio_size));
}
