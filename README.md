# esp32-chatgpt-speaker

## Current test: Friday greetings using SpeechRecognition

The selected firmware is a USB microphone bridge, replacing the WakeNet path
described later in this document. It does not play the WAV at startup.
The ESP32 records four-second mono 16 kHz PCM clips; a Python helper on the PC
uses `SpeechRecognition.recognize_google()` with its default testing key.
When the transcript contains "hey friday", "hi friday", or "hello friday", the PC commands the ESP32
to play `main/assets/hiashita.wav`. The PC and USB connection must remain active.
This is a clip-based test with gaps during transcription/playback, not a
continuous local wake-word detector. Google recognition requires internet and
uploads recorded clips; its default testing access may fail or be revoked.
No Google Cloud project or PyAudio installation is required.

In PowerShell, from the project folder:

```powershell
python -m venv .stt-venv
& .\.stt-venv\Scripts\python.exe -m pip install -r tools/requirements-stt.txt

. C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1
Remove-Item 'Env:_IDF.PY_COMPLETE' -ErrorAction SilentlyContinue
$env:PYTHONUTF8 = "1"
& C:\Espressif\tools\python\v5.5.5\venv\Scripts\python.exe C:\esp\v5.5.5\esp-idf\tools\idf.py -p COM4 flash

& .\.stt-venv\Scripts\python.exe tools/hi_kuba.py --port COM4
```

Close `idf_monitor` first; only one program can own COM4. The helper resets the
board on connection. Say "Hey Friday", "Hi Friday", or "Hello Friday" when
the helper prints `LISTEN NOW`. Recognition results appear in that terminal.
Press Ctrl+C to stop. Test options:

Trigger matching ignores case and punctuation and requires complete words.
Use repeated `--phrase` options to override the defaults with custom triggers.

```powershell
# Capture locally without uploading; listen to recordings/latest.wav afterward.
& .\.stt-venv\Scripts\python.exe tools/hi_kuba.py --record-only
# Try the other physical microphone channel if input is silent or unusable.
& .\.stt-venv\Scripts\python.exe tools/hi_kuba.py --record-only --channel 3
# Verify the speaker without a recording or Google request.
& .\.stt-venv\Scripts\python.exe tools/hi_kuba.py --play-test
```

Recordings are saved locally to `recordings/latest.wav` (overwritten each clip,
excluded from Git). Firmware protocol responses and an audio checksum detect
failed microphone reads and corrupted USB transfers. Logs are suppressed after
bridge initialization so they cannot be mixed into binary audio.

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

The original firmware bring-up test printed (the current USB bridge does not):

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

The speaker amplifier also requires TCA9555 expander pin 8 (P10) HIGH.
`esp_board_init()` now configures that pin as an output through I2C address
`0x20`, preserves the other expander pins, and verifies the register values.
Look for `Speaker amplifier enabled` before startup playback. Codec writes
alone do not confirm that the speaker is audible.

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

## Previous WakeNet setup (reference)

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

## Previous planned voice trigger (not active)

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
