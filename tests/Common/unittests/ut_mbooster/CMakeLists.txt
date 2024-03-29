set(LAUNCHER ${CMAKE_HOME_DIRECTORY}/src/launcherlib)
set(MBOOSTER ${CMAKE_HOME_DIRECTORY}/src/mbooster)

# Set sources
set(SRC ut_mbooster.cpp ${LAUNCHER}/appdata.cpp ${LAUNCHER}/booster.cpp
${LAUNCHER}/connection.cpp
${LAUNCHER}/logger.cpp 
${MBOOSTER}/mbooster.cpp ${MBOOSTER}/eventhandler.cpp
${LAUNCHER}/singleinstance.cpp ${LAUNCHER}/socketmanager.cpp)

# Set moc headers
set(MOC_HDRS ut_mbooster.h ${MBOOSTER}/eventhandler.h)

# Run moc
qt4_wrap_cpp(MOC_SRC ${MOC_HDRS})

# Enable test library
set(QT_USE_QTTEST TRUE)

# Set include paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_HOME_DIRECTORY}/src/common ${LAUNCHER} ${MBOOSTER})

link_libraries(${QT_QTCORE_LIBRARY} ${QT_QTTEST_LIBRARY} ${MEEGOTOUCH_LIBRARIES} ${LIBCREDS})

# Enable Qt (may not be needed, because already defined on higher level)
include(${QT_USE_FILE})

add_executable(ut_mbooster ${SRC} ${MOC_SRC} )

# Install
install(PROGRAMS ut_mbooster DESTINATION /usr/share/applauncherd-tests/)

