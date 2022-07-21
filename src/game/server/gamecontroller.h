/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTROLLER_H
#define GAME_SERVER_GAMECONTROLLER_H

/*
	Class: Game Controller
		Controls the main game logic. Keeping track of team and player score,
		winning conditions and specific game logic.
*/
class IGameController
{
	class CGS *m_pGS;
	class IServer *m_pServer;

	// spawn
	struct CSpawnEval
	{
		CSpawnEval()
		{
			m_Got = false;
			m_FriendlyTeam = -1;
			m_Pos = vec2(100,100);
			m_Score = 0;
		}

		vec2 m_Pos;
		bool m_Got;
		int m_FriendlyTeam;
		float m_Score;
	};
	vec2 m_aaSpawnPoints[SPAWN_NUM][64];
	int m_aNumSpawnPoints[SPAWN_NUM];

	float EvaluateSpawnPos(CSpawnEval *pEval, vec2 Pos) const;
	void EvaluateSpawnType(CSpawnEval *pEval, int Type, vec2 BotPos) const;

protected:
	CGS *GS() const { return m_pGS; }
	IServer *Server() const { return m_pServer; }

	// info
	int m_GameFlags;

	void UpdateGameInfo(int ClientID);

public:
	IGameController(class CGS *pGS);
	virtual ~IGameController() {};

	virtual void OnCharacterDamage(class CPlayer* pFrom, class CPlayer* pTo, int Damage);
	virtual void OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual bool OnCharacterSpawn(class CCharacter *pChr);
	virtual bool OnEntity(int Index, vec2 Pos);

	void OnPlayerConnect(class CPlayer *pPlayer);
	void OnPlayerDisconnect(class CPlayer *pPlayer);
	void OnPlayerInfoChange(class CPlayer *pPlayer, int WorldID);
	void OnReset();

	virtual void CreateLogic(int Type, int Mode, vec2 Pos, int Health) = 0;

	// general
	virtual void Snap();
	virtual void Tick();

	bool CanSpawn(int SpawnType, vec2 *pPos, vec2 BotPos) const;
	void DoTeamChange(class CPlayer *pPlayer, bool DoChatMsg=true);

};

#endif
