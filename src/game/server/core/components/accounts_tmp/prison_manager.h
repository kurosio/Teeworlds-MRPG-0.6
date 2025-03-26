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

class PrisonManager
{
	CGS* GS() const;
	CPlayer* GetPlayer() const;
	int m_ClientID {};
	PrisonTerm m_PrisonTerm {};

public:
	void Init(int ClientID)
	{
		m_ClientID = ClientID;
		Load();
	}

	void Imprison(int Seconds);
	void Release();
	void PostTick();
	bool IsInPrison() const { return m_PrisonTerm.IsActive(); }

private:
	void Load();
	void Save() const;
};

#endif