/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H
#define GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H

#include <game/voting.h>

class CGS;
class CPlayer;
class CVoteGroupHidden;

typedef void (*VoteOptionCallbackImpl)(CPlayer*, int, std::string, void*);
typedef struct { VoteOptionCallbackImpl m_Impl; void* m_pData; } VoteOptionCallback;

enum
{
	VWF_DISABLED          = 0, // regular title group

	// settings
	VWF_SEPARATE          = 1 << 1, // separates the end and the beginning of the new group by a line

	// styles
	VWF_STYLE_SIMPLE      = 1 << 2, // example: ╭ │ ╰
	VWF_STYLE_DOUBLE      = 1 << 3, // example: ╔ ═ ╚
	VWF_STYLE_STRICT      = 1 << 4, // example: ┌ │ └
	VWF_STYLE_STRICT_BOLD = 1 << 5, // example: ┏ ┃ ┗

	// hidden
	VWF_OPEN              = 1 << 6, // default open group
	VWF_CLOSED            = 1 << 7, // default close group
	VWF_UNIQUE            = 1 << 8, // default close group toggle unique groups 
	VWF_SEPARATE_OPEN     = VWF_OPEN | VWF_SEPARATE, // default open group with separate
	VWF_SEPARATE_CLOSED   = VWF_CLOSED | VWF_SEPARATE, // default close group with separate
	VWF_SEPARATE_UNIQUE   = VWF_UNIQUE | VWF_SEPARATE, // default close group with separate
};

class CVoteOption
{
public:
	char m_aDescription[VOTE_DESC_LENGTH] {};
	char m_aCommand[VOTE_CMD_LENGTH] {};
	int m_Depth {};
	int m_SettingID { -1 };
	int m_SettingID2 { -1 };
	bool m_Line { false };
	VoteOptionCallback m_Callback {};
};

class CVoteGroup
{
	friend class CVoteWrapper;

	std::deque<CVoteOption> m_vpVotelist {};

	CGS* m_pGS {};
	CPlayer* m_pPlayer {};
	CGS* GS() const { return m_pGS; }

	bool m_TitleIsSet {};
	int m_GroupSize {};
	int m_HiddenID {};
	int m_Flags {};
	int m_ClientID {};
	int m_CurrentDepth {};

	CVoteGroup(int ClientID, int Flags);

	bool IsEmpty() const { return m_GroupSize <= 0; }
	bool IsTitleSet() const { return m_TitleIsSet; }
	bool IsHidden() const;

