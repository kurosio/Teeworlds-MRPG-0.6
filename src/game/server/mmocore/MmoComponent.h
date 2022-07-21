/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_MMO_COMPONENT_H
#define GAME_SERVER_MMO_COMPONENT_H

#include <engine/server/sql_connect_pool.h>
#include <engine/server/sql_string_helpers.h>

using namespace sqlstr;
class MmoComponent
{
protected:
	class CGS* m_GameServer;
	class IServer* m_pServer;
	class MmoController* m_Job;
	friend MmoController; // provide access for the controller

	CGS* GS() const { return m_GameServer; }
	IServer* Server() const { return m_pServer; }
	MmoController* Job() const { return m_Job; }

public:
	virtual ~MmoComponent() {}

private:
	virtual void OnInitWorld(const char* pWhereLocalWorld) {};
	virtual void OnInit() {};
	virtual void OnInitAccount(class CPlayer* pPlayer) {};
	virtual void OnTick() {};
	virtual void OnResetClient(int ClientID) {};
	virtual bool OnHandleTile(class CCharacter* pChr, int IndexCollision) { return false; };
	virtual bool OnHandleMenulist(class CPlayer* pPlayer, int Menulist, bool ReplaceMenu) { return false; };
	virtual bool OnHandleVoteCommands(class CPlayer* pPlayer, const char* CMD, const int VoteID, const int VoteID2, int Get, const char* GetText) { return false; }
	virtual void OnMessage(int MsgID, void* pRawMsg, int ClientID) {};

	virtual void OnPrepareInformation(class IStorageEngine* pStorage, class CDataFileWriter* pDataFile) {};
};

#endif