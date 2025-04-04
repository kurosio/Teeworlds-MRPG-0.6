#include <gtest/gtest.h>
#include <base/timeperiod.h>

class CTimePeriodTest : public ::testing::Test {};

// Test default constructor and isZero
TEST_F(CTimePeriodTest, DefaultConstructorCreatesZeroDuration) {
    CTimePeriod period;
    EXPECT_TRUE(period.isZero());
    EXPECT_EQ(period.seconds(), 0);
    EXPECT_EQ(period.minutes(), 0);
    EXPECT_EQ(period.hours(), 0);
    EXPECT_EQ(period.days(), 0);
    EXPECT_EQ(period.duration().count(), 0);
}

// Test constructor with values
TEST_F(CTimePeriodTest, ConstructorWithValuesInitializesCorrectly) {
    CTimePeriod period(30, 45, 5, 2);
    EXPECT_FALSE(period.isZero());
    EXPECT_EQ(period.seconds(), 30);
    EXPECT_EQ(period.minutes(), 45);
    EXPECT_EQ(period.hours(), 5);
    EXPECT_EQ(period.days(), 2);

    // Total should be 2d*86400 + 5h*3600 + 45m*60 + 30s = 193530 seconds
    EXPECT_EQ(period.duration().count(), 193530);
}

// Test normalization of values
TEST_F(CTimePeriodTest, ValuesAreNormalizedCorrectly) {
    // 90 seconds should normalize to 1 minute and 30 seconds
    CTimePeriod period(90, 0, 0, 0);
    EXPECT_EQ(period.seconds(), 30);
    EXPECT_EQ(period.minutes(), 1);
    EXPECT_EQ(period.hours(), 0);
    EXPECT_EQ(period.days(), 0);

    // 70 minutes should normalize to 1 hour and 10 minutes
    CTimePeriod period2(0, 70, 0, 0);
    EXPECT_EQ(period2.seconds(), 0);
    EXPECT_EQ(period2.minutes(), 10);
    EXPECT_EQ(period2.hours(), 1);
    EXPECT_EQ(period2.days(), 0);

    // 25 hours should normalize to 1 day and 1 hour
    CTimePeriod period3(0, 0, 25, 0);
    EXPECT_EQ(period3.seconds(), 0);
    EXPECT_EQ(period3.minutes(), 0);
    EXPECT_EQ(period3.hours(), 1);
    EXPECT_EQ(period3.days(), 1);

    // Complex case: 90s + 70m + 25h
    CTimePeriod period4(90, 70, 25, 0);
    EXPECT_EQ(period4.seconds(), 30);
    EXPECT_EQ(period4.minutes(), 11);
    EXPECT_EQ(period4.hours(), 2);
    EXPECT_EQ(period4.days(), 1);
}

// Test parsing from string (short format)
TEST_F(CTimePeriodTest, ParsesShortFormatCorrectly) {
    CTimePeriod period("5s");
    EXPECT_EQ(period.seconds(), 5);
    EXPECT_EQ(period.duration().count(), 5);

    CTimePeriod period2("10m");
    EXPECT_EQ(period2.minutes(), 10);
    EXPECT_EQ(period2.duration().count(), 600);

    CTimePeriod period3("3h");
    EXPECT_EQ(period3.hours(), 3);
    EXPECT_EQ(period3.duration().count(), 10800);

    CTimePeriod period4("2d");
    EXPECT_EQ(period4.days(), 2);
    EXPECT_EQ(period4.duration().count(), 172800);
}

// Test parsing from string (long format)
TEST_F(CTimePeriodTest, ParsesLongFormatCorrectly) {
    CTimePeriod period("5seconds");
    EXPECT_EQ(period.seconds(), 5);

    CTimePeriod period2("10minutes");
    EXPECT_EQ(period2.minutes(), 10);

    CTimePeriod period3("3hours");
    EXPECT_EQ(period3.hours(), 3);

    CTimePeriod period4("2days");
    EXPECT_EQ(period4.days(), 2);
}

// Test parsing complex string with multiple time units
TEST_F(CTimePeriodTest, ParsesComplexStringCorrectly) {
    CTimePeriod period("2d5h30m15s");
    EXPECT_EQ(period.seconds(), 15);
    EXPECT_EQ(period.minutes(), 30);
    EXPECT_EQ(period.hours(), 5);
    EXPECT_EQ(period.days(), 2);
    EXPECT_EQ(period.duration().count(), 2*86400 + 5*3600 + 30*60 + 15);

    // Test with spaces and different order
    CTimePeriod period2("15s 2d 30m 5h");
    EXPECT_EQ(period2.seconds(), 15);
    EXPECT_EQ(period2.minutes(), 30);
    EXPECT_EQ(period2.hours(), 5);
    EXPECT_EQ(period2.days(), 2);
}

// Test parsing months and years
TEST_F(CTimePeriodTest, ParsesMonthsAndYearsCorrectly) {
    CTimePeriod period("1mo");
    EXPECT_EQ(period.duration().count(), 2419200);

    CTimePeriod period2("1y");
    EXPECT_EQ(period2.duration().count(), 31536000);

    CTimePeriod period3("1months");
    EXPECT_EQ(period3.duration().count(), 2419200);

    CTimePeriod period4("1years");
    EXPECT_EQ(period4.duration().count(), 31536000);
}

// Test toString method
TEST_F(CTimePeriodTest, ToStringFormatsCorrectly) {
    CTimePeriod period(15, 30, 5, 2);
    EXPECT_EQ(period.toString(), "2d 5h 30m 15s");

    // Test with some zero values
    CTimePeriod period2(15, 0, 5, 0);
    EXPECT_EQ(period2.toString(), "5h 15s");

    // Test with all zero values except seconds
    CTimePeriod period3(15, 0, 0, 0);
    EXPECT_EQ(period3.toString(), "15s");

    // Test with completely zero duration
    CTimePeriod period4;
    EXPECT_EQ(period4.toString(), "0s");
}

// Test asSqlInterval method
TEST_F(CTimePeriodTest, SqlIntervalFormatsCorrectly) {
    CTimePeriod period(15, 30, 5, 2);
    EXPECT_EQ(period.asSqlInterval(), "interval 192615 second");

    CTimePeriod period2(0, 0, 0, 0);
    EXPECT_EQ(period2.asSqlInterval(), "interval 0 second");
}

// Test comparison operators
TEST_F(CTimePeriodTest, ComparisonOperatorsWorkCorrectly) {
    CTimePeriod p1(15, 30, 5, 2);
    CTimePeriod p2(15, 30, 5, 2);
    CTimePeriod p3(0, 0, 0, 0);
    CTimePeriod p4(16, 30, 5, 2);

    // Test equality
    EXPECT_TRUE(p1 == p2);
    EXPECT_FALSE(p1 == p3);

    // Test inequality
    EXPECT_TRUE(p1 != p3);
    EXPECT_FALSE(p1 != p2);

    // Test less than
    EXPECT_TRUE(p3 < p1);
    EXPECT_FALSE(p1 < p3);

    // Test greater than
    EXPECT_TRUE(p1 > p3);
    EXPECT_FALSE(p3 > p1);

    // Test less than or equal
    EXPECT_TRUE(p1 <= p2);
    EXPECT_TRUE(p3 <= p1);
    EXPECT_FALSE(p1 <= p3);

    // Test greater than or equal
    EXPECT_TRUE(p1 >= p2);
    EXPECT_TRUE(p1 >= p3);
    EXPECT_FALSE(p3 >= p1);

    // Test with slightly different values
    EXPECT_TRUE(p1 < p4);
    EXPECT_TRUE(p4 > p1);
}

// Test complex parsing scenarios
TEST_F(CTimePeriodTest, HandlesComplexParsingScenarios) {
    // Mixed short and long formats
    CTimePeriod period("1d 2hours 30m 45seconds");
    EXPECT_EQ(period.days(), 1);
    EXPECT_EQ(period.hours(), 2);
    EXPECT_EQ(period.minutes(), 30);
    EXPECT_EQ(period.seconds(), 45);

    // Repeated units should be cumulative
    CTimePeriod period2("1d 1d 1h 1h");
    EXPECT_EQ(period2.days(), 2);
    EXPECT_EQ(period2.hours(), 2);

    // Test with extra spaces and mixed case (should be handled by scanner)
    CTimePeriod period3("1d  2h   3m    4s");
    EXPECT_EQ(period3.days(), 1);
    EXPECT_EQ(period3.hours(), 2);
    EXPECT_EQ(period3.minutes(), 3);
    EXPECT_EQ(period3.seconds(), 4);
}

// Test complex parsing scenarios
TEST_F(CTimePeriodTest, EmptyLiterals)
{
    // Mixed short and long formats
    CTimePeriod period("1");
    EXPECT_EQ(period.days(), 0);
    EXPECT_EQ(period.hours(), 0);
    EXPECT_EQ(period.minutes(), 1);
    EXPECT_EQ(period.seconds(), 0);

    CTimePeriod period2("1 2 3 4 5");
    EXPECT_EQ(period2.days(), 0);
    EXPECT_EQ(period2.hours(), 0);
    EXPECT_EQ(period2.minutes(), 15);
    EXPECT_EQ(period2.seconds(), 0);

    CTimePeriod period3("2h 15  10y");
    EXPECT_EQ(period3.days(), 3650);
    EXPECT_EQ(period3.hours(), 2);
    EXPECT_EQ(period3.minutes(), 15);
    EXPECT_EQ(period3.seconds(), 0);
}