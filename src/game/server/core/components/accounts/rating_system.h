#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_RATING_SYSTEM_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_RATING_SYSTEM_H

class CGS;
class CAccountData;
class RatingSystem
{
	int m_Rating {};
	int64_t m_Wins {};
	int64_t m_Losses {};
	int64_t m_Played {};
	std::vector<int> m_vHistory {};
	CAccountData* m_pAccount {};
	std::string m_FileName {};

public:
	RatingSystem() = default;

	void Init(CAccountData* pAccount);
	void DecreaseRating(int Rating);
	void UpdateRating(CGS* pGS, bool Won, CAccountData* pOpponentAccount);
	void UpdateRating(CGS* pGS, bool won, int OpponentRating, int OpponentLevel);

	int GetRating() const { return m_Rating; }
	int GetPlayed() const  { return m_Played; }
	int GetWins() const { return m_Wins; }
	int GetLosses() const { return m_Losses; }
	double GetWinRate() const;
	std::string GetRankName() const;

private:
	void Create();
	void Load();
	void Save();
	void UpdateClient() const;
};


#endif