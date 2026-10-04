idf_component_register(
    SRCS
        "main.c"
        "hardeware_driver/bsp_board.c"
        "speech_det_driver/mic_speech.c"

    INCLUDE_DIRS
        "."
        "hardeware_driver"
        "speech_det_driver"

    EMBED_FILES
        "assets/hiashita.wav"
)