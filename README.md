# esp32-chatgpt-speaker

## Development environment

- **OS:** Windows
- **Editor:** Visual Studio Code
- **Framework:** ESP-IDF 5.5.5
- **Target:** ESP32-S3

## ESP-IDF setup

1. Install the official **Espressif IDF** extension in VS Code.
2. Open the ESP-IDF Installation Manager from the VS Code command palette.
3. Use **Easy Installation** to install ESP-IDF and its toolchain.
4. Open the configured ESP-IDF PowerShell terminal.

Useful commands:

```powershell
idf.py build
idf.py flash
idf.py monitor
```

Configure the project:

```powershell
idf.py set-target esp32s3
```

Build:

```powershell
idf.py build
```

Connect the board over USB and flash:

```powershell
idf.py flash
```

Open the serial monitor:

```powershell
idf.py monitor
```

The first firmware test should print:

```text
Hello, Ashita!
```

## ESP-IDF version

The project initially used ESP-IDF 6.1, but the Waveshare audio code was incompatible with newer audio/I2S APIs.

Install and use **ESP-IDF 5.5.5**.

After switching versions:

```powershell
idf.py fullclean
idf.py set-target esp32s3
```

## Waveshare board support

Download the official **ESP32-S3-AUDIO-Board Examples** from:

https://docs.waveshare.com/ESP32-S3-AUDIO-Board/Resources-And-Documents

On the page:

```text
Examples
→ ESP32-S3-AUDIO-Board Examples
```

Extract the archive and keep the `ESP-IDF` examples available as a reference.

Copy:

```text
ESP-IDF/factory_01/main/hardeware_driver/bsp_board.c
ESP-IDF/factory_01/main/hardeware_driver/bsp_board.h
```

to:

```text
main/hardeware_driver/
```

These files configure the board's I2C, I2S, ES8311 playback codec, and ES7210 microphone codec.

## Audio playback

Record a test phrase in Audacity and export it as:

```text
16 kHz
16-bit PCM
Stereo
WAV
```

Store it at:

```text
main/assets/hiashita.wav
```

Add the codec dependency in:

```text
main/idf_component.yml
```

```yaml
dependencies:
  espressif/esp_codec_dev: "^1.6.2"
```

Update `main/CMakeLists.txt` to compile `bsp_board.c` and embed `hiashita.wav`.

Parse the WAV header at runtime to read:

```text
audio format
channel count
sample rate
bits per sample
```

Use these values when calling `esp_board_init()`.

Play the WAV in small chunks through `esp_audio_play()` to avoid large temporary allocations.

## Speech recognition

Copy:

```text
ESP-IDF/factory_01/main/speech_det_driver/mic_speech.c
ESP-IDF/factory_01/main/speech_det_driver/mic_speech.h
```

to:

```text
main/speech_det_driver/
```

Add ESP-SR to `main/idf_component.yml`:

```yaml
dependencies:
  espressif/esp_codec_dev: "^1.6.2"
  espressif/esp-sr: "^2.1.5"
```

The speech stack uses:

- ESP-SR AFE for microphone preprocessing
- WakeNet for wake-word detection
- MultiNet for offline command recognition

## Speech model partition

Create a root-level `partitions.csv`:

```csv
# Name,     Type, SubType, Offset,   Size, Flags
nvs,        data,   nvs,      0x9000,       0x6000,
factory,    0,      0,        0x10000,      3M,
flash_test, data,   fat,      ,             528K,
model,      data,   spiffs,   ,             5900K,
```

Enable it with:

```text
idf.py menuconfig
→ Partition Table
→ Custom partition table CSV
→ partitions.csv
```

Set the board flash size to 16 MB:

```text
idf.py menuconfig
→ Serial flasher config
→ Flash size
→ 16 MB
```

## Windows ESP-SR build fix

ESP-SR model packaging may fail on Windows because of the default `cp1252` console encoding.

Enable UTF-8 before building:

```powershell
$env:PYTHONUTF8="1"
```

Then rebuild:

```powershell
idf.py fullclean
idf.py build
```

## Planned voice trigger

```text
"Hi ESP"
    ↓
WakeNet
    ↓
"Hello Jarvis"
    ↓
MultiNet
    ↓
play hiashita.wav
```

A custom wake phrase can be added later once the basic speech-recognition pipeline is stable.