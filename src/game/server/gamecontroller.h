/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class CGS;
class IServer;
class IGameController
{
	CGS* m_pGS {};
	IServer* m_pServer {};

	struct CSpawnEval
	{
		vec2 m_Pos{};
		float m_Score{};
		bool m_Got{};
	};
	std::array<std::vector<vec2>, NUM_SPAWN> m_aaSpawnPoints{};
	void EvaluateSpawnType(CSpawnEval* Pos, int Type, std::pair<vec2, float> LimiterSpread) const;

protected:
	CGS *GS() const { return m_pGS; }
	IServer *Server() const { return m_pServer; }

	int m_GameFlags{};

	void UpdateGameInfo(int ClientID);

public:
	IGameController(class CGS *pGS);
	virtual ~IGameController() = default;

	virtual void OnInit() {};
	virtual void OnCharacterDamage(class CPlayer* pFrom, class CPlayer* pTo, int Damage);
	virtual void OnCharacterDeath(class CPlayer* pVictim, class CPlayer *pKiller, int Weapon);

	virtual bool OnCharacterSpawn(class CCharacter *pChr);
	virtual bool OnCharacterBotSpawn(class CCharacterBotAI *pChr);

	virtual void OnEntity(int Index, vec2 Pos, int Flags);
	virtual void OnEntitySwitch(int Index, vec2 Pos, int Flags, int Number);

	void OnPlayerConnect(class CPlayer *pPlayer);
	void OnPlayerDisconnect(class CPlayer *pPlayer);

	// general
	virtual void Snap();
	virtual void Tick();

	bool CanSpawn(int SpawnType, vec2 *pPos, std::pair<vec2, float> LimiterSpread = std::make_pair(vec2(), -1.f)) const;
	float EvaluateSpawnPos(CSpawnEval* pEval, vec2 Pos) const;
	void DoTeamChange(class CPlayer *pPlayer);
};

#endif
