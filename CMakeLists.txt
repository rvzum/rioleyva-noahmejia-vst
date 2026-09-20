cmake_minimum_required(VERSION 3.22)

# Universal binary (Intel + Apple Silicon). Must be set before project().
if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS" FORCE)
endif()
if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment version" FORCE)
endif()

project(27wavVST VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ------------------------------------------------------------------
# Prefer a local checkout at ThirdParty/JUCE (e.g. downloaded by hand as a zip
# from GitHub) if present -- this avoids depending on `git clone` succeeding
# over the network, which can be unreliable on some connections. Falls back
# to fetching it automatically via git if no local copy exists.
set(JUCE_LOCAL_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/JUCE")

if (EXISTS "${JUCE_LOCAL_DIR}/CMakeLists.txt")
    message(STATUS "27wavVST: using local JUCE checkout at ${JUCE_LOCAL_DIR}")
    add_subdirectory(${JUCE_LOCAL_DIR} JUCE)
else()
    message(STATUS "27wavVST: no local JUCE checkout found at ${JUCE_LOCAL_DIR} -- fetching JUCE from GitHub (this can take a while on a slow connection)")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Plugin target -----------------------------------------------------------
# Originally declared IS_MIDI_EFFECT/no audio bus (a "pure" MIDI-only VST3
# category). FL Studio's plugin database only offers IS_SYNTH-category
# plugins in its "generator" picker (Channel Rack) -- a MIDI Effect-category
# plugin is only selectable as a Mixer/channel FX slot, and FL's Mixer never
# forwards note MIDI to FX plugins at all, so it could never actually
# receive notes there. IS_SYNTH TRUE + a declared stereo output bus is what
# makes FL Studio treat it as an instrument you can put on a channel --
# where it actually receives Piano Roll / live MIDI. That output bus is no
# longer silent: PluginProcessor now plays back a selectable oneshot sample
# (Source/Audio/SamplerEngine) through it -- see README.md's "Writing
# melodies" section and Source/Audio/README.md. See README.md for the FL
# Studio routing notes.
juce_add_plugin(27wavVST
    PRODUCT_NAME                "RIO LEYVA x NOAH MEJIA VST"
    COMPANY_NAME                "27wav"
    BUNDLE_ID                   "com.w27wav.rioleyva"
    PLUGIN_MANUFACTURER_CODE    Wv27
    PLUGIN_CODE                 Rlva
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    IS_MIDI_EFFECT              FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
)

target_sources(27wavVST PRIVATE
    Source/PluginProcessor/PluginProcessor.cpp
    Source/PluginEditor/PluginEditor.cpp
    Source/PluginEditor/PremiumLookAndFeel.cpp
    Source/MIDI/MidiEventParser.cpp
    Source/MIDI/MidiInputHandler.cpp
    Source/MIDI/NoteStorage.cpp
    Source/MIDI/MidiFilePattern.cpp
    Source/Visualization/PianoRollView.cpp
    Source/Effects/AnimatedGradientBackground.cpp
    Source/Effects/GifDecoder.cpp
    Source/Effects/GifBackground.cpp
    Source/Effects/FrameSequenceBackground.cpp
    Source/Effects/BackgroundManager.cpp
    Source/Audio/SamplerEngine.cpp
    Source/Audio/EffectsChain.cpp
    Source/Library/SampleLibraryManager.cpp
    Source/Composition/ComposedMelody.cpp
    Source/Composition/MelodySequencer.cpp
)

# --- Background media (GIF + MP4) -------------------------------------------
# Drop .gif or .mp4 files into Assets/Backgrounds/ and reconfigure/rebuild --
# each one ends up compiled into the plugin as binary data (no external file
# dependency at runtime, so the VST3 stays a single self-contained bundle)
# and shows up automatically in the in-plugin background switcher.
# See Assets/Backgrounds/README.md for full details and constraints.
file(GLOB W27_BACKGROUND_GIFS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Assets/Backgrounds/*.gif")
file(GLOB W27_BACKGROUND_MP4S CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Assets/Backgrounds/*.mp4")

set(W27_MP4_FRAME_FILES)

if (W27_BACKGROUND_MP4S)
    # .mp4 can't be played back directly (no video codec here -- see
    # GifDecoder.h's comment on why decoders aren't written from scratch for
    # anything beyond GIF's simple LZW scheme). Instead, ffmpeg extracts each
    # video to a JPEG frame sequence ONCE at configure time; those frames are
    # embedded exactly like GIF frames and played back the same way at runtime.
    find_program(W27_FFMPEG_EXECUTABLE ffmpeg)

    if (NOT W27_FFMPEG_EXECUTABLE)
        message(WARNING "27wavVST: ffmpeg not found -- .mp4 files in Assets/Backgrounds/ will be SKIPPED. Install it with 'brew install ffmpeg', then re-run cmake -B build to enable them.")
    else()
        foreach (mp4File IN LISTS W27_BACKGROUND_MP4S)
            get_filename_component(mp4Base "${mp4File}" NAME_WE)
            set(mp4OutDir "${CMAKE_BINARY_DIR}/mp4_frames/${mp4Base}")
            # Records the source .mp4's own last-write-time at the moment its
            # frames were last extracted -- lets us detect "the file at this
            # path changed" even when the filename stayed identical (e.g.
            # replacing Assets/Backgrounds/rioleyva.mp4 with a different
            # video but keeping the same name), which file(GLOB ...) alone
            # cannot: a glob only reacts to files being added/removed, never
            # to an existing match's content changing. Without this, a
            # same-name replacement would silently keep showing the OLD
            # video's already-cached frames forever.
            set(mp4MtimeMarker "${mp4OutDir}/.source_mtime")

            file(TIMESTAMP "${mp4File}" mp4CurrentMtime "%Y-%m-%dT%H:%M:%S" UTC)

            file(GLOB mp4ExistingFrames "${mp4OutDir}/*.jpg")
            list(LENGTH mp4ExistingFrames mp4ExistingFrameCount)

            set(mp4StoredMtime "")
            if (EXISTS "${mp4MtimeMarker}")
                file(READ "${mp4MtimeMarker}" mp4StoredMtime)
            endif()

            if (mp4ExistingFrameCount EQUAL 0 OR NOT mp4StoredMtime STREQUAL mp4CurrentMtime)
                if (mp4ExistingFrames)
                    file(REMOVE ${mp4ExistingFrames}) # clear stale frames from a previous (different) video first
                endif()
                file(MAKE_DIRECTORY "${mp4OutDir}")
                message(STATUS "27wavVST: extracting frames from ${mp4File} with ffmpeg (source is new or has changed since it was last extracted)")

                # 15fps, capped at 1080px wide, 15s max, near-lossless JPEG
                # quality (q:v 2) -- bumped up again from 720px/q:v 3 per
                # explicit user report that the background still looked
                # "terrible quality"/distorted (the scale filter's -2
                # preserves aspect ratio exactly -- no stretching -- so the
                # complaint was image quality/compression artefacts, not
                # distortion; the fix is a sharper source and less JPEG
                # compression, plus FrameSequenceBackground.cpp now also
                # draws with high-quality resampling). Still well short of
                # full source resolution so extraction time, embedded
                # binary size and per-frame draw cost all stay in check.
                # Adjust here if you want a different balance.
                execute_process(
                    COMMAND "${W27_FFMPEG_EXECUTABLE}" -y -i "${mp4File}" -t 15
                            -vf "fps=15,scale='min(1080,iw)':-2" -q:v 2
                            "${mp4OutDir}/${mp4Base}_frame_%04d.jpg"
                    RESULT_VARIABLE mp4ExtractResult
                    OUTPUT_QUIET ERROR_QUIET
                )

                if (NOT mp4ExtractResult EQUAL 0)
                    message(WARNING "27wavVST: ffmpeg failed to extract frames from ${mp4File} -- this background will be skipped.")
                else()
                    file(WRITE "${mp4MtimeMarker}" "${mp4CurrentMtime}")
                endif()
            endif()

            file(GLOB mp4Frames "${mp4OutDir}/*.jpg")
            list(APPEND W27_MP4_FRAME_FILES ${mp4Frames})
        endforeach()
    endif()
endif()

set(W27_BACKGROUND_BINARY_SOURCES ${W27_BACKGROUND_GIFS} ${W27_MP4_FRAME_FILES})

if (W27_BACKGROUND_BINARY_SOURCES)
    juce_add_binary_data(27wavVSTBackgrounds SOURCES ${W27_BACKGROUND_BINARY_SOURCES})
    target_link_libraries(27wavVST PRIVATE 27wavVSTBackgrounds)
    # A dedicated flag (rather than relying on JUCE's own generic
    # JUCE_TARGET_HAS_BINARY_DATA, which juce_add_binary_data sets to 1 for
    # EVERY binary-data library linked in, Samples' below included) -- so
    # BackgroundManager.cpp can tell specifically whether ITS BinaryData.h
    # was actually generated, independent of whether Assets/Samples/ has
    # anything in it.
    target_compile_definitions(27wavVST PRIVATE W27_HAS_BACKGROUND_DATA=1)
endif()

# --- Sample libraries (oneshot packs) ---------------------------------------
# Samples are NOT embedded into the plugin binary -- real Rio Leyva packs
# run into the gigabytes, and compiling that much data into the binary
# made builds impractically slow and would have produced a multi-gigabyte
# plugin bundle. Instead, Library/SampleLibraryManager reads audio files
# straight from a fixed folder on disk at runtime
# (~/Music/27wav rioleyva & noahmejia banks/, re-scanned every time the plugin's
# library button is opened -- see that class and Assets/Samples/README.md
# for the full folder convention). Nothing to configure/embed here at
# build time as a result.

target_compile_definitions(27wavVST PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(27wavVST
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
