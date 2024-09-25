#ifndef GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_PRISON_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_ACCOUNTS_PRISON_MANAGER_H

#include <vector>
#include <ctime>

class CGS;
class CPlayer;

struct PrisonTerm
{
	int ImprisonmentTime {};
	time_t StartTime {};

	bool IsActive() const { return (time(nullptr) - StartTime) < ImprisonmentTime; }
	int RemainingTime() const { return maximum(0, ImprisonmentTime - static_cast<int>(time(nullptr) - StartTime)); }
};

class CPrisonManager
{
	int m_ClientID {};
	PrisonTerm m_PrisonTerm {};

	CGS* GS() const;
	CPlayer* GetPlayer() const;
	
public:
	void Init(int ClientID)
	{
		m_ClientID = ClientID;
		LoadPrisonData();
	}

	void Imprison(int Seconds);
	void Free();
	bool IsInPrison() const { return m_PrisonTerm.IsActive(); }
	void UpdatePrison();

	std::pair<int, std::string> GetPrisonStatusString() const;

private:
	void LoadPrisonData();
	void SavePrisonData() const;
};

#endif