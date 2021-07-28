

add_executable(rzsz
          rzsz.c 
          serial.c)
target_link_libraries(rzsz lxyzmodem)

add_executable(dbg_serial dbg_serial.c serial.c)
target_link_libraries(dbg_serial lxyzmodem)
