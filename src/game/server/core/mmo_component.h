/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_MMO_COMPONENT_H
#define GAME_SERVER_CORE_MMO_COMPONENT_H

#include <engine/server/sql_string_helpers.h>

using namespace sqlstr;
class MmoComponent
{
protected:
	class CGS* m_GameServer;
	class IServer* m_pServer;
	class CMmoController* m_Core;
	friend CMmoController; // provide access for the controller

	CGS* GS() const { return m_GameServer; }
	IServer* Server() const { return m_pServer; }
	CMmoController* Core() const { return m_Core; }

public:
	virtual ~MmoComponent() {}

private:
	virtual void OnInitWorld(const char* pWhereLocalWorld) {};
	virtual void OnInit() {};
	virtual void OnInitAccount(class CPlayer* pPlayer) {};
	virtual void OnTick() {};
	virtual void OnResetClient(int ClientID) {};
	virtual bool OnMessage(int MsgID, void* pRawMsg, int ClientID) { return false; };
	virtual bool OnHandleTile(class CCharacter* pChr, int IndexCollision) { return false; };
	virtual bool OnHandleMenulist(class CPlayer* pPlayer, int Menulist, bool ReplaceMenu) { return false; };
	virtual bool OnHandleVoteCommands(class CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText) { return false; }
	virtual void OnHandleTimePeriod(TIME_PERIOD Period) { return; }
	virtual void OnPlayerHandleTimePeriod(class CPlayer* pPlayer, TIME_PERIOD Period) { return; }
};

#endif