/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H
#define GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H

#include <game/voting.h>

class CGS;
class CPlayer;
class CVoteGroupHidden;

enum
{
	FLAG_DISABLED = 0,
	HIDE_DEFAULT_OPEN = 1 << 1,
	HIDE_DEFAULT_CLOSE = 1 << 2,
	HIDE_UNIQUE = 1 << 3,

	BORDER_SIMPLE = 1 << 4,      // example: ╭ │ ╰
	DOUBLE_BORDER = 1 << 5,      // example: ╔ ═ ╚
	BORDER_STRICT = 1 << 6,      // example: ┌ │ └
	BORDER_STRICT_BOLD = 1 << 7, // example: ┏ ┃ ┗
};

class CVoteOption
{
public:
	char m_aDescription[VOTE_DESC_LENGTH] {};
	char m_aCommand[VOTE_CMD_LENGTH] {};
	int m_SettingID { -1 };
	int m_SettingID2 { -1 };
	bool m_Title { false };
	bool m_Line { false };
};

class CVoteGroup
{
	friend class CVoteWrapper;

	std::deque<CVoteOption> m_vpVotelist {};

	CGS* m_pGS {};
	CGS* GS() const { return m_pGS; }

	int m_ButtonSize {};
	int m_GroupHash {};
	int m_Flags {};
	int m_ClientID {};

	CVoteGroup(int ClientID, int Flags);

	bool IsEmpty() const { return m_ButtonSize <= 0; };

	void AddVoteTitleImpl(const char* pCmd, int SettingsID1, int SettingsID2, const char* pText, ...);
	void AddVoteImpl(const char* pCmd, int Settings1, int Settings2, const char* pText, ...);

	void AddLineImpl();
	void AddEmptylineImpl();
	void AddBackpageImpl();
	void AddItemValueImpl(int ItemID);
};

class CVoteWrapper : public MultiworldIdentifiableStaticData<std::map<int, std::deque<CVoteGroup*>>>
{
	CVoteGroup* m_pGroup {};

public:
	CVoteWrapper(int ClientID)
	{
		m_pGroup = new CVoteGroup(ClientID, FLAG_DISABLED);
		m_pData[ClientID].push_back(m_pGroup);
	}

	CVoteWrapper(int ClientID, int Flags)
	{
		m_pGroup = new CVoteGroup(ClientID, Flags);
		m_pData[ClientID].push_back(m_pGroup);
	}

	template<typename ... Args>
	CVoteWrapper(int ClientID, const std::string& TitleText, Args&& ... argsfmt)
	{
		m_pGroup = new CVoteGroup(ClientID, FLAG_DISABLED);
		m_pGroup->AddVoteTitleImpl("null", NOPE, NOPE, TitleText.c_str(), std::forward<Args>(argsfmt)...);
		m_pData[ClientID].push_back(m_pGroup);
	}

	template<typename ... Args>
	CVoteWrapper(int ClientID, int Flags, const std::string& TitleText, Args&& ... argsfmt)
	{
		m_pGroup = new CVoteGroup(ClientID, Flags);
		m_pGroup->AddVoteTitleImpl("null", NOPE, NOPE, TitleText.c_str(), std::forward<Args>(argsfmt)...);
		m_pData[ClientID].push_back(m_pGroup);
	}

	bool IsEmpty() const { return m_pGroup->IsEmpty(); }

	/*
	 * Global static data
	 */
	static void AddLine(int ClientID) noexcept {
		const auto pVoteGroup = new CVoteGroup(ClientID, FLAG_DISABLED);
		pVoteGroup->AddLineImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	static void AddBackpage(int ClientID) noexcept {
		const auto pVoteGroup = new CVoteGroup(ClientID, FLAG_DISABLED);
		pVoteGroup->AddBackpageImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	static void AddEmptyline(int ClientID) noexcept {
		const auto pVoteGroup = new CVoteGroup(ClientID, FLAG_DISABLED);
		pVoteGroup->AddEmptylineImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}

	/*
	 * Tools options group
	 */
	CVoteWrapper& AddIfLine(bool Checker) noexcept {
		if(Checker)
			m_pGroup->AddLineImpl();
		return *this;
	}
	CVoteWrapper& AddLine(){
		return AddIfLine(true);
	}
	CVoteWrapper& AddIfEmptyline(bool Checker) noexcept {
		if(Checker)
			m_pGroup->AddEmptylineImpl();
		return *this;
	}
	CVoteWrapper& AddEmptyline(){
		return AddIfEmptyline(true);
	}
	CVoteWrapper& AddIfBackpage(bool Checker) noexcept {
		if(Checker)
			m_pGroup->AddBackpageImpl();
		return *this;
	}
	CVoteWrapper& AddBackpage() noexcept {
		return AddIfBackpage(true);
	}
	CVoteWrapper& AddIfItemValue(bool Checker, int ItemID = itGold) noexcept {
		if(Checker)
			m_pGroup->AddItemValueImpl(ItemID);
		return *this;
	}
	CVoteWrapper& AddItemValue(int ItemID = itGold) noexcept {
		return AddIfItemValue(ItemID);
	}

	/*
	 * Default group
	 */
	template<typename ... Args>
	CVoteWrapper& AddIf(bool Checker, const std::string& Text, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl("null", NOPE, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIf(bool Checker, int MenuID, const std::string& Text, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl("MENU", MenuID, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIf(bool Checker, int MenuID, int InteractID, const std::string& Text, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl("MENU", MenuID, InteractID, Text.c_str(), std::forward<Args>(argsfmt)...);
		return *this;
	}
	template<typename ... Args>
	CVoteWrapper& Add(const std::string& Text, Args&& ... argsfmt) {
		return AddIf(true, Text, std::forward<Args>(argsfmt)...);
	}
	template<typename ... Args>
	CVoteWrapper& Add(int MenuID, const std::string& Text, Args&& ... argsfmt) {
		return AddIf(true, MenuID, Text, std::forward<Args>(argsfmt)...);
	}
	template<typename ... Args>
	CVoteWrapper& Add(int MenuID, int InteractID, const std::string& Text, Args&& ... argsfmt) {
		return AddIf(true, MenuID, InteractID, Text, std::forward<Args>(argsfmt)...);
	}

	/*
	 * Option group
	 */
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, const std::string& Text, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl(pCmd, NOPE, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, int Settings1, const std::string& Text, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl(pCmd, Settings1, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, int Settings1, int Settings2, const std::string& Text, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl(pCmd, Settings1, Settings2, Text.c_str(), std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddOption(const char* pCmd, const std::string& Text, Args&& ... argsfmt) {
		return AddIfOption(true, pCmd, Text, std::forward<Args>(argsfmt)...);
	}
	template <typename ... Args>
	CVoteWrapper& AddOption(const char* pCmd, int Settings1, const std::string& Text, Args&& ... argsfmt) {
		return AddIfOption(true, pCmd, Settings1, Text, std::forward<Args>(argsfmt)...);
	}
	template <typename ... Args>
	CVoteWrapper& AddOption(const char* pCmd, int Settings1, int Settings2, const std::string& Text, Args&& ... argsfmt) {
		return AddIfOption(true, pCmd, Settings1, Settings2, Text, std::forward<Args>(argsfmt)...);
	}

	// Rebuild votes
	static void RebuildVotes(int ClientID);
	static CVoteOption* GetOptionVoteByAction(int ClientID, const char* pActionName);
};

#endif
