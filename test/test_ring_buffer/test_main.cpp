#define DOCTEST_CONFIG_IMPLEMENT         // REQUIRED: Enable custom main()
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS  // Compiling on ESP32 - the embedded platform doesn't support signals.

#include <doctest.h>
#include <string.h>

#include "ring_buffer.h"


TEST_SUITE("ring_buffer()") {
    TEST_CASE("dummy") {
        CHECK(TRUE);
    }
}

int main(int argc, char **argv) {
    doctest::Context context;

    context.setOption("success", true);     // Report successful tests
    context.setOption("no-exitcode", true); // Do not return non-zero code on failed test case
    context.applyCommandLine(argc, argv);

    return context.run();
}
