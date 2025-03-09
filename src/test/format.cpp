#include <algorithm>
#include <array>
#include <numeric>
#include <gtest/gtest.h>

#include <base/big_int.h>
#include <base/system.h>
#include <base/format.h>

TEST(FormatTest, FmtDigit)
{
	EXPECT_EQ(fmt_digit<int>(123456), "123.456");
	EXPECT_EQ(fmt_digit<std::string>("123456"), "123.456");
}

TEST(FormatTest, FmtBigDigit)
{
	EXPECT_EQ(fmt_big_digit("1234567"), "1.23m");
}

TEST(FormatTest, Fmt)
{
	EXPECT_EQ(fmt_default("Hello, {}!", "World"), std::string("Hello, World!"));
	EXPECT_EQ(fmt_default("Hello, World!"), std::string("Hello, World!"));
	EXPECT_EQ(fmt_default("{h}, {w}!", "Hello", "World"), std::string("Hello, World!"));
}

inline CFormatter g_fmt_handled {};
std::string fmt_handler(int, const char* text, void*)
{
	return std::string(text) + " (handled)";
}

TEST(FormatTest, FmtHandle)
{
	g_fmt_handled.init(&fmt_handler, nullptr);
	EXPECT_EQ(g_fmt_handled("{}, World!", "Hello"), std::string("Hello, World! (handled)"));
	g_fmt_handled.init(nullptr, nullptr);
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
	g_fmt_handled.init(&fmt_handler_args, nullptr);

	EXPECT_EQ(g_fmt_handled("Hello, {}!", "World"), std::string("HELLO, World!"));

	g_fmt_handled.use_flags(FMTFLAG_HANDLE_ARGS);
	EXPECT_EQ(g_fmt_handled("Hello, {}!", "World"), std::string("HELLO, WORLD!"));

	g_fmt_handled.init(nullptr, nullptr);
}

TEST(FormatTest, FmtMissedArgs)
{
	EXPECT_EQ(fmt_default("Hello, {0}!"), "Hello, {0}!");
}