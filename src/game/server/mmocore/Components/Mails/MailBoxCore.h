/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_MAIL_CORE_H
#define GAME_SERVER_COMPONENT_MAIL_CORE_H
#include <game/server/mmocore/MmoComponent.h>

class CMailBoxCore : public MmoComponent
{
	bool OnHandleVoteCommands(CPlayer* pPlayer, const char* CMD, int VoteID, int VoteID2, int Get, const char* GetText) override;
	void OnMessage(int MsgID, void* pRawMsg, int ClientID) override;

public:
	int GetMailLettersSize(int AccountID);
	void GetInformationInbox(CPlayer *pPlayer);
	void SendInbox(const char* pFrom, int AccountID, const char* pName, const char* pDesc, int ItemID = -1, int Value = -1, int Enchant = -1);
	bool SendInbox(const char* pFrom, const char* pNickname, const char* pName, const char* pDesc, int ItemID = -1, int Value = -1, int Enchant = -1);

private:
	void DeleteMailLetter(int MailLetterID);
	void AcceptMailLetter(CPlayer* pPlayer, int MailLetterID);
	void SetReadState(int MailLetterID, bool State);
	void SendClientListMail(CPlayer* pPlayer);
};

#endif