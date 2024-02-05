/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H
#define GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H

#include <game/voting.h>

class CGS;
class CPlayer;
class CVoteWrapper;
class CVoteGroupHidden;

enum
{
	HIDE_DISABLE = 1 << 0,
	HIDE_DEFAULT_OPEN = 1 << 1,
	HIDE_DEFAULT_CLOSE = 1 << 2,
	HIDE_UNIQUE = 1 << 3,

	BORDER_SIMPLE = 1 << 4,      // example: ╭ │ ╰
	DOUBLE_BORDER = 1 << 5,      // example: ╔ ═ ╚
	BORDER_STRICT = 1 << 6,      // example: ┌ │ └
	BORDER_STRICT_BOLD = 1 << 7, // example: ┏ ┃ ┗
};

class CVoteWrapper : public MultiworldIdentifiableStaticData<std::map<int, std::deque<CVoteOption>>>
{
	std::deque<CVoteOption*> m_vpVoteGroup {};

	CGS* m_pGS {};
	CGS* GS() const { return m_pGS; }

	int m_ButtonSize{};
	int m_HideHash {};
	int m_Flags {};
	int m_ClientID {};

public:
	// CVoteWrapper constructor with ClientID parameter
	CVoteWrapper(int ClientID) : m_ClientID(ClientID) {
		InitWrapper();
	}

	// CVoteWrapper constructor with ClientID and Flags parameters
	CVoteWrapper(int ClientID, int Flags) : m_Flags(Flags), m_ClientID(ClientID) {
		InitWrapper();
		AddEmptyline();
	}

	// CVoteWrapper constructor with ClientID, Flags, TitleText, and argsfmt parameters
	template<typename ... Args>
	CVoteWrapper(int ClientID, int Flags, const std::string& TitleText, Args&& ... argsfmt) : m_Flags(Flags), m_ClientID(ClientID) {
		InitWrapper();
		AddVoteImpl("null", NOPE, NOPE, TitleText.c_str(), std::forward<Args>(argsfmt)...);
	}

	// CVoteWrapper constructor with ClientID, TitleText, and argsfmt parameters
	template<typename ... Args>
	CVoteWrapper(int ClientID, const std::string& TitleText, Args&& ... argsfmt) : m_ClientID(ClientID) {
		InitWrapper();
		AddVoteImpl("null", NOPE, NOPE, TitleText.c_str(), std::forward<Args>(argsfmt)...);
	}

	// CVoteWrapper destructor
	~CVoteWrapper();

	bool IsEmpty() const;

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
	CVoteWrapper& AddItemValue(int ItemID = itGold) { return AddIfItemValue(true, ItemID); }
	CVoteWrapper& AddEmptyline() { return AddIfEmptyline(true); }

	template<typename ... Args>
	CVoteWrapper& AddIf(bool Checker, const std::string& Text, Args&& ... argsfmt) {
		return Checker ? AddVoteImpl("null", NOPE, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...) : *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIf(bool Checker, int MenuID, const std::string& Text, Args&& ... argsfmt) {
		return Checker ? AddVoteImpl("MENU", MenuID, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...) : *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIf(bool Checker, int MenuID, int InteractID, const std::string& Text, Args&& ... argsfmt) {
		return Checker ? AddVoteImpl("MENU", MenuID, InteractID, Text.c_str(), std::forward<Args>(argsfmt)...) : *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, const std::string& Text, Args&& ... argsfmt) {
		return Checker ? AddVoteImpl(pCmd, NOPE, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...) : *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, int Settings1, const std::string& Text, Args&& ... argsfmt) {
		return Checker ? AddVoteImpl(pCmd, Settings1, NOPE, Text.c_str(), std::forward<Args>(argsfmt)...) : *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, int Settings1, int Settings2, const std::string& Text, Args&& ... argsfmt) {
		return Checker ? AddVoteImpl(pCmd, Settings1, Settings2, Text.c_str(), std::forward<Args>(argsfmt)...) : *this;
	}
	CVoteWrapper& AddIfEmptyline(bool Checker) {
		return Checker ? AddVoteImpl("null", NOPE, NOPE, "\0") : *this;
	}
	CVoteWrapper& AddIfItemValue(bool Checker, int ItemID = itGold);

	static void AddEmptyline(int ClientID) {
		CVoteWrapper(ClientID).AddEmptyline();
	}
	static void AddBackpage(int ClientID) {
		CVoteWrapper(ClientID).Add("").AddOption("Back", "\u226A Backpage \u226A");
	}

private:
	void InitWrapper();
	void RebuildByFlags() const;
	CVoteWrapper& AddVoteImpl(const char* pCmd, int TempInt, int TempInt2, const char* pText, ...);
	void HashHide(const char* pStr)
	{
		m_HideHash = 0;
		while(*pStr)
			m_HideHash = m_HideHash << 1 ^ *pStr++;
	}
};

#endif
