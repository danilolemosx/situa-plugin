# Situa (AAX/AudioSuite, VST3, AU) com JUCE 8
#
#   cmake -B ~/.cache/situa-aax/build -S juce -DCMAKE_BUILD_TYPE=Release
#   cmake --build ~/.cache/situa-aax/build --target Situa_AAX Situa_VST3 Situa_AU testa-situa -j 10
#
# Compile fora do HD externo (o caminho tem espaços). O JUCE é o 8.0.9 sem remendo (o mesmo do Decanta).
# O Opus (BSD) é baixado e compilado junto, estático, na primeira configuração.
# Formatos: AAX (Media Composer/Pro Tools), VST3 e AU. No Windows, VST3 e AAX.

cmake_minimum_required(VERSION 3.22)
option(SITUA_UNIVERSAL "Mac: compilar para Apple Silicon e Intel" OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
if(SITUA_UNIVERSAL)
	set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
else()
	set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
endif()
project(SituaPlugin VERSION 0.1.0 LANGUAGES C CXX)

set(JUCE_DIR "$ENV{HOME}/.cache/decanta-aax/JUCE" CACHE PATH "Pasta do JUCE 8.0.9")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "JUCE não encontrado em ${JUCE_DIR}")
endif()
add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)

# --- Opus (codecs de celular e de chamada) ---------------------------------------
include(FetchContent)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
if(SITUA_UNIVERSAL)
	# as otimizações de processador são escolhidas por arquitetura; no binário duplo, ficam de fora
	set(OPUS_DISABLE_INTRINSICS ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(opus
	GIT_REPOSITORY https://github.com/xiph/opus.git
	GIT_TAG v1.5.2
	GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(opus)
set_target_properties(opus PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(APPLE)
	set(SITUA_FORMATOS AAX VST3 AU)
else()
	set(SITUA_FORMATOS AAX VST3)
endif()

juce_add_plugin(Situa
	COMPANY_NAME "Danilo Lemos"
	BUNDLE_ID "br.danilolemos.situa"
	PLUGIN_MANUFACTURER_CODE DnLm
	PLUGIN_CODE Situ
	FORMATS ${SITUA_FORMATOS}
	AAX_IDENTIFIER br.danilolemos.situa
	AAX_CATEGORY Reverb
	PRODUCT_NAME "Situa"
	DESCRIPTION "Dialogue worldizing: device, space and barrier"
	IS_SYNTH FALSE
	NEEDS_MIDI_INPUT FALSE
	NEEDS_MIDI_OUTPUT FALSE
	IS_MIDI_EFFECT FALSE
	NEEDS_WEB_BROWSER TRUE
	NEEDS_WEBVIEW2 TRUE
	VST3_CATEGORIES Fx Reverb
	AU_MAIN_TYPE kAudioUnitType_Effect
	COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(SituaUI SOURCES
	ui/index.html
	ui/style.css
	ui/app.js
	ui/fonts/archivo.woff2
	"${JUCE_DIR}/modules/juce_gui_extra/native/javascript/index.js")

set(SITUA_MOTOR Source/Motor.cpp Source/Convolucao.cpp)

target_sources(Situa PRIVATE
	Source/PluginProcessor.cpp
	Source/PluginEditor.cpp
	${SITUA_MOTOR})

target_compile_definitions(Situa PUBLIC
	JUCE_WEB_BROWSER=1
	JUCE_USE_CURL=0
	JUCE_VST3_CAN_REPLACE_VST2=0)
if(WIN32)
	target_compile_definitions(Situa PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

target_link_libraries(Situa PRIVATE opus)
target_compile_definitions(Situa PRIVATE SITUA_COM_OPUS=1)

target_link_libraries(Situa
	PRIVATE
		SituaUI
		juce::juce_audio_utils
		juce::juce_gui_extra
		juce::juce_dsp
	PUBLIC
		juce::juce_recommended_config_flags
		juce::juce_recommended_lto_flags
		juce::juce_recommended_warning_flags)

# --- testes ---------------------------------------------------------------------
# testa-situa: o processador direto (sem host), em blocos, como o AudioSuite chamaria;
# e a janela do VST3 aberta por alguns segundos.
juce_add_console_app(testa-situa PRODUCT_NAME "testa-situa")
target_sources(testa-situa PRIVATE teste/Main.cpp Source/PluginProcessor.cpp ${SITUA_MOTOR})
target_compile_definitions(testa-situa PRIVATE
	SITUA_SEM_JANELA=1
	JUCE_PLUGINHOST_VST3=1
	JUCE_MODAL_LOOPS_PERMITTED=1
	JUCE_WEB_BROWSER=0
	JUCE_USE_CURL=0)
target_compile_definitions(testa-situa PRIVATE SITUA_COM_OPUS=1)
if(APPLE)
	target_compile_definitions(testa-situa PRIVATE JUCE_PLUGINHOST_AU=1)   # o AU só existe no Mac
endif()
target_link_libraries(testa-situa PRIVATE
	opus
	juce::juce_audio_processors
	juce::juce_audio_formats
	juce::juce_audio_utils
	juce::juce_dsp
	juce::juce_recommended_config_flags)
