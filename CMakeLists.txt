cmake_minimum_required (VERSION 3.5)

project (libctorrent LANGUAGES C)

set (CMAKE_C_EXTENSIONS ON)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug)
endif ()

include_directories (src)
include_directories (include)

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
set (THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads REQUIRED)

add_library (ctorrent)
target_sources (ctorrent
    PRIVATE
        src/util/error.c
        src/bcode.c
        src/crypt/sha1.c
        src/bitset.c
        src/disk.c
        src/message.c
        src/thread_msg.c
        src/piece.c
        src/pqueue.c
        src/torrent.c
        src/tracker.c
        src/tracker_udp.c
        src/net.c
        src/eventloop.c
        src/peer.c
        src/peerserver.c
        src/timer_queue.c
)

add_executable (tget)
target_sources (tget
    PRIVATE
        src/main.c
)

target_link_libraries (tget PUBLIC Threads::Threads)
target_link_libraries (tget PRIVATE ctorrent)

enable_testing ()
add_subdirectory (test/unit)
