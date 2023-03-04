#!/bin/bash
install_name_tool -add_rpath @executable_path/../Frameworks $1
#install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib RSDKv5

#install_name_tool -change /opt/local/lib/libgcc/libstdc++.6.dylib @executable_path/../Frameworks/libgcc/libstdc++.6.dylib RSDKv5
#install_name_tool -change /opt/local/lib/libgcc/libgcc_s.1.dylib @executable_path/../Frameworks/libgcc/libgcc_s.1.dylib RSDKv5

install_name_tool -change @executable_path/libSDL2-2.0.0.dylib @executable_path/../Frameworks/libSDL2-2.0.0.dylib $1
install_name_tool -change /opt/local/lib/libSDL2-2.0.0.dylib @executable_path/../Frameworks/libSDL2-2.0.0.dylib $1
install_name_tool -change /opt/local/lib/libtheora.0.dylib @executable_path/../Frameworks/libtheora.0.dylib $1
install_name_tool -change /opt/local/lib/libogg.0.dylib @executable_path/../Frameworks/libogg.0.dylib $1
install_name_tool -change /opt/local/lib/libvorbis.0.dylib @executable_path/../Frameworks/libvorbis.0.dylib $1
