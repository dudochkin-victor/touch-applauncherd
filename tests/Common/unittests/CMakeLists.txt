add_subdirectory(ut_booster)
add_subdirectory(ut_boosterfactory)
add_subdirectory(ut_connection)
add_subdirectory(ut_daemon)
add_subdirectory(ut_dbooster)
add_subdirectory(ut_mbooster)
add_subdirectory(ut_qtbooster)
add_subdirectory(ut_socketmanager)

install(FILES tests.xml DESTINATION /usr/share/applauncherd-tests)

