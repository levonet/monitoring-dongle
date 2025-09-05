#define DOCTEST_CONFIG_IMPLEMENT         // REQUIRED: Enable custom main()
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS  // Compiling on ESP32 - the embedded platform doesn't support signals.

#include <doctest.h>
#include <string.h>

#include "serializer.h"

#define CMP(str1, str2) (!strcmp((str1), (const char*)(str2)))


TEST_SUITE("rtrim()") {
    TEST_CASE("Remove any kind of non char simbols from right side") {
        unsigned char sample1[] = " test test";
        CHECK(CMP(" test test", rtrim(sample1)));
        CHECK(CMP(" test test", rtrim(sample1)));
        unsigned char sample2[] = " test test ";
        CHECK(CMP(" test test", rtrim(sample2)));
        unsigned char sample3[] = " test test     ";
        CHECK(CMP(" test test", rtrim(sample3)));
        unsigned char sample4[] = " test test  \t  ";
        CHECK(CMP(" test test", rtrim(sample4)));
        unsigned char sample5[] = " test test  \n  ";
        CHECK(CMP(" test test", rtrim(sample5)));
        unsigned char sample6[] = " test test  \r  ";
        CHECK(CMP(" test test", rtrim(sample6)));
        unsigned char sample7[] = " test test  \0  ";
        CHECK(CMP(" test test", rtrim(sample7)));
    }
}

TEST_SUITE("nextln()") {
    TEST_CASE("Simple line") {
        unsigned char sample[] = "012345";
        size_t len = sizeof(sample);
        CHECK(nextln(sample, len) == NULL);
        CHECK(CMP("012345", sample));

        CHECK(nextln(sample, 4) == NULL);
        CHECK(CMP("012", sample));
    }
    TEST_CASE("Line with return") {
        unsigned char sample1[] = "012345\n";
        size_t len1 = sizeof(sample1);
        CHECK(nextln(sample1, len1) == NULL);
        CHECK(CMP("012345", sample1));

        unsigned char sample2[] = "012345\n\r";
        size_t len2 = sizeof(sample2);
        CHECK(nextln(sample2, len2) == NULL);
        CHECK(CMP("012345", sample2));
    }
    TEST_CASE("Several lines") {
        unsigned char sample1[] = "012\n345\n\r678\n";
        size_t len1 = sizeof(sample1);
        unsigned char *sample2 = nextln(sample1, len1);
        CHECK(CMP("012", sample1));
        CHECK(CMP("345\n\r678\n", sample2));

        size_t len2 = len1 - (sample2 - sample1);
        unsigned char *sample3 = nextln(sample2, len2);
        CHECK(CMP("345", sample2));
        CHECK(CMP("678\n", sample3));

        size_t len3 = len2 - (sample3 - sample2);
        unsigned char *sample4 = nextln(sample3, len3);
        CHECK(sample4 == NULL);
        CHECK(CMP("678", sample3));
    }
    TEST_CASE("Can be zerro") {
        unsigned char sample1[] = "012\n\000345";
        size_t len1 = sizeof(sample1);
        unsigned char *sample2 = nextln(sample1, len1);
        CHECK(CMP("012", sample1));
        CHECK(CMP("345", sample2));
    }
    TEST_CASE("Lost tail") {
        unsigned char sample1[] = "$Hostname=fluidnc001-1\n[echo: $H";
        size_t len1 = 32;
        unsigned char *sample2 = nextln(sample1, len1);
        CHECK(CMP("$Hostname=fluidnc001-1", sample1));
        CHECK(CMP("[echo: $H", sample2));
    }
}

int main(int argc, char **argv) {
    doctest::Context context;

    context.setOption("success", true);     // Report successful tests
    context.setOption("no-exitcode", true); // Do not return non-zero code on failed test case
    context.applyCommandLine(argc, argv);

    return context.run();
}
