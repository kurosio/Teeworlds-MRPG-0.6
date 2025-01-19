#include "rating_system.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "AccountData.h"

constexpr double DECAY_FACTOR = 0.01;
constexpr double LEVEL_FACTOR = 0.05;

void RatingSystem::Init(CAccountData* pAccount)
{
	m_pAccount = pAccount;
	m_FileName = GetRatingFilename(pAccount->GetID());
	Load();
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

	// elo rating calculation
	double ExpectedScore = 1.0 / (1.0 + std::pow(10, (OpponentRating - m_Rating) / 400.0));
	int Score = Won ? 1 : 0;

	// adjust rating change based on result
	double RatingChange = g_Config.m_SvRatingCoefficientBase * (Score - ExpectedScore);

	// adjust for level difference
	int LevelDifference = m_pAccount->GetLevel() - OpponentLevel;
	RatingChange *= (1.0 + LEVEL_FACTOR * std::abs(LevelDifference));

	// further adjustments for high ratings
	if(m_Rating >= g_Config.m_SvMaxRating)
		RatingChange *= 0.5;

	// apply decay factor
	double Decay = 1.0 - DECAY_FACTOR * std::min(m_Rating, g_Config.m_SvMaxRating) / g_Config.m_SvMaxRating;
	RatingChange *= Decay;

	// update rating and track history
	int OldRating = m_Rating;
	m_Rating = std::clamp(static_cast<int>(std::round(m_Rating + RatingChange)), g_Config.m_SvMinRating, g_Config.m_SvMaxRating);
	m_Played++;
	m_vHistory.push_back(Score);

	// notify player of rating change
	int RatingDiff = m_Rating - OldRating;
	if(RatingDiff != 0 && pGS)
	{
		const char* Message = RatingDiff > 0 ?
			"Your rating increased by {}({}) points ({})":
			"Your rating decreased by {}({}) points ({})";
		pGS->Chat(m_pAccount->GetClientID(), Message, std::abs(RatingDiff), m_Rating, GetRankName());
		Save();
	}
}

std::string RatingSystem::GetRankName() const
{
	static const std::array<std::pair<double, std::string>, 5> Ranks = 
	{
		std::make_pair(22.0, "Bronze"),
		std::make_pair(44.0, "Silver"),
		std::make_pair(66.0, "Gold"),
		std::make_pair(88.0, "Platinum"),
		std::make_pair(100.0, "Legend")
	};

	double Percent = 100.0 * (m_Rating - g_Config.m_SvMinRating) / (g_Config.m_SvMaxRating - g_Config.m_SvMinRating);
	for(const auto& [threshold, rank] : Ranks)
	{
		if(Percent <= threshold)
			return rank;
	}
	return "Legend";
}

void RatingSystem::Create()
{
	if(!m_pAccount)
		return;

	if(m_vHistory.empty())
	{
		m_Rating = g_Config.m_SvMinRating;
		m_Played = 0;
	}

	Save();
}

void RatingSystem::Load()
{
	if(!m_pAccount)
		return;

	ByteArray RawData;
	if(!mystd::file::load(m_FileName.c_str(), &RawData))
	{
		Create();
		return;
	}

	auto j = nlohmann::json::parse((char*)RawData.data());
	m_Rating = j.value("rating", g_Config.m_SvMinRating);
	m_Played = j.value("played", 0);
	m_vHistory = j.value("history", std::vector<int>{});
}

void RatingSystem::Save()
{
	if(!m_pAccount)
		return;

	nlohmann::json j = {
		{"rating", m_Rating},
		{"played", m_Played},
		{"history", m_vHistory}
	};

	std::string Data = j.dump(4);
	auto Result = mystd::file::save(m_FileName.c_str(), Data.data(), static_cast<unsigned>(Data.size()));
	if(Result == mystd::file::result::SUCCESSFUL)
	{
		auto* pServer = Instance::Server();
		pServer->UpdateAccountBase(m_pAccount->GetID(), pServer->ClientName(m_pAccount->GetClientID()), m_Rating);
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Rating = '{}' WHERE ID = '{}'", m_Rating, m_pAccount->GetID());
	}
	else
	{
		dbg_msg("rating", "Failed to save the rating file AID(%d).", m_pAccount->GetID());
	}
}