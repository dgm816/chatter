add_executable(chatter main.c log.c irc.c tui.c version.c)
target_link_libraries(chatter PRIVATE ${CURSES_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)