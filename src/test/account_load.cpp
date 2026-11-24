#include <game/server/core/components/accounts/account_load_plan.h>

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

TEST(AccountLoadPlanTest, RunsIndependentStagesAndHonorsDependencies)
{
	CAccountLoadPlan Account;
	std::vector<std::string> vStarted;
	std::map<std::string, CAccountLoadPlan::CCompletion> vCompleteStage;
	bool Completed = false;
	bool Succeeded = false;

	auto Register = [&](const std::string& Name, std::vector<std::string> Dependencies)
	{
		Account.Add(Name, std::move(Dependencies), [&, Name](CAccountLoadPlan::CCompletion Done)
		{
			vStarted.push_back(Name);
			vCompleteStage[Name] = std::move(Done);
		});
	};
	Register("account", {});
	Register("inventory", {});
	Register("equipment", { "inventory" });

	Account.Start([&](bool Success)
	{
		Completed = true;
		Succeeded = Success;
	});

	EXPECT_EQ(vStarted, (std::vector<std::string>{ "account", "inventory" }));
	EXPECT_FALSE(Completed);
	vCompleteStage.at("inventory")(true);
	EXPECT_EQ(vStarted, (std::vector<std::string>{ "account", "inventory", "equipment" }));
	EXPECT_FALSE(Completed);
	vCompleteStage.at("account")(true);
	EXPECT_FALSE(Completed);
	vCompleteStage.at("equipment")(true);
	EXPECT_TRUE(Completed);
	EXPECT_TRUE(Succeeded);
}

TEST(AccountLoadPlanTest, FailureStopsPlanAndCompletesOnce)
{
	CAccountLoadPlan Account;
	std::map<std::string, CAccountLoadPlan::CCompletion> vCompleteStage;
	int Completions = 0;
	bool Succeeded = true;

	for(const std::string Name : { "database", "files" })
	{
		Account.Add(Name, {}, [&, Name](CAccountLoadPlan::CCompletion Done)
		{
			vCompleteStage[Name] = std::move(Done);
		});
	}
	Account.Start([&](bool Success)
	{
		++Completions;
		Succeeded = Success;
	});

	vCompleteStage.at("database")(false);
	vCompleteStage.at("files")(true);
	EXPECT_EQ(Completions, 1);
	EXPECT_FALSE(Succeeded);
}

TEST(AccountLoadPlanTest, CancellationSuppressesLateCompletion)
{
	CAccountLoadPlan Account;
	CAccountLoadPlan::CCompletion CompleteStage;
	bool Completed = false;
	Account.Add("pending", {}, [&](CAccountLoadPlan::CCompletion Done) { CompleteStage = std::move(Done); });
	Account.Start([&](bool) { Completed = true; });

	Account.Cancel();
	CompleteStage(true);
	EXPECT_FALSE(Completed);
}

TEST(AccountLoadPlanTest, RejectsCyclesWithoutStartingStages)
{
	CAccountLoadPlan Account;
	int Starts = 0;
	bool Completed = false;
	bool Succeeded = true;
	Account.Add("first", { "second" }, [&](CAccountLoadPlan::CCompletion) { ++Starts; });
	Account.Add("second", { "first" }, [&](CAccountLoadPlan::CCompletion) { ++Starts; });

	Account.Start([&](bool Success)
	{
		Completed = true;
		Succeeded = Success;
	});

	EXPECT_EQ(Starts, 0);
	EXPECT_TRUE(Completed);
	EXPECT_FALSE(Succeeded);
}
