import unittest

from hi_kuba import CLIP_BYTES, checksum, matches_phrase, matches_trigger, read_pcm


class FakePort:
    def __init__(self, header, pcm):
        self.header = header
        self.pcm = pcm

    def read_until(self, *args, **kwargs):
        return self.header

    def read(self, length):
        # Deliberately fragmented USB reads.
        result, self.pcm = self.pcm[:701], self.pcm[701:]
        return result


class BridgeTests(unittest.TestCase):
    def test_all_friday_greetings(self):
        for text in ("Hey Friday", "HI, FRIDAY!", "Hello Friday", "Well, hey Friday please"):
            with self.subTest(text=text):
                self.assertTrue(matches_trigger(text))

    def test_reject_unrequested_greetings_and_partial_words(self):
        for text in ("hi kuba", "hi Cuba", "hi Google", "Friday", "hi Fridays", "Friday hello"):
            with self.subTest(text=text):
                self.assertFalse(matches_trigger(text))

    def test_custom_trigger_override(self):
        self.assertTrue(matches_trigger("start music", ("start music",)))
        self.assertFalse(matches_trigger("hi friday", ("start music",)))

    def test_phrase_with_case_punctuation_and_context(self):
        self.assertTrue(matches_phrase("Oh, HI Kuba!", "hi kuba"))

    def test_accept_google_cuba_spelling(self):
        self.assertTrue(matches_phrase("hi Cuba", "hi kuba"))
        self.assertTrue(matches_phrase("Hi Kuba!", "hi cuba"))
        self.assertFalse(matches_phrase("hi cuban", "hi kuba"))

    def test_reject_partial_word_and_reversed_phrase(self):
        self.assertFalse(matches_phrase("hi kuban", "hi kuba"))
        self.assertFalse(matches_phrase("kuba hi", "hi kuba"))
        self.assertFalse(matches_phrase("hi kuba", ""))

    def test_fnv_known_vector(self):
        self.assertEqual(checksum(b"hello"), 0x4F9F2CAB)

    def test_fragmented_audio(self):
        pcm = bytes(range(256)) * (CLIP_BYTES // 256)
        header = f"KUBA PCM {len(pcm)} {checksum(pcm):08x}\n".encode()
        self.assertEqual(read_pcm(FakePort(header, pcm)), pcm)

    def test_corrupt_audio_is_rejected(self):
        pcm = bytes(CLIP_BYTES)
        header = f"KUBA PCM {CLIP_BYTES} {checksum(pcm) ^ 1:08x}\n".encode()
        with self.assertRaisesRegex(RuntimeError, "corrupt"):
            read_pcm(FakePort(header, pcm))

    def test_untrusted_length_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "length"):
            read_pcm(FakePort(b"KUBA PCM 999999999 00000000\n", b""))


if __name__ == "__main__":
    unittest.main()
