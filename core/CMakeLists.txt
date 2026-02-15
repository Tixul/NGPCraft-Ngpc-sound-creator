add_library(ngpc_sound_core STATIC
    src/core.cpp
    src/file.cpp
    src/instrument.cpp
    src/k1_stream.cpp
    src/midi.cpp
    src/polling_driver.cpp
    src/psg.cpp
    src/project.cpp
    src/sound_engine.cpp
    src/z80_machine.cpp
    third_party/z80/Z80.c
)

target_include_directories(ngpc_sound_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/z80
)

target_compile_features(ngpc_sound_core PUBLIC cxx_std_17)
