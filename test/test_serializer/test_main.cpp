#define DOCTEST_CONFIG_IMPLEMENT         // REQUIRED: Enable custom main()
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS  // Compiling on ESP32 - the embedded platform doesn't support signals.

#include <doctest.h>
#include <string.h>

#include "serializer.h"


TEST_SUITE("rtrim()") {
    TEST_CASE("Remove any kind of non char simbols from right side") {
        char sample1[] = " test test";
        CHECK(!strcmp(" test test", rtrim(sample1)));
        char sample2[] = " test test ";
        CHECK(!strcmp(" test test", rtrim(sample2)));
        char sample3[] = " test test     ";
        CHECK(!strcmp(" test test", rtrim(sample3)));
        char sample4[] = " test test  \t  ";
        CHECK(!strcmp(" test test", rtrim(sample4)));
        char sample5[] = " test test  \n  ";
        CHECK(!strcmp(" test test", rtrim(sample5)));
        char sample6[] = " test test  \r  ";
        CHECK(!strcmp(" test test", rtrim(sample6)));
        char sample7[] = " test test  \0  ";
        CHECK(!strcmp(" test test", rtrim(sample7)));
    }
}

TEST_SUITE("nextln()") {
    TEST_CASE("Simple line") {
        char sample[] = "012345";
        size_t len = sizeof(sample);
        CHECK(nextln(sample, len) == NULL);
        CHECK(!strcmp("012345", sample));

        CHECK(nextln(sample, 4) == NULL);
        CHECK(!strcmp("012", sample));
    }
    TEST_CASE("Line with return") {
        char sample1[] = "012345\n";
        size_t len1 = sizeof(sample1);
        CHECK(nextln(sample1, len1) == NULL);
        CHECK(!strcmp("012345", sample1));

        char sample2[] = "012345\n\r";
        size_t len2 = sizeof(sample2);
        CHECK(nextln(sample2, len2) == NULL);
        CHECK(!strcmp("012345", sample2));
    }
    TEST_CASE("Several lines") {
        char sample1[] = "012\n345\n\r678\n";
        size_t len1 = sizeof(sample1);
        char *sample2 = nextln(sample1, len1);
        CHECK(!strcmp("012", sample1));
        CHECK(!strcmp("345\n\r678\n", sample2));

        size_t len2 = len1 - (sample2 - sample1);
        char *sample3 = nextln(sample2, len2);
        CHECK(!strcmp("345", sample2));
        CHECK(!strcmp("678\n", sample3));

        size_t len3 = len2 - (sample3 - sample2);
        char *sample4 = nextln(sample3, len3);
        CHECK(sample4 == NULL);
        CHECK(!strcmp("678", sample3));
    }
    TEST_CASE("Can be zerro") {
        char sample1[] = "012\n\000345";
        size_t len1 = sizeof(sample1);
        char *sample2 = nextln(sample1, len1);
        CHECK(!strcmp("012", sample1));
        CHECK(!strcmp("345", sample2));
    }
    TEST_CASE("Lost tail") {
        char sample1[] = "$Hostname=fluidnc001-1\n[echo: $H";
        size_t len1 = 32;
        char *sample2 = nextln(sample1, len1);
        CHECK(!strcmp("$Hostname=fluidnc001-1", sample1));
        CHECK(!strcmp("[echo: $H", sample2));
    }
}

int main(int argc, char **argv) {
    doctest::Context context;

    context.setOption("success", true);     // Report successful tests
    context.setOption("no-exitcode", true); // Do not return non-zero code on failed test case
    context.applyCommandLine(argc, argv);

    return context.run();
}
