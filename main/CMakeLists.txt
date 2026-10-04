idf_component_register(
    SRCS
        "main.c"
        "hardeware_driver/bsp_board.c"

    INCLUDE_DIRS
        "."
        "hardeware_driver"

    EMBED_FILES
        "assets/hiashita.wav"
)