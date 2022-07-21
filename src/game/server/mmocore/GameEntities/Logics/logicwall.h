/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_LOGIC_H
#define GAME_SERVER_ENTITIES_LOGIC_H
#include <game/server/entity.h>

class CPlayer;

class CLogicWallLine : public CEntity
{
	int m_ClientID;
	bool m_Spawned;

public:
	CLogicWallLine(CGameWorld *pGameWorld, vec2 Pos);
	virtual void Snap(int SnappingClient);
	virtual void Tick();
	void Respawn(bool Spawn);
	void SetClientID(int ClientID);
};

class CLogicWall : public CEntity
{
	int m_RespawnTick;
	CLogicWallLine *pLogicWallLine;

public:
	CLogicWall(CGameWorld *pGameWorld, vec2 Pos);
	virtual void Snap(int SnappingClient);
	virtual void Tick();
	void SetDestroy(int Sec);
private:

	CPlayer *FindPlayerAI(float Distance);
};

class CLogicWallFire : public CEntity
{
	vec2 m_Dir;
	CLogicWall *pLogicWall;

public:
	CLogicWallFire(CGameWorld *pGameWorld, vec2 Pos, vec2 Direction, CLogicWall *Eyes);
	virtual void Snap(int SnappingClient);
	virtual void Tick();
};

class CLogicWallWall : public CEntity
{
	int m_Health;
	int m_SaveHealth;
	int m_RespawnTick;

public:
	CLogicWallWall(CGameWorld *pGameWorld, vec2 Pos, int Mode, int Health);
	virtual void Snap(int SnappingClient);
	virtual void Tick();

	void TakeDamage();
	void SetDestroy(int Sec);
	vec2 GetTo() const { return m_PosTo; }
	int GetHealth() const { return m_Health; }
};

class CLogicDoorKey : public CEntity
{
	int m_ItemID;

public:
	CLogicDoorKey(CGameWorld *pGameWorld, vec2 Pos, int ItemID, int Mode);
	virtual void Snap(int SnappingClient);
	virtual void Tick();

};

class CLogicDungeonDoorKey : public CEntity
{
	int m_BotID;
	bool m_OpenedDoor;

public:
	CLogicDungeonDoorKey(CGameWorld *pGameWorld, vec2 Pos, int BotID);
	virtual void Snap(int SnappingClient);
	virtual void Tick();

	bool SyncStateChanges();
	void ResetDoor() { m_OpenedDoor = false; };
};

#endif
