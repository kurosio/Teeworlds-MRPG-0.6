// Tests of the json helper every loader uses (mystd::json::parse).
//
// The helper is the only place where a document coming from a data file or a
// database column meets the parser, and the server has to survive whatever is
// in there: a document that was cut short, two documents that ended up one
// after the other, or a document whose content is not what the caller expects.

#include <teeother/stdafx_shared.h>

#include <gtest/gtest.h>

namespace
{
struct SResult
{
	bool m_Failed = true;
	bool m_Called = false;
	nlohmann::json m_Json;
};

SResult ParseDocument(const std::string& Data, const std::function<void(nlohmann::json&)>& pCallback = {})
{
	SResult Result;
	Result.m_Failed = mystd::json::parse(Data, [&Result, &pCallback](nlohmann::json& Json)
	{
		Result.m_Called = true;
		Result.m_Json = Json;
		if(pCallback)
			pCallback(Json);
	});
	return Result;
}
}

TEST(JsonParseTest, CompleteDocumentIsApplied)
{
	const auto Result = ParseDocument(R"({"skin":"default","color_body":-1})");
	EXPECT_FALSE(Result.m_Failed);
	EXPECT_TRUE(Result.m_Called);
	EXPECT_EQ(Result.m_Json.value("skin", std::string()), "default");
	EXPECT_EQ(Result.m_Json.value("color_body", 0), -1);
}

TEST(JsonParseTest, EmptyInputIsReportedWithoutRunningTheCallback)
{
	// An empty file or column is "nothing stored yet": the helper reports it the
	// same way as unreadable data (the callers then rebuild the file), but the
	// callback never sees an empty document.
	const auto Result = ParseDocument("");
	EXPECT_TRUE(Result.m_Failed);
	EXPECT_FALSE(Result.m_Called);
}

TEST(JsonParseTest, TwoDocumentsInARowAreReportedAndDropped)
{
	// The reported regression: a second '{' right after a complete document.
	// The document is rejected as a whole - never parsed twice, never merged.
	const auto Result = ParseDocument(R"({"skin":"default","color_body":-1}{"skin":"default"})");
	EXPECT_TRUE(Result.m_Failed);
	EXPECT_FALSE(Result.m_Called);
}

TEST(JsonParseTest, TrailingDataAfterADocumentIsReported)
{
	const auto Result = ParseDocument(R"({"skin":"default"} trailing)");
	EXPECT_TRUE(Result.m_Failed);
	EXPECT_FALSE(Result.m_Called);
}

TEST(JsonParseTest, TruncatedDocumentIsReported)
{
	const auto Result = ParseDocument(R"({"skin":"default","color_body":-)");
	EXPECT_TRUE(Result.m_Failed);
	EXPECT_FALSE(Result.m_Called);
}

TEST(JsonParseTest, ContentTheCallbackCannotUseDoesNotStopTheServer)
{
	// A document that parses but does not hold the expected keys throws inside
	// the callback. That must end the load, not the process: this helper runs
	// while worlds start.
	const auto Result = ParseDocument(R"({"unexpected":1})", [](nlohmann::json& Json)
	{
		(void)Json.at("required_key").get<std::string>();
	});
	EXPECT_TRUE(Result.m_Failed);
	EXPECT_TRUE(Result.m_Called);
}
