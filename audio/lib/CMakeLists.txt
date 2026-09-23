add_library(audio STATIC
    src/audio_buffer.cpp
    src/wav_writer.cpp
    src/wav_reader.cpp
    src/osc.cpp
    src/oversample.cpp
)

target_include_directories(audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(audio PUBLIC cxx_std_17)
