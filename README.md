# esp32-chatgpt-speaker

## Development environment

This project is being developed on:

- **OS:** Windows
- **Editor:** Visual Studio Code
- **Framework:** ESP-IDF 5.5.5
- **Target:** ESP32-S3

### ESP-IDF setup

1. Installed the official **Espressif IDF** extension in VS Code.

2. Opened the ESP-IDF Installation Manager from the VS Code command palette.

3. Used the **Easy Installation** option to install:
   - ESP-IDF
   - ESP32 compiler/toolchain
   - Python environment
   - CMake/Ninja build tools
   - flashing and serial-monitor utilities

4. ESP-IDF provides a configured PowerShell environment used for commands such as:

```powershell
idf.py build
idf.py flash
idf.py monitor
```

5. Configured the project for the ESP32-S3:

```powershell
idf.py set-target esp32s3
```

6. Built the first firmware:

```powershell
idf.py build
```

7. Connected the ESP32-S3 over USB. The board was detected on `COM4`.

8. Flashed the firmware:

```powershell
idf.py flash
```

9. Opened the serial monitor:

```powershell
idf.py monitor
```

10. The board booted successfully and executed `app_main()`:

```text
Hello, Ashita!
```

This confirmed that the ESP-IDF toolchain, build process, USB connection, flashing, and serial communication were working correctly.

### ESP-IDF version

Development initially started with ESP-IDF 6.1.

When the Waveshare board-support code was added, build errors appeared because the vendor audio/I2S code was written against the ESP-IDF 5.x APIs.

ESP-IDF **5.5.5** was therefore installed and selected for this project.

The project was then regenerated using:

```powershell
idf.py fullclean
idf.py set-target esp32s3
```

## Audio playback preparation

A short voice recording saying "Hi Ashita" was created in Audacity and exported as a WAV file.

The file is stored at:

```text
main/assets/hiashita.wav
```

The WAV file is embedded directly into the firmware through `main/CMakeLists.txt`.

### Waveshare board-support code

The official Waveshare example package for the **ESP32-S3-AUDIO-Board** was downloaded from:

**Waveshare documentation → ESP32-S3-AUDIO-Board → Resources → Examples → ESP32-S3-AUDIO-Board Examples**

Direct documentation page:

https://docs.waveshare.com/ESP32-S3-AUDIO-Board/Resources-And-Documents

On that page, download **ESP32-S3-AUDIO-Board Examples** under the **Examples** section.

The downloaded examples were extracted and the `ESP-IDF` folder was copied into the repository temporarily as a reference.

The complete Waveshare factory application is **not** being integrated into this project.

From:

```text
ESP-IDF/factory_01/main/hardeware_driver/
```

only the following low-level board-support files were copied:

```text
bsp_board.c
bsp_board.h
```

They were copied into:

```text
main/hardeware_driver/
```

The following Waveshare factory-demo components were intentionally **not** copied:

```text
audio_play_driver
speech_det_driver
button_driver
rgb_led_driver
tca9555_driver
```

These higher-level factory-demo components are not required for the current audio playback milestone.

The reused `bsp_board` code provides the low-level configuration needed for:

- I2C
- I2S
- ES8311 speaker/audio playback codec
- ES7210 microphone codec

### Audio codec dependency

The Espressif codec dependency was added in:

```text
main/idf_component.yml
```

with:

```yaml
dependencies:
  espressif/esp_codec_dev: "^1.6.2"
```

`main/CMakeLists.txt` was updated to:

- compile `bsp_board.c`
- expose the board-support headers
- embed `hiashita.wav` into the firmware

## Next milestone

Initialize the board audio hardware and play the embedded `hiashita.wav` recording through the speaker.