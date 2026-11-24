#include "rating_system.h"

#include <engine/server.h>
#include <engine/server/async_file.h>
#include <game/server/core/tools/db_async_context.h>
#include <game/server/gamecontext.h>
#include "account_data.h"
#include "account_db.h"

namespace
{
	constexpr int MAX_HISTORY_SIZE = 20;
	constexpr int RANK_COUNT = 6;
	constexpr double EXPECTED_SCORE_DIVISOR = 500.0;
	constexpr double LEVEL_ADVANTAGE_FACTOR = 0.02;
	constexpr double UPSET_WIN_BONUS_FACTOR = 0.05;
	constexpr int MAX_UPSET_BONUS = 8;
	constexpr int MAX_STREAK_BONUS = 5;

	std::string GetRatingFileName(int AccountID)
	{
		return std::string("server_data/account_rating/raiting_").append(std::to_string(AccountID)).append("AID").append(".json");
	}
}

void RatingSystem::Init(CAccountData* pAccount)
{
	m_pAccount = pAccount;
	m_Rating = g_Config.m_SvMinRating;
	m_Wins = 0;
	m_Losses = 0;
	m_Played = 0;
	m_vHistory.clear();
	m_FileName = GetRatingFileName(pAccount->GetID());
}

void RatingSystem::DecreaseRating(int Decreasing)
{
	m_Rating = std::clamp(static_cast<int>(std::round(m_Rating - Decreasing)), g_Config.m_SvMinRating, g_Config.m_SvMaxRating);
	Save();
}

double RatingSystem::GetWinRate() const
{
	if(m_Played == 0)
		return 0.0;
	return (static_cast<double>(m_Wins) / static_cast<double>(m_Played)) * 100.0;
}

void RatingSystem::UpdateRating(CGS* pGS, bool Won, CAccountData* pOpponentAccount)
{
	int OpponentRating = pOpponentAccount->GetRatingSystem().GetRating();
	int OpponentLevel = pOpponentAccount->GetLevel();
	UpdateRating(pGS, Won, OpponentRating, OpponentLevel);
}

void RatingSystem::UpdateRating(CGS* pGS, bool Won, int OpponentRating, int OpponentLevel)
{
	if(!m_pAccount)
		return;

	// Elo-like expectation.
	const double ExpectedScore = 1.0 / (1.0 + std::pow(10.0, (OpponentRating - m_Rating) / EXPECTED_SCORE_DIVISOR));
	int Score = Won ? 1 : 0;
	double RatingChange = g_Config.m_SvRatingCoefficientBase * (Score - ExpectedScore);

	// Small reward for winning against stronger opponents.
	if(Won && OpponentRating > m_Rating)
	{
		const int RatingGap = OpponentRating - m_Rating;
		RatingChange += minimum(MAX_UPSET_BONUS, static_cast<int>(std::round(RatingGap * UPSET_WIN_BONUS_FACTOR)));
	}

	// Level difference should matter, but only slightly.
	const int LevelDifference = m_pAccount->GetLevel() - OpponentLevel;
	RatingChange *= (1.0 + LEVEL_ADVANTAGE_FACTOR * std::clamp(LevelDifference, -6, 6));

	// Winning streak bonus improves motivation to keep playing.
	int StreakBonus = 0;
	if(Won)
	{
		for(auto it = m_vHistory.rbegin(); it != m_vHistory.rend() && *it == 1; ++it)
			StreakBonus++;
		RatingChange += minimum(StreakBonus, MAX_STREAK_BONUS);
	}

	int OldRating = m_Rating;
	m_Rating = std::clamp(m_Rating + static_cast<int>(std::round(RatingChange)), g_Config.m_SvMinRating, g_Config.m_SvMaxRating);
	m_Played++;

	m_vHistory.push_back(Score);
	if(m_vHistory.size() > MAX_HISTORY_SIZE)
		m_vHistory.erase(m_vHistory.begin(), m_vHistory.begin() + (m_vHistory.size() - MAX_HISTORY_SIZE));

	if(Won)
		m_Wins++;
	else
		m_Losses++;

	const int RatingDiff = m_Rating - OldRating;
	if(RatingDiff != 0 && pGS)
	{
		const char* Message = RatingDiff > 0 ?
			"Rating +{} -> {} ({~})":
			"Rating -{} -> {} ({~})";
		pGS->Chat(m_pAccount->GetClientID(), Message, std::abs(RatingDiff), m_Rating, GetRankName());
		pGS->Chat(m_pAccount->GetClientID(), "Progress to {}: {~}/{} pts.", GetNextRankName(), GetRankPointsProgress(), GetRankPointsRequired());
		Save();
	}

	UpdateClient();
}

std::vector<RatingSystem::RankInfo> RatingSystem::BuildRanks() const
{
	std::vector<RankInfo> vRanks;
	vRanks.reserve(RANK_COUNT);

	const int Range = maximum(1, g_Config.m_SvMaxRating - g_Config.m_SvMinRating);
	const int Step = maximum(1, Range / (RANK_COUNT - 1));
	vRanks.push_back({g_Config.m_SvMinRating, "Bronze"});
	vRanks.push_back({g_Config.m_SvMinRating + Step * 1, "Silver"});
	vRanks.push_back({g_Config.m_SvMinRating + Step * 2, "Gold"});
	vRanks.push_back({g_Config.m_SvMinRating + Step * 3, "Platinum"});
	vRanks.push_back({g_Config.m_SvMinRating + Step * 4, "Diamond"});
	vRanks.push_back({g_Config.m_SvMaxRating, "Legend"});
	return vRanks;
}

int RatingSystem::GetCurrentRankIndex(const std::vector<RankInfo>& vRanks) const
{
	int RankIndex = 0;
	for(size_t i = 0; i < vRanks.size(); i++)
	{
		if(m_Rating >= vRanks[i].m_MinRating)
			RankIndex = static_cast<int>(i);
	}
	return RankIndex;
}

std::string RatingSystem::GetRankName() const
{
	const auto vRanks = BuildRanks();
	const int RankIndex = GetCurrentRankIndex(vRanks);
	return vRanks[RankIndex].m_pName;
}

std::string RatingSystem::GetNextRankName() const
{
	const auto vRanks = BuildRanks();
	const int RankIndex = GetCurrentRankIndex(vRanks);
	const bool IsLastRank = RankIndex >= static_cast<int>(vRanks.size()) - 1;
	return IsLastRank ? "Legend" : vRanks[RankIndex + 1].m_pName;
}

int RatingSystem::GetRankPointsProgress() const
{
	const auto vRanks = BuildRanks();
	const int RankIndex = GetCurrentRankIndex(vRanks);
	const bool IsLastRank = RankIndex >= static_cast<int>(vRanks.size()) - 1;
	if(IsLastRank)
		return GetRankPointsRequired();

	const int RankFloor = vRanks[RankIndex].m_MinRating;
	return maximum(0, m_Rating - RankFloor);
}

int RatingSystem::GetRankPointsRequired() const
{
	const auto vRanks = BuildRanks();
	const int RankIndex = GetCurrentRankIndex(vRanks);
	const bool IsLastRank = RankIndex >= static_cast<int>(vRanks.size()) - 1;
	if(IsLastRank)
		return 0;

	const int RankFloor = vRanks[RankIndex].m_MinRating;
	const int NextRankFloor = vRanks[RankIndex + 1].m_MinRating;
	return maximum(1, NextRankFloor - RankFloor);
}

void RatingSystem::Create()
{
	if(!m_pAccount)
		return;

	if(m_vHistory.empty())
	{
		m_Rating = g_Config.m_SvMinRating;
		m_Played = 0;
		m_Wins = 0;
		m_Losses = 0;
	}

	Save();
	UpdateClient();
}

void RatingSystem::RegisterLoadStage()
{
	if(!m_pAccount)
		return;

	const int ClientID = m_pAccount->GetClientID();
	const std::string Filename = m_FileName;
	m_pAccount->AddLoadStage("rating", [ClientID, Filename](CAccountData::CLoadDone Done)
	{
		auto pContext = DbAsync::MakeContext(ClientID);
		AsyncFile::Read(Filename.c_str(), [pContext, Filename, Done = std::move(Done)](AsyncFile::SReadResult&& Read) mutable
		{
			auto* pPlayer = pContext->GetPlayer();
			if(!pPlayer || !pPlayer->Account())
			{
				Done(false);
				return;
			}
			if(!Read.Ok() && !Read.Missing())
			{
				dbg_msg("rating_system", "could not read '%s': %s", Filename.c_str(), Read.m_Error.c_str());
				Done(false);
				return;
			}

			const std::string& Content = Read.m_Content;
			auto& Self = pPlayer->Account()->GetRatingSystem();
			if(Content.empty())
			{
				Self.Create();
				Done(true);
				return;
			}

			const bool ParseError = mystd::json::parse(Content, [&Self](nlohmann::json& j)
			{
				Self.m_Rating = j.value("rating", g_Config.m_SvMinRating);
				Self.m_Played = j.value("played", 0);
				Self.m_Wins = j.value("wins", 0);
				Self.m_Losses = j.value("losses", 0);
				Self.m_vHistory = j.value("history", std::vector<int>{});
			});

			if(ParseError)
			{
				dbg_msg("rating_system", "can't initialize '%s'. Re-creating....", Self.m_FileName.c_str());
				Self.m_Rating = g_Config.m_SvMinRating;
				Self.m_Played = 0;
				Self.m_Wins = 0;
				Self.m_Losses = 0;
				Self.m_vHistory.clear();
				Self.Save();
			}

			Self.UpdateClient();
			Done(true);
		});
	});
}

void RatingSystem::Save()
{
	if(!m_pAccount)
		return;

	nlohmann::json j = {
		{"rating", m_Rating},
		{"played", m_Played},
		{"wins", m_Wins},
		{"losses", m_Losses},
		{"history", m_vHistory}
	};

	// The file is a cache of the rating; it is written in the background and the
	// database copy is updated regardless of the disk.
	AsyncFile::Write(m_FileName.c_str(), j.dump(4));
	AccountDb::SaveRating(m_pAccount->GetID(), m_Rating);
}

void RatingSystem::UpdateClient() const
{
	if(!m_pAccount)
		return;

	auto* pServer = Instance::Server();
	pServer->UpdateAccountBase(m_pAccount->GetID(), pServer->ClientName(m_pAccount->GetClientID()), m_Rating);
}
