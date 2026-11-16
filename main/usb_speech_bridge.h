#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Owns USB and microphone reads until reset. WAV is played only on a PC command. */
esp_err_t usb_speech_bridge_run(const uint8_t *wav_pcm, size_t wav_pcm_size);