	void SetVoteTitleImpl(const char* pCmd, int SettingsID1, int SettingsID2, const char* pText, ...);
	void AddVoteImpl(const char* pCmd, int Settings1, int Settings2, const char* pText, ...);
	void SetLastVoteCallback(const VoteOptionCallbackImpl& CallbackImpl, void* pUser) { m_vpVotelist.back().m_Callback = { CallbackImpl, pUser }; }

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
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		m_pData[ClientID].push_back(m_pGroup);
	}

	template <typename T = int>
	CVoteWrapper(int ClientID, T Flags)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, Flags);
		m_pData[ClientID].push_back(m_pGroup);
	}

	template<typename ... Args>
	CVoteWrapper(int ClientID, const char* pTitle, Args&& ... argsfmt)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, pTitle, std::forward<Args>(argsfmt)...);
		m_pData[ClientID].push_back(m_pGroup);
	}

	template<typename ... Args>
	CVoteWrapper(int ClientID, int Flags, const char* pTitle, Args&& ... argsfmt)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		m_pGroup = new CVoteGroup(ClientID, Flags);
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, pTitle, std::forward<Args>(argsfmt)...);
		m_pData[ClientID].push_back(m_pGroup);
	}

	/*
	 * Post initilize title
	 */
	template<typename ... Args>
	CVoteWrapper& SetTitle(const char* pTitle, Args&& ... argsfmt) noexcept {
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, pTitle, std::forward<Args>(argsfmt)...);
		return *this;
	}
	template<typename ... Args>
	CVoteWrapper& SetTitle(int Flags, const char* pTitle, Args&& ... argsfmt) noexcept {
		m_pGroup->m_Flags = Flags;
		m_pGroup->SetVoteTitleImpl("null", NOPE, NOPE, pTitle, std::forward<Args>(argsfmt)...);
		return *this;
	}

	bool IsEmpty() const { return m_pGroup->IsEmpty(); }
	bool IsTittleSet() const { return m_pGroup->IsTitleSet(); }

	/*
	 * Global static data
	 */
	static void AddLine(int ClientID) noexcept {
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddLineImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	static void AddBackpage(int ClientID) noexcept {
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddBackpageImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	static void AddEmptyline(int ClientID) noexcept {
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddEmptylineImpl();
		m_pData[ClientID].push_back(pVoteGroup);
	}
	static void AddItemValue(int ClientID, int ItemID) noexcept
	{
		const auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
		pVoteGroup->AddItemValueImpl(ItemID);
		m_pData[ClientID].push_back(pVoteGroup);
	}

	/*
	 * Group level
	 */
	CVoteWrapper& BeginDepthList() noexcept {
		m_pGroup->m_CurrentDepth++;
		return *this;
	}
	CVoteWrapper& EndDepthList() noexcept {
		m_pGroup->m_CurrentDepth--;
		return *this;
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
	CVoteWrapper& AddItemValue(int ItemID) noexcept {
		m_pGroup->AddItemValueImpl(ItemID);
		return *this;
	}

	/*
	 * Default group
	 */
	template<typename ... Args>
	CVoteWrapper& AddIf(bool Checker, const char* pText, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl("null", NOPE, NOPE, pText, std::forward<Args>(argsfmt)...);
		return *this;
	}

	template<typename ... Args>
	CVoteWrapper& Add(const char* pText, Args&& ... argsfmt) {
		return AddIf(true, pText, std::forward<Args>(argsfmt)...);
	}

	/*
	 * Menu group
	 */
	template <typename ... Args>
	CVoteWrapper& AddIfMenu(bool Checker, int MenuID, const char* pText, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl("MENU", MenuID, NOPE, pText, std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfMenu(bool Checker, int MenuID, int GroupInteractID, const char* pText, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl("MENU", MenuID, GroupInteractID, pText, std::forward<Args>(argsfmt)...);
		return *this;
	}
	template<typename ... Args>
	CVoteWrapper& AddMenu(int MenuID, const char* pText, Args&& ... argsfmt) {
		return AddIfMenu(true, MenuID, pText, std::forward<Args>(argsfmt)...);
	}
	template<typename ... Args>
	CVoteWrapper& AddMenu(int MenuID, int GroupInteractID, const char* pText, Args&& ... argsfmt) {
		return AddIfMenu(true, MenuID, GroupInteractID, pText, std::forward<Args>(argsfmt)...);
	}

	/*
	 * Option group
	 */
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, const char* pText, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl(pCmd, NOPE, NOPE, pText, std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, int Settings1, const char* pText, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl(pCmd, Settings1, NOPE, pText, std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddIfOption(bool Checker, const char* pCmd, int Settings1, int Settings2, const char* pText, Args&& ... argsfmt) {
		if(Checker)
			m_pGroup->AddVoteImpl(pCmd, Settings1, Settings2, pText, std::forward<Args>(argsfmt)...);
		return *this;
	}
	template <typename ... Args>
	CVoteWrapper& AddOption(const char* pCmd, const char* pText, Args&& ... argsfmt) {
		return AddIfOption(true, pCmd, pText, std::forward<Args>(argsfmt)...);
	}
	template <typename ... Args>
	CVoteWrapper& AddOption(const char* pCmd, int Settings1, const char* pText, Args&& ... argsfmt) {
		return AddIfOption(true, pCmd, Settings1, pText, std::forward<Args>(argsfmt)...);
	}
	template <typename ... Args>
	CVoteWrapper& AddOption(const char* pCmd, int Settings1, int Settings2, const char* pText, Args&& ... argsfmt) {
		return AddIfOption(true, pCmd, Settings1, Settings2, pText, std::forward<Args>(argsfmt)...);
	}

	/*
	 * Option callback group
	 */
	template<typename ... Args>
	CVoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl, const char* pText, Args&& ... argsfmt) {
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", NOPE, NOPE, pText, std::forward<Args>(argsfmt)...);
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}
	template<typename ... Args>
	CVoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl, int Settings1, const char* pText, Args&& ... argsfmt) {
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", Settings1, NOPE, pText, std::forward<Args>(argsfmt)...);
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}
	template<typename ... Args>
	CVoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl, int Settings1, int Settings2, const char* pText, Args&& ... argsfmt) {
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", Settings1, Settings2, pText, std::forward<Args>(argsfmt)...);
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}

	// Rebuild votes
	static void RebuildVotes(int ClientID);
	static CVoteOption* GetOptionVoteByAction(int ClientID, const char* pActionName);
};

class CVotePlayerData
{
	friend class CVoteGroup;
	friend class CVoteWrapper;

	struct VoteGroupHidden
	{
		bool m_Value {};
		int m_Flag {};
	};

	CGS* m_pGS {};
	CPlayer* m_pPlayer {};
	int m_LastMenuID{};
	int m_CurrentMenuID { };
	int m_TempMenuInteger {};
	std::thread m_VoteUpdater {};
	enum class STATE_UPDATER { WAITING, RUNNING, DONE };
	std::atomic<STATE_UPDATER> m_VoteUpdaterStatus{ STATE_UPDATER::WAITING };
	ska::unordered_map<int, ska::unordered_map<int, VoteGroupHidden>> m_aHiddenGroup{};

	VoteGroupHidden* EmplaceHidden(int ID, int Type);
	VoteGroupHidden* GetHidden(int ID);
	void ResetHidden(int MenuID);
	void ResetHidden() { ResetHidden(m_CurrentMenuID); }
	static void ThreadVoteUpdater(CVotePlayerData* pData);

public:
	CVotePlayerData()
	{
		m_CurrentMenuID = MENU_MAIN;
	}

	~CVotePlayerData()
	{
		if(m_VoteUpdater.joinable())
			m_VoteUpdater.join();

		ClearVotes();
		m_pGS = nullptr;
		m_pPlayer = nullptr;
		m_aHiddenGroup.clear();
	}

	void Initilize(CGS* pGS, CPlayer* pPlayer)
	{
		m_pGS = pGS;
		m_pPlayer = pPlayer;
	}

	void ApplyVoteUpdaterData();
	void UpdateVotes(int MenuID);
	void UpdateVotesIf(int MenuID);
	void UpdateCurrentVotes() { UpdateVotes(m_CurrentMenuID); }
	void ClearVotes() const;

	void SetCurrentMenuID(int MenuID) { m_CurrentMenuID = MenuID; }
	int GetCurrentMenuID() const { return m_CurrentMenuID; }
	int GetMenuTemporaryInteger() const { return m_TempMenuInteger; }

	void SetLastMenuID(int MenuID) { m_LastMenuID = MenuID; }
	int GetLastMenuID() const { return m_LastMenuID; }

	bool ParsingDefaultSystemCommands(const char* CMD, const int VoteID, const int VoteID2, int Get, const char* Text);
};

#endif
