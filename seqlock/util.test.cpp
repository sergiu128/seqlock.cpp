#include "seqlock/util.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

using namespace seqlock::util;  // NOLINT

TEST(Util, RoundToPageSize) {
    auto page_size = getpagesize();
    ASSERT_EQ(RoundToPageSize(0), page_size);
    ASSERT_EQ(RoundToPageSize(page_size - 1), page_size);
    ASSERT_EQ(RoundToPageSize(page_size + 1), 2 * page_size);
    ASSERT_EQ(RoundToPageSize(page_size), page_size);
}
