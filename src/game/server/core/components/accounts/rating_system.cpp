#include "rating_system.h"

#include <engine/server.h>
#include <game/server/gamecontext.h>
#include "account_data.h"

constexpr double DECAY_FACTOR = 0.01;
constexpr double LEVEL_FACTOR = 0.05;

inline static std::string getFileName(int AccountID)
{
	return std::string("server_data/account_rating/raiting_").append(std::to_string(AccountID)).append("AID").append(".json");
}

void RatingSystem::Init(CAccountData* pAccount)
{
	m_pAccount = pAccount;
	m_FileName = getFileName(pAccount->GetID());
	Load();
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

	// update history
	m_vHistory.push_back(Score);
	if(m_vHistory.size() > 20)
		m_vHistory.erase(m_vHistory.begin(), m_vHistory.begin() + (m_vHistory.size() - 20));

	// update won and lose
	if(Won)
		m_Wins++;
	else
		m_Losses++;

	// notify player of rating change
	int RatingDiff = m_Rating - OldRating;
	if(RatingDiff != 0 && pGS)
	{
		const char* Message = RatingDiff > 0 ?
			"Your rating increased by {}({}) points ({}).":
			"Your rating decreased by {}({}) points ({}).";
		pGS->Chat(m_pAccount->GetClientID(), Message, std::abs(RatingDiff), m_Rating, GetRankName());
		pGS->Chat(m_pAccount->GetClientID(), "Wins: {} / Losses: {} / Win rate: {~.2}%.", GetWins(), GetLosses(), GetWinRate());
		Save();
	}

	UpdateClient();
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
		m_Wins = 0;
		m_Losses = 0;
	}

	Save();
	UpdateClient();
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
	m_Wins = j.value("wins", 0);
	m_Losses = j.value("losses", 0);
	m_vHistory = j.value("history", std::vector<int>{});
	UpdateClient();
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

	std::string Data = j.dump(4);
	auto Result = mystd::file::save(m_FileName.c_str(), Data.data(), static_cast<unsigned>(Data.size()));
	if(Result == mystd::file::result::SUCCESSFUL)
	{
		Database->Execute<DB::UPDATE>("tw_accounts_data", "Rating = '{}' WHERE ID = '{}'", m_Rating, m_pAccount->GetID());
	}
	else
	{
		dbg_msg("rating", "Failed to save the rating file AID(%d).", m_pAccount->GetID());
	}
}

void RatingSystem::UpdateClient() const
{
	if(!m_pAccount)
		return;

	auto* pServer = Instance::Server();
	pServer->UpdateAccountBase(m_pAccount->GetID(), pServer->ClientName(m_pAccount->GetClientID()), m_Rating);
}
