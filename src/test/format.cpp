#include <algorithm>
#include <gtest/gtest.h>

#include <base/big_int.h>
#include <base/system.h>
#include <base/format.h>

TEST(FormatTest, FmtDigit)
{
	// Test integer value
	EXPECT_EQ(fmt_digit<int>(123456), "123.456");

	// Test string value
	EXPECT_EQ(fmt_digit<std::string>("123456"), "123.456");
}

TEST(FormatTest, FmtBigDigit)
{
	// Test integer value
	EXPECT_EQ(fmt_big_digit<int>(1234567), "1.234million");

	// Test string value
	EXPECT_EQ(fmt_big_digit<std::string>("1234567"), "1.234million");
}

TEST(FormatTest, Fmt)
{
	// Test formatting with arguments
	EXPECT_EQ(fmt("Hello, {0}!", "World"), "Hello, World!");

	// Test formatting without arguments
	EXPECT_EQ(fmt("Hello, World!"), "Hello, World!");
}

std::string fmt_handler(int, const char* text, void*)
{
	return std::string(text) + " (handled)";;
}

TEST(FormatTest, FmtHandle)
{
	// Test formatting with handler callback
	fmt_init_handler_func(fmt_handler, nullptr);
	EXPECT_EQ(fmt_handle(0, "Hello, World!"), "Hello, World! (handled)");
	fmt_init_handler_func(nullptr, nullptr);
}

TEST(FormatTest, FmtUseFlags)
{
	// Test setting and getting flags
	fmt_use_flags(FMTFLAG_HANDLE_ARGS | FMTFLAG_DIGIT_COMMAS);
	EXPECT_EQ(struct_handler_fmt::get_flags(), FMTFLAG_HANDLE_ARGS | FMTFLAG_DIGIT_COMMAS);
}

// return std::string in upper case
std::string fmt_handler_args(int, const char* text, void*)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}
	
TEST(FormatTest, FmtHandleArgs)
{
	// Test formatting with handle args flag
	fmt_use_flags(FMTFLAG_HANDLE_ARGS);
	fmt_init_handler_func(fmt_handler_args, nullptr);
	EXPECT_EQ(fmt_handle(0, "Hello, {0}!", "World"), "HELLO, WORLD!");
	fmt_init_handler_func(nullptr, nullptr);
}

TEST(FormatTest, FmtDigitCommas)
{
	// Test formatting with digit commas flag
	fmt_use_flags(FMTFLAG_DIGIT_COMMAS);
	EXPECT_EQ(fmt_digit<int>(1234567890), "1.234.567.890");
}

TEST(FormatTest, FmtBigDigitCommas)
{
	// Test formatting with big digit commas flag
	fmt_use_flags(FMTFLAG_DIGIT_COMMAS);
	EXPECT_EQ(fmt_big_digit<int>(1234567890), "1.234billion");
}

TEST(FormatTest, FmtMissedArgs)
{
	// Test formatting with missed arguments
	EXPECT_EQ(fmt("Hello, {0}!"), "Hello, {0}!");
}