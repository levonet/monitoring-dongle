#define DOCTEST_CONFIG_IMPLEMENT         // REQUIRED: Enable custom main()
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS  // Compiling on ESP32 - the embedded platform doesn't support signals.

#include <doctest.h>
#include <string.h>

#include "ring_buffer.h"

ring_buffer_t rbuf;

TEST_SUITE("ring_buffer()") {
    TEST_CASE("one byte in/out") {
        ring_buffer_init(&rbuf, 8);
        CHECK(ring_buffer_available(&rbuf) == 0);
        CHECK(ring_buffer_read(&rbuf) == -1);
        ring_buffer_append(&rbuf, '1');
        CHECK(ring_buffer_available(&rbuf) == 1);
        CHECK(ring_buffer_read(&rbuf) == '1');
        CHECK(ring_buffer_available(&rbuf) == 0);
        CHECK(ring_buffer_read(&rbuf) == -1);
        ring_buffer_delete(&rbuf);
    }
    TEST_CASE("fill and catch up") {
        ring_buffer_init(&rbuf, 8);
        ring_buffer_append(&rbuf, '1');
        ring_buffer_append(&rbuf, '2');
        ring_buffer_append(&rbuf, '3');
        CHECK(ring_buffer_read(&rbuf) == '1');
        CHECK(ring_buffer_read(&rbuf) == '2');
        ring_buffer_append(&rbuf, '4');
        ring_buffer_append(&rbuf, '5');
        CHECK(ring_buffer_read(&rbuf) == '3');
        CHECK(ring_buffer_read(&rbuf) == '4');
        CHECK(ring_buffer_read(&rbuf) == '5');
        CHECK(ring_buffer_read(&rbuf) == -1);
        ring_buffer_delete(&rbuf);
    }
    TEST_CASE("move in the ring") {
        ring_buffer_init(&rbuf, 8);
        ring_buffer_append(&rbuf, '1');
        ring_buffer_append(&rbuf, '2');
        ring_buffer_append(&rbuf, '3');
        ring_buffer_append(&rbuf, '4');
        ring_buffer_append(&rbuf, '5');
        CHECK(ring_buffer_read(&rbuf) == '1');
        CHECK(ring_buffer_read(&rbuf) == '2');
        ring_buffer_append(&rbuf, '6');
        ring_buffer_append(&rbuf, '7');
        ring_buffer_append(&rbuf, '8');
        CHECK(ring_buffer_read(&rbuf) == '3');
        CHECK(ring_buffer_read(&rbuf) == '4');
        CHECK(ring_buffer_read(&rbuf) == '5');
        CHECK(ring_buffer_read(&rbuf) == '6');
        CHECK(ring_buffer_read(&rbuf) == '7');
        ring_buffer_append(&rbuf, '9');
        ring_buffer_append(&rbuf, 'a');
        ring_buffer_append(&rbuf, 'b');
        ring_buffer_append(&rbuf, 'c');
        CHECK(ring_buffer_read(&rbuf) == '8');
        CHECK(ring_buffer_read(&rbuf) == '9');
        CHECK(ring_buffer_read(&rbuf) == 'a');
        CHECK(ring_buffer_read(&rbuf) == 'b');
        CHECK(ring_buffer_read(&rbuf) == 'c');
        CHECK(ring_buffer_read(&rbuf) == -1);
        ring_buffer_delete(&rbuf);
    }
    TEST_CASE("no overflow") {
        ring_buffer_init(&rbuf, 8);             // 0:0
        ring_buffer_append(&rbuf, '1');         // 1:0
        ring_buffer_append(&rbuf, '2');         // 2:0
        ring_buffer_append(&rbuf, '3');         // 3:0
        ring_buffer_append(&rbuf, '4');         // 4:0
        CHECK(ring_buffer_read(&rbuf) == '1');  // 4:1
        CHECK(ring_buffer_read(&rbuf) == '2');  // 4:2
        ring_buffer_append(&rbuf, '5');         // 5:2
        ring_buffer_append(&rbuf, '6');         // 6:2
        ring_buffer_append(&rbuf, '7');         // 7:2
        ring_buffer_append(&rbuf, '8');         // 0:2
        ring_buffer_append(&rbuf, '9');         // 1:2
        CHECK(ring_buffer_head(&rbuf) == 1);
        CHECK(ring_buffer_tail(&rbuf) == 2);
        ring_buffer_append(&rbuf, 'a');         // skip: 1+1:2
        ring_buffer_append(&rbuf, 'b');         // skip: 1+1:2
        ring_buffer_append(&rbuf, 'c');         // skip: 1+1:2
        CHECK(ring_buffer_read(&rbuf) == '3');  // 1:3
        CHECK(ring_buffer_read(&rbuf) == '4');  // 1:4
        CHECK(ring_buffer_read(&rbuf) == '5');  // 1:5
        CHECK(ring_buffer_read(&rbuf) == '6');  // 1:6
        CHECK(ring_buffer_read(&rbuf) == '7');  // 1:7
        CHECK(ring_buffer_read(&rbuf) == '8');  // 1:0
        CHECK(ring_buffer_read(&rbuf) == '9');  // 1:1
        CHECK(ring_buffer_read(&rbuf) == -1);   // empty: 1:1
        ring_buffer_append(&rbuf, 'd');         // 2:1
        ring_buffer_append(&rbuf, 'e');         // 3:1
        ring_buffer_append(&rbuf, 'f');         // 4:1
        CHECK(ring_buffer_read(&rbuf) == 'd');  // 4:2
        CHECK(ring_buffer_read(&rbuf) == 'e');  // 4:3
        CHECK(ring_buffer_read(&rbuf) == 'f');  // 4:4
        CHECK(ring_buffer_head(&rbuf) == 4);
        CHECK(ring_buffer_tail(&rbuf) == 4);
        ring_buffer_delete(&rbuf);
    }
}

int main(int argc, char **argv) {
    doctest::Context context;

    context.setOption("success", true);     // Report successful tests
    context.setOption("no-exitcode", true); // Do not return non-zero code on failed test case
    context.applyCommandLine(argc, argv);

    return context.run();
}
