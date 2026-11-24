#ifndef GAME_SERVER_CORE_COMPONENTS_DUTIES_DUTIES_MANAGER_H
#define GAME_SERVER_CORE_COMPONENTS_DUTIES_DUTIES_MANAGER_H
#include <game/server/core/mmo_component.h>

#include "dungeon_data.h"

class CVoteLoad;

class CDutiesManager : public MmoComponent
{
	~CDutiesManager() override
	{
		mystd::freeContainer(CDungeonData::Data());
	}

	void OnPreInit() override;
	bool OnPlayerVoteCommand(CPlayer* pPlayer, const char* pCmd, const std::vector<std::any> &Extras, int ReasonNumber, const char* pReason) override;
	bool OnSendMenuVotes(CPlayer* pPlayer, int Menulist) override;

public:
	void ShowInsideMenu(CPlayer* pPlayer) const;
	void ShowDungeonInfo(CPlayer* pPlayer, CDungeonData* pDungeon) const;
	void ShowPvpInfo(CPlayer* pPlayer, int WorldID) const;
	void ShowRhythmInfo(CPlayer* pPlayer, int WorldID) const;
	void ShowSocialInfo(CPlayer* pPlayer, int WorldID) const;
	void ShowMiniGamesInfo(CPlayer* pPlayer, int WorldID) const;
	const std::vector<std::string>& GetRhythmDifficulties(int WorldID) const;

	static CVoteLoad BestDungeonLoad();
	static CVoteLoad BestRhythmLoad();

	CDungeonData* GetDungeonByID(int DungeonID) const;
	CDungeonData* GetDungeonByWorldID(int WorldID) const;
	int GetWorldsCountByType(WorldType Type) const;

private:
	struct SBestPlayer
	{
		std::optional<std::pair<std::string, int>> m_Value {};
		int m_Stamp {};
		bool m_Loaded {};
	};
	mutable SBestPlayer m_BestDungeon {};
	mutable SBestPlayer m_BestRhythm {};
	mutable std::map<int, std::vector<std::string>> m_DifficultiesByWorld {};

	// true when the entry has to be read (the load is marked as running)
	bool NeedBest(const SBestPlayer& Cache, const CVoteLoad& Load) const;
	void SetBest(SBestPlayer& Cache, std::optional<std::pair<std::string, int>>&& Value, const CVoteLoad& Load) const;

	const std::optional<std::pair<std::string, int>>& GetBestDungeonPlayer() const;
	const std::optional<std::pair<std::string, int>>& GetBestRhythmPlayer() const;
};
#endif
