#include <gtest/gtest.h>

#include <base/math.h>

TEST(Percent, AddProcentToSource)
{
	// int
	int SourceInt = 4096;
	add_percent_to_source(&SourceInt, 50.0f);
	EXPECT_EQ(SourceInt, 6144);

	// float
	float SourceFloat = 192.055f;
	add_percent_to_source(&SourceFloat, 100.0f);
	EXPECT_FLOAT_EQ(SourceFloat, 384.11f);

	// double
	double SourceDouble = 200.0000428482;
	add_percent_to_source(&SourceDouble, 100.0f);
	EXPECT_DOUBLE_EQ(SourceDouble, 400.0000856964);
}

TEST(Percent, TestPercent)
{
	// translate to percent
	EXPECT_EQ(translate_to_percent(92593, 22310), 24); // int
	EXPECT_FLOAT_EQ(translate_to_percent(500.025f, 450.05f), 90.0054997f); // float
	EXPECT_DOUBLE_EQ(translate_to_percent(1329.00525, 450.00055), 33.859952772948034); // double

	// translate to percent rest
	EXPECT_EQ(translate_to_percent_rest(30, 50), 15); // int
	EXPECT_FLOAT_EQ(translate_to_percent_rest(30.5f, 50.1f), 15.2805f); // float
	EXPECT_DOUBLE_EQ(translate_to_percent_rest(30.00055, 50.33f), 15.099277364326475); // double

	// translate not explicit percent
	EXPECT_EQ(translate_to_percent_rest(30, 50), 15); // int
	EXPECT_FLOAT_EQ(translate_to_percent_rest(30.5f, 50), 15.25f); // float
	EXPECT_DOUBLE_EQ(translate_to_percent_rest(30.00055, 50), 15.000274999999998); // double
}