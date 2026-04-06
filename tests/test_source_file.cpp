#include <gtest/gtest.h>

#include "source_file.h"

TEST(SourceFileTest, AcceptsVtExtension) {
    EXPECT_TRUE(hasSupportedSourceExtension("examples/hello_world.vt"));
}

TEST(SourceFileTest, RejectsLegacyVantaExtension) {
    EXPECT_FALSE(hasSupportedSourceExtension("examples/hello_world.vanta"));
}

TEST(SourceFileTest, RejectsMissingExtension) {
    EXPECT_FALSE(hasSupportedSourceExtension("examples/hello_world"));
}
