add_executable(ex1-volume ex1-volume.c)
target_link_libraries(ex1-volume PUBLIC ceed)

add_executable(ex2-surface ex2-surface.c)
target_link_libraries(ex2-surface PUBLIC ceed)

add_executable(ex3-volume ex3-volume.c)
target_link_libraries(ex3-volume PUBLIC ceed)
