#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_RATING_SYSTEM_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_RATING_SYSTEM_H

class CGS;
class CAccountData;
class RatingSystem
{
private:
	int m_Rating {};
	int m_Played {};
	std::vector<int> m_vHistory {};
	CAccountData* m_pAccount {};
	std::string m_FileName {};

public:
	RatingSystem() = default;

	void Init(CAccountData* pAccount);
	void UpdateRating(CGS* pGS, bool Won, CAccountData* pOpponentAccount);
	void UpdateRating(CGS* pGS, bool won, int OpponentRating, int OpponentLevel);

	int GetRating() const { return m_Rating; }
	int GetPlayed() const  { return m_Played; }
	int GetWins() const { return std::count(m_vHistory.begin(), m_vHistory.end(), 1); }
	int GetLosses() const { return std::count(m_vHistory.begin(), m_vHistory.end(), 0); }
	std::string GetRankName() const;

	void Create();
	void Load();
	void Save();
};


#endif