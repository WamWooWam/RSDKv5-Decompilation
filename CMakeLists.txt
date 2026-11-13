# THIS CMAKE SHOULD BE USED FOR ANDROID ONLY but also has to be used for classic mac os unless you want to figure out how to port Retro68's cmake helpers over
# might make this all plats later but not needed for now, the Makefile works fine

cmake_minimum_required(VERSION 3.7)

if(CMAKE_TOOLCHAIN_FILE MATCHES "retroppc")
    cmake_minimum_required(VERSION 3.20)
    project(RetroEngine C CXX)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    set(GAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Game CACHE PATH "")
    set(MILSKO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmacos/milsko CACHE PATH "")
    set(OGG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmacos/libogg CACHE PATH "")
    set(THEORA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmacos/libtheora CACHE PATH "")

    set(MW_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MW_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(MW_TRY_OPENGL ON CACHE BOOL "" FORCE)
    set(MW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(MW_INSTALL_HEADERS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${MILSKO_DIR} milsko EXCLUDE_FROM_ALL)

    set(INCLUDE_INTTYPES_H 1)
    set(INCLUDE_STDINT_H 1)
    set(INCLUDE_SYS_TYPES_H 1)
    set(SIZE16 int16_t)
    set(USIZE16 uint16_t)
    set(SIZE32 int32_t)
    set(USIZE32 uint32_t)
    set(SIZE64 int64_t)
    set(USIZE64 uint64_t)
    configure_file(${OGG_DIR}/include/ogg/config_types.h.in ${CMAKE_CURRENT_BINARY_DIR}/ogg/include/ogg/config_types.h @ONLY)

    add_library(ogg STATIC ${OGG_DIR}/src/bitwise.c ${OGG_DIR}/src/framing.c)
    target_include_directories(ogg PUBLIC ${OGG_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}/ogg/include)
    target_compile_options(ogg PRIVATE -O2 -w)

    add_library(theoradec STATIC
        ${THEORA_DIR}/lib/apiwrapper.c
        ${THEORA_DIR}/lib/bitpack.c
        ${THEORA_DIR}/lib/decapiwrapper.c
        ${THEORA_DIR}/lib/decinfo.c
        ${THEORA_DIR}/lib/decode.c
        ${THEORA_DIR}/lib/dequant.c
        ${THEORA_DIR}/lib/fragment.c
        ${THEORA_DIR}/lib/huffdec.c
        ${THEORA_DIR}/lib/idct.c
        ${THEORA_DIR}/lib/info.c
        ${THEORA_DIR}/lib/internal.c
        ${THEORA_DIR}/lib/quant.c
        ${THEORA_DIR}/lib/state.c
    )
    target_include_directories(theoradec PUBLIC ${THEORA_DIR}/include)
    target_link_libraries(theoradec PUBLIC ogg)
    target_compile_options(theoradec PRIVATE -O2 -w)

    set(DEFINES
        RETRO_REVISION=2
        RETRO_STANDALONE=1
        RETRO_USE_MOD_LOADER=0
        BASE_PATH=\"\"
    )

    add_library(Game SHARED
        ${GAME_DIR}/Game.c
        ${GAME_DIR}/Objects/All.c
    )
    target_include_directories(Game PRIVATE ${GAME_DIR} ${GAME_DIR}/Objects)
    target_compile_definitions(Game PRIVATE ${DEFINES})
    target_compile_options(Game PRIVATE -fsigned-char -O2 -w)
    set_target_properties(Game PROPERTIES LINK_FLAGS -Wl,-bE:${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmacos/Game.exp)

    add_custom_command(
        OUTPUT Game.pef
        COMMAND ${MAKE_PEF} libGame.so -o Game.pef
        DEPENDS Game
    )

    if(REZ_INCLUDE_PATH)
        set(REZ_INCLUDE -I${REZ_INCLUDE_PATH})
    endif()

    add_custom_command(
        OUTPUT GameLogic.bin GameLogic.dsk
        COMMAND ${REZ} ${REZ_FLAGS} ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmacos/Game.r ${REZ_INCLUDE}
                -o GameLogic.bin --cc GameLogic.dsk -t shlb -c SMan --data Game.pef
        DEPENDS Game.pef ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmacos/Game.r
    )
    add_custom_target(Game_shlb ALL DEPENDS GameLogic.bin)

    set_source_files_properties(dependencies/all/miniz/miniz.c PROPERTIES COMPILE_DEFINITIONS "MINIZ_NO_TIME;MINIZ_NO_ARCHIVE_APIS")

    add_application(RSDKv5
        RSDKv5/main.cpp
        RSDKv5/RSDK/Core/RetroEngine.cpp
        RSDKv5/RSDK/Core/Math.cpp
        RSDKv5/RSDK/Core/Reader.cpp
        RSDKv5/RSDK/Core/Link.cpp
        RSDKv5/RSDK/Core/ModAPI.cpp
        RSDKv5/RSDK/Dev/Debug.cpp
        RSDKv5/RSDK/Storage/Storage.cpp
        RSDKv5/RSDK/Storage/Text.cpp
        RSDKv5/RSDK/Graphics/Drawing.cpp
        RSDKv5/RSDK/Graphics/Scene3D.cpp
        RSDKv5/RSDK/Graphics/Animation.cpp
        RSDKv5/RSDK/Graphics/Sprite.cpp
        RSDKv5/RSDK/Graphics/Palette.cpp
        RSDKv5/RSDK/Graphics/Video.cpp
        RSDKv5/RSDK/Audio/Audio.cpp
        RSDKv5/RSDK/Input/Input.cpp
        RSDKv5/RSDK/Scene/Scene.cpp
        RSDKv5/RSDK/Scene/Collision.cpp
        RSDKv5/RSDK/Scene/Object.cpp
        RSDKv5/RSDK/Scene/Objects/DefaultObject.cpp
        RSDKv5/RSDK/Scene/Objects/DevOutput.cpp
        RSDKv5/RSDK/User/Core/UserAchievements.cpp
        RSDKv5/RSDK/User/Core/UserCore.cpp
        RSDKv5/RSDK/User/Core/UserLeaderboards.cpp
        RSDKv5/RSDK/User/Core/UserPresence.cpp
        RSDKv5/RSDK/User/Core/UserStats.cpp
        RSDKv5/RSDK/User/Core/UserStorage.cpp
        dependencies/all/tinyxml2/tinyxml2.cpp
        dependencies/all/iniparser/iniparser.cpp
        dependencies/all/iniparser/dictionary.cpp
        dependencies/all/miniz/miniz.c
        dependencies/cmacos/cmacosHelpers.cpp
        dependencies/cmacos/resource.r
        CREATOR SMan
    )
    set_target_properties(RSDKv5 PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_include_directories(RSDKv5 PRIVATE
        RSDKv5/
        dependencies/all/
        dependencies/all/tinyxml2/
        dependencies/all/iniparser/
        dependencies/cmacos/
    )
    target_compile_definitions(RSDKv5 PRIVATE ${DEFINES})
    target_compile_options(RSDKv5 PRIVATE -fsigned-char -O2 -w $<$<COMPILE_LANGUAGE:CXX>:-fpermissive -Wno-narrowing>)
    target_link_libraries(RSDKv5 PRIVATE Mw theoradec ogg InterfaceLib m)

    return()
else()
  set(OGG_DIR dependencies/android/libogg)
  set(THEORA_DIR dependencies/android/libtheora)
  add_library(
      libogg
      STATIC
      ${OGG_DIR}/src/bitwise.c
      ${OGG_DIR}/src/framing.c
  )

  add_library(libtheora STATIC
      ${THEORA_DIR}/lib/analyze.c
      ${THEORA_DIR}/lib/apiwrapper.c
      ${THEORA_DIR}/lib/bitpack.c
      ${THEORA_DIR}/lib/cpu.c
      ${THEORA_DIR}/lib/decapiwrapper.c
      ${THEORA_DIR}/lib/decinfo.c
      ${THEORA_DIR}/lib/decode.c
      ${THEORA_DIR}/lib/dequant.c
      ${THEORA_DIR}/lib/encapiwrapper.c
      ${THEORA_DIR}/lib/encfrag.c
      ${THEORA_DIR}/lib/encinfo.c
      ${THEORA_DIR}/lib/encode.c
      ${THEORA_DIR}/lib/encoder_disabled.c
      ${THEORA_DIR}/lib/enquant.c
      ${THEORA_DIR}/lib/fdct.c
      ${THEORA_DIR}/lib/fragment.c
      ${THEORA_DIR}/lib/huffdec.c
      ${THEORA_DIR}/lib/huffenc.c
      ${THEORA_DIR}/lib/idct.c
      ${THEORA_DIR}/lib/info.c
      ${THEORA_DIR}/lib/internal.c
      ${THEORA_DIR}/lib/mathops.c
      ${THEORA_DIR}/lib/mcenc.c
      ${THEORA_DIR}/lib/quant.c
      ${THEORA_DIR}/lib/rate.c
      ${THEORA_DIR}/lib/state.c
      ${THEORA_DIR}/lib/tokenize.c
  )

  target_include_directories(libogg PRIVATE dependencies/android/libogg/include)
  target_include_directories(libtheora PRIVATE dependencies/android/libogg/include dependencies/android/libtheora/include)

  target_compile_options(libogg PRIVATE -ffast-math -fsigned-char -O2 -fPIC -DPIC -DBYTE_ORDER=LITTLE_ENDIAN -D_ARM_ASSEM_ -w)
  target_compile_options(libtheora PRIVATE -ffast-math -fsigned-char -O2 -fPIC -DPIC -DBYTE_ORDER=LITTLE_ENDIAN -D_ARM_ASSEM_ -w)

  add_library(RetroEngine SHARED
      RSDKv5/main.cpp
      RSDKv5/RSDK/Core/RetroEngine.cpp
      RSDKv5/RSDK/Core/Math.cpp
      RSDKv5/RSDK/Core/Reader.cpp
      RSDKv5/RSDK/Core/Link.cpp
      RSDKv5/RSDK/Core/ModAPI.cpp
      RSDKv5/RSDK/Dev/Debug.cpp
      RSDKv5/RSDK/Storage/Storage.cpp
      RSDKv5/RSDK/Storage/Text.cpp
      RSDKv5/RSDK/Graphics/Drawing.cpp
      RSDKv5/RSDK/Graphics/Scene3D.cpp
      RSDKv5/RSDK/Graphics/Animation.cpp
      RSDKv5/RSDK/Graphics/Sprite.cpp
      RSDKv5/RSDK/Graphics/Palette.cpp
      RSDKv5/RSDK/Graphics/Video.cpp
      RSDKv5/RSDK/Audio/Audio.cpp
      RSDKv5/RSDK/Input/Input.cpp
      RSDKv5/RSDK/Scene/Scene.cpp
      RSDKv5/RSDK/Scene/Collision.cpp
      RSDKv5/RSDK/Scene/Object.cpp
      RSDKv5/RSDK/Scene/Objects/DefaultObject.cpp
      RSDKv5/RSDK/Scene/Objects/DevOutput.cpp
      RSDKv5/RSDK/User/Core/UserAchievements.cpp
      RSDKv5/RSDK/User/Core/UserCore.cpp
      RSDKv5/RSDK/User/Core/UserLeaderboards.cpp
      RSDKv5/RSDK/User/Core/UserPresence.cpp
      RSDKv5/RSDK/User/Core/UserStats.cpp
      RSDKv5/RSDK/User/Core/UserStorage.cpp
      dependencies/all/tinyxml2/tinyxml2.cpp
      dependencies/all/iniparser/iniparser.cpp
      dependencies/all/iniparser/dictionary.cpp
      dependencies/all/miniz/miniz.c
      dependencies/android/androidHelpers.cpp
  )

  target_include_directories(RetroEngine PRIVATE
      RSDKv5/
      dependencies/android/libogg/include/
      dependencies/android/libtheora/include/
      dependencies/android/
      dependencies/all/
      dependencies/all/tinyxml2/
      dependencies/all/iniparser/
  )

  target_compile_options(RetroEngine PRIVATE -fexceptions -frtti -DRSDK_USE_OGL -DRETRO_REVISION=3)

  find_package(games-controller REQUIRED CONFIG)
  find_package(game-activity REQUIRED CONFIG)
  find_package(games-frame-pacing REQUIRED CONFIG)
  find_package(oboe REQUIRED CONFIG)

  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u GameActivity_onCreate")

  target_link_libraries(
      RetroEngine
      android
      libogg
      libtheora
      EGL
      GLESv3
      log
      z
      jnigraphics
      games-controller::paddleboat_static
      game-activity::game-activity
      games-frame-pacing::swappy_static
      oboe::oboe
      OpenSLES
  )
endif()