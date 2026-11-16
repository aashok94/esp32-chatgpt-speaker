"""ESP32 microphone -> USB -> recognize_google() -> Friday greeting -> ESP32 WAV.

No PC microphone, PyAudio, Google Cloud project, or private API key is used.
Only the four-second clips requested by this program are uploaded to Google.
"""

import argparse
import array
import re
import sys
import time
import wave
from pathlib import Path

import serial
import speech_recognition as sr

SAMPLE_RATE = 16000
CLIP_BYTES = SAMPLE_RATE * 4 * 2
DEFAULT_PHRASES = ("hey friday", "hi friday", "hello friday")


def checksum(data):
    result = 2166136261
    for byte in data:
        result = ((result ^ byte) * 16777619) & 0xFFFFFFFF
    return result


def normalize(text):
    # Google commonly transcribes the spoken name Kuba as Cuba.
    return ["kuba" if word == "cuba" else word
            for word in re.findall(r"\w+", text.casefold())]


def matches_phrase(text, phrase):
    words, target = normalize(text), normalize(phrase)
    return bool(target) and any(
        words[i : i + len(target)] == target
        for i in range(len(words) - len(target) + 1)
    )


def matches_trigger(text, phrases=DEFAULT_PHRASES):
    return any(matches_phrase(text, phrase) for phrase in phrases)


def read_status(port, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.read_until(b"\n", size=512)
        if not line.startswith(b"KUBA ") or not line.endswith(b"\n"):
            continue  # Ignore boot logs; never interpret them as audio.
        status = line.decode("ascii", errors="replace").strip()
        if status.startswith("KUBA ERROR"):
            raise RuntimeError(status)
        return status
    raise TimeoutError("No bridge response. Check the firmware and close idf_monitor.")


def read_pcm(port):
    header = read_status(port)
    parts = header.split()
    if len(parts) != 4 or parts[:2] != ["KUBA", "PCM"]:
        raise RuntimeError(f"Unexpected audio header: {header}")
    length = int(parts[2])
    expected_hash = int(parts[3], 16)
    if length != CLIP_BYTES:
        raise RuntimeError(f"Unexpected PCM length: {length}")
    data = bytearray()
    deadline = time.monotonic() + 15
    while len(data) < length and time.monotonic() < deadline:
        data.extend(port.read(length - len(data)))
    if len(data) != length or checksum(data) != expected_hash:
        raise RuntimeError("Incomplete or corrupt USB audio; recognition was skipped.")
    return bytes(data)


def save_wav(path, pcm):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--language", default="en-US")
    parser.add_argument("--phrase", action="append",
                        help="Override default triggers; repeat to accept multiple phrases")
    parser.add_argument("--channel", type=int, choices=(1, 3), default=1)
    parser.add_argument("--save", type=Path, default=Path("recordings/latest.wav"))
    parser.add_argument("--record-only", action="store_true", help="One local clip; no upload")
    parser.add_argument("--once", action="store_true", help="Recognize one clip and exit")
    parser.add_argument("--play-test", action="store_true", help="Play WAV without recording/uploading")
    args = parser.parse_args()
    phrases = args.phrase or DEFAULT_PHRASES
    if any(not normalize(phrase) for phrase in phrases):
        parser.error("--phrase must contain words")
    prompt = " / ".join(f'"{phrase}"' for phrase in phrases)

    recognizer = sr.Recognizer()
    recognizer.operation_timeout = 15
    with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=5) as port:
        # Match ESP-IDF's hard reset, keeping GPIO0 high. Reapply DTR after
        # changing RTS for Windows usbser.sys, as esp-idf-monitor does.
        port.dtr = False
        port.rts = True
        port.dtr = False
        time.sleep(0.1)
        port.rts = False
        port.dtr = False
        time.sleep(3)
        port.reset_input_buffer()
        port.write(b"?")
        status = read_status(port)
        if status != "KUBA READY 1 16000 4":
            raise RuntimeError(f"Unexpected firmware protocol: {status}")
        print("Connected to ESP32 speech bridge.", flush=True)
        if args.play_test:
            port.write(b"P")
            print(read_status(port), flush=True)
            return
        port.write(str(args.channel).encode("ascii"))
        if read_status(port) != "KUBA CHANNEL OK":
            raise RuntimeError("Microphone channel selection failed")
        if not args.record_only:
            print("Four-second clips will be sent to Google. Ctrl+C stops.", flush=True)

        while True:
            port.write(b"R")
            status = read_status(port)
            if status != "KUBA LISTEN 4":
                raise RuntimeError(f"Unexpected capture response: {status}")
            print(f"LISTEN NOW: say {prompt}.", flush=True)
            pcm = read_pcm(port)
            save_wav(args.save, pcm)
            samples = array.array("h", pcm)
            if sys.byteorder != "little":
                samples.byteswap()
            peak = max(abs(sample) for sample in samples)
            rms = (sum(sample * sample for sample in samples) / len(samples)) ** 0.5
            print(f"Mic {args.channel}: peak={peak}, RMS={rms:.1f}; saved {args.save}", flush=True)
            if args.record_only:
                return
            if peak == 0:
                print("Microphone is silent; no upload. Try --channel 3.", flush=True)
            else:
                try:
                    text = recognizer.recognize_google(sr.AudioData(pcm, SAMPLE_RATE, 2),
                                                       language=args.language,
                                                       endpoint="https://www.google.com/speech-api/v2/recognize")
                    print(f"Transcript: {text}", flush=True)
                    if matches_trigger(text, phrases):
                        port.write(b"P")
                        if read_status(port) != "KUBA PLAYED":
                            raise RuntimeError("No playback confirmation")
                        print("Phrase matched; ESP32 WAV played.", flush=True)
                except sr.UnknownValueError:
                    print("Google could not understand this clip.", flush=True)
                except sr.RequestError as error:
                    print(f"Google request failed: {error}", flush=True)
                    if not args.once:
                        time.sleep(5)
            if args.once:
                return


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")
    except (serial.SerialException, OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
