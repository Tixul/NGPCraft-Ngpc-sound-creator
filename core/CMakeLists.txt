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
)

target_include_directories(ngpc_sound_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# The Z80 core is header-only and reached only by z80_machine.cpp, which hides it
# behind a pimpl -- so it stays PRIVATE and out of every other translation unit.
target_include_directories(ngpc_sound_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ngpc_z80
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ngpc_apu
)

target_compile_features(ngpc_sound_core PUBLIC cxx_std_17)
