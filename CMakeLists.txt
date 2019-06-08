cmake_minimum_required (VERSION 3.5)

project (libctorrent LANGUAGES C)

set (CMAKE_C_STANDARD 99)
set (CMAKE_C_EXTENSIONS ON)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug)
endif ()

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
set (THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads REQUIRED)

add_executable (tget)
target_sources (tget
    PRIVATE
        src/bcode.c
        src/bitset.c
        src/crypt/sha1.c
        src/disk.c
        src/util/error.c
        src/main.c
        src/message.c
        src/thread.c
        src/thread_msg.c
        src/timer_queue.c
        src/piece.c
        src/pqueue.c
        src/torrent.c
        src/tracker.c
        src/tracker_udp.c
)
target_include_directories (tget PRIVATE src)
target_link_libraries (tget PUBLIC Threads::Threads)
