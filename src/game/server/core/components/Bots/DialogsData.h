/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_DIALOGS_DATA_H
#define GAME_SERVER_COMPONENT_DIALOGS_DATA_H

class CPlayerDialog
{
	friend class CDialogStep;
	class CGS* GS() const;
	class CPlayer* m_pPlayer {};

	int m_BotCID{};
	int m_MobID{};
	int m_BotType{};
	int m_Step{};
	char m_aFormatedText[1024] {};

public:
	CPlayerDialog()
	{
		Clear();
	}

	bool IsActive() const
	{
		return m_BotCID >= MAX_PLAYERS && m_BotCID < MAX_CLIENTS;
	}

	void Init(CPlayer* pPlayer);
	void Start(int BotCID);
	void Next();
	bool CheckActionCompletion() const;
	void End();
	void Tick();


private:
	void PrepareDialog(const class CDialogStep* pDialog, const char* pLeftNickname, const char* pRightNickname);
	const char* GetCurrentText() const { return m_aFormatedText; }
	void ClearText();

	class CDialogStep* GetCurrent() const;
	void ShowCurrentDialog() const;
	void Clear();
};

class CDialogStep
{
	friend class CPlayerDialog;

	int m_BotLeftSideID{};
	int m_BotRightSideID{};
	std::string m_Text{};
	bool m_Request{};
	int64_t m_Flags{};

public:
	void Init(int BotID, const nlohmann::json& JsonDialog);

	const char* GetText() const
	{
		return m_Text.c_str();
	}

	bool IsEmptyDialog() const
	{
		return m_Text.empty();
	}

	bool IsRequestAction() const
	{
		return m_Request;
	}

	int64_t GetFlag() const
	{
		return m_Flags;
	}

private:
	void Show(int ClientID) const;
};

#endif
