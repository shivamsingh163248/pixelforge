#include "pixelforge/version.h"

#include <gtest/gtest.h>

#include <string>

TEST(Version, MacroAndConstantsAgree) {
    EXPECT_EQ(pixelforge::version_major, PIXELFORGE_VERSION_MAJOR);
    EXPECT_EQ(pixelforge::version_minor, PIXELFORGE_VERSION_MINOR);
    EXPECT_EQ(pixelforge::version_patch, PIXELFORGE_VERSION_PATCH);
    EXPECT_EQ(std::string(pixelforge::version_string), PIXELFORGE_VERSION_STRING);
}
