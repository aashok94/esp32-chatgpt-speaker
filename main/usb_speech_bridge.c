#include "usb_speech_bridge.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "hardeware_driver/bsp_board.h"

#define SAMPLE_RATE 16000
#define CLIP_SAMPLES (SAMPLE_RATE * 4)
#define BLOCK_SAMPLES 256
#define INPUT_CHANNELS 4

/* Suppress console logs while USB carries binary PCM. The PC receives explicit
 * protocol status instead; boot logs before the handshake are ignored. */
static int quiet_log(const char *format, va_list args)
{
    (void)format;
    (void)args;
    return 0;
}

static esp_err_t send_bytes(const void *data, size_t length)
{
    const uint8_t *cursor = data;
    while (length) {
        /* The driver queues each write as one ring-buffer item, so a complete
         * clip cannot be submitted in one call to a 4096-byte TX buffer. */
        size_t block = length > 1024 ? 1024 : length;
        int sent = usb_serial_jtag_write_bytes(cursor, block, pdMS_TO_TICKS(2000));
        if (sent <= 0) {
            return ESP_ERR_TIMEOUT;
        }
        cursor += sent;
        length -= sent;
    }
    return ESP_OK;
}

static esp_err_t send_status(const char *text)
{
    return send_bytes(text, strlen(text));
}

static uint32_t pcm_checksum(const uint8_t *data, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ data[i]) * 16777619u;
    }
    return hash;
}

static esp_err_t record_clip(int16_t *raw, int16_t *mono, unsigned mic_channel)
{
    /* Discard samples accumulated while the PC was transcribing or playing. */
    for (int i = 0; i < 8; ++i) {
        esp_err_t ret = esp_get_feed_data(true, raw,
                                        BLOCK_SAMPLES * INPUT_CHANNELS * sizeof(int16_t));
        if (ret != ESP_OK) {
            return ret;
        }
    }
    esp_err_t ret = send_status("KUBA LISTEN 4\n");
    if (ret != ESP_OK) {
        return ret;
    }
    for (size_t offset = 0; offset < CLIP_SAMPLES; offset += BLOCK_SAMPLES) {
        ret = esp_get_feed_data(true, raw,
                               BLOCK_SAMPLES * INPUT_CHANNELS * sizeof(int16_t));
        if (ret != ESP_OK) {
            return ret;
        }
        for (size_t i = 0; i < BLOCK_SAMPLES; ++i) {
            mono[offset + i] = raw[i * INPUT_CHANNELS + mic_channel];
        }
    }
    return ESP_OK;
}

esp_err_t usb_speech_bridge_run(const uint8_t *wav_pcm, size_t wav_pcm_size)
{
    if (!wav_pcm || !wav_pcm_size || esp_get_feed_channel() != INPUT_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }
    int16_t *mono = heap_caps_malloc(CLIP_SAMPLES * sizeof(int16_t),
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int16_t *raw = malloc(BLOCK_SAMPLES * INPUT_CHANNELS * sizeof(int16_t));
    if (!mono || !raw) {
        free(mono);
        free(raw);
        return ESP_ERR_NO_MEM;
    }

    vprintf_like_t previous_log = esp_log_set_vprintf(quiet_log);
    usb_serial_jtag_driver_config_t config = {
        .tx_buffer_size = 4096,
        .rx_buffer_size = 256,
    };
    esp_err_t ret = usb_serial_jtag_driver_install(&config);
    if (ret != ESP_OK) {
        esp_log_set_vprintf(previous_log);
        free(mono);
        free(raw);
        return ret;
    }

    unsigned mic_channel = 1; /* RMNM: microphones occupy channels 1 and 3. */
    while (true) {
        uint8_t command;
        if (usb_serial_jtag_read_bytes(&command, 1, pdMS_TO_TICKS(100)) != 1) {
            continue;
        }
        if (command == '?') {
            ret = send_status("KUBA READY 1 16000 4\n");
        } else if (command == '1' || command == '3') {
            mic_channel = command - '0';
            ret = send_status("KUBA CHANNEL OK\n");
        } else if (command == 'R') {
            ret = record_clip(raw, mono, mic_channel);
            if (ret == ESP_OK) {
                char header[80];
                size_t bytes = CLIP_SAMPLES * sizeof(int16_t);
                snprintf(header, sizeof(header), "KUBA PCM %u %08lx\n",
                         (unsigned)bytes,
                         (unsigned long)pcm_checksum((const uint8_t *)mono, bytes));
                ret = send_status(header);
                if (ret == ESP_OK) {
                    ret = send_bytes(mono, bytes);
                }
            }
            if (ret != ESP_OK) {
                send_status("KUBA ERROR capture-or-transfer\n");
            }
        } else if (command == 'P') {
            ret = ESP_OK;
            for (size_t offset = 0; offset < wav_pcm_size && ret == ESP_OK; offset += 2048) {
                size_t bytes = wav_pcm_size - offset;
                if (bytes > 2048) {
                    bytes = 2048;
                }
                ret = esp_audio_play((const int16_t *)(wav_pcm + offset), bytes,
                                     pdMS_TO_TICKS(2000));
            }
            /* Codec writes enqueue DMA data; allow the final short block to drain. */
            vTaskDelay(pdMS_TO_TICKS(100));
            ret = send_status(ret == ESP_OK ? "KUBA PLAYED\n" : "KUBA ERROR playback\n");
        }
        /* A disconnected PC must not terminate the microphone service. */
        if (ret != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
