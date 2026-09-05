/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H
#define GAME_SERVER_CORE_UTILITIES_VOTE_WRAPPER_H

#include <base/types.h>
#include <stack>

// forward declarations
class CGS;
class CPlayer;

using VoteOptionCallbackImpl = void(*)(CPlayer*, int, std::string, void*);

struct VoteOptionCallback
{
	VoteOptionCallbackImpl m_Impl{ nullptr };
	void* m_pData{ nullptr };
};

enum VoteDepthListStyles : int
{
	DEPTH_LIST_STYLE_DEFAULT = 0,
	DEPTH_LIST_STYLE_ROMAN,
	DEPTH_LIST_STYLE_BOLD,
	DEPTH_LIST_STYLE_CIRCLE,
	NUM_DEPTH_LIST_STYLES,
};

enum VoteDepthListStylesLevels : int
{
	DEPTH_LVL1 = 0,
	DEPTH_LVL2,
	DEPTH_LVL3,
	DEPTH_LVL4,
	DEPTH_LVL5,
};

enum VoteWrapperFlags : int
{
	VWF_DISABLED = 0,
	VWF_SEPARATE = 1 << 1,
	VWF_ALIGN_TITLE = 1 << 2,
	VWF_STYLE_SIMPLE = 1 << 3,
	VWF_STYLE_DOUBLE = 1 << 4,
	VWF_STYLE_STRICT = 1 << 5,
	VWF_STYLE_STRICT_BOLD = 1 << 6,
	VWF_OPEN = 1 << 7,
	VWF_CLOSED = 1 << 8,
	VWF_UNIQUE = 1 << 9,
	VWF_SEPARATE_OPEN = VWF_OPEN | VWF_SEPARATE,
	VWF_SEPARATE_CLOSED = VWF_CLOSED | VWF_SEPARATE,
	VWF_SEPARATE_UNIQUE = VWF_UNIQUE | VWF_SEPARATE,

	VWF_STYLE_MASK = VWF_STYLE_SIMPLE | VWF_STYLE_DOUBLE | VWF_STYLE_STRICT | VWF_STYLE_STRICT_BOLD,
	VWF_HIDABLE_MASK = VWF_CLOSED | VWF_OPEN | VWF_UNIQUE,
};

class CVoteOption
{
public:
	char m_aDescription[VOTE_DESC_LENGTH]{};
	char m_aCommand[VOTE_CMD_LENGTH]{};
	int m_Depth{};
	std::vector<std::any> m_Extras;
	int m_SortPriority{ NOPE };
	bool m_Line{};
	bool m_Title{};
	VoteOptionCallback m_Callback{};
};

class CVoteGroup
{
	friend class VoteWrapper;

	struct NumeralDepth
	{
		int m_Value{};
		int m_Style{};
	};

	std::map<int, NumeralDepth> m_vDepthNumeral;
	std::deque<CVoteOption> m_vpVotelist;
	CGS* m_pGS{ nullptr };
	CPlayer* m_pPlayer{ nullptr };

	int m_ClientID{ -1 };
	int m_CurrentDepth{};
	int m_GroupSize{};
	int m_HiddenID{};
	int m_Flags{};
	bool m_HasTitle{};
	bool m_NextMarkedListItem{};

	CGS* GS() const noexcept { return m_pGS; }

	CVoteGroup(int ClientID, int Flags);

	void SetNumeralDepthStyles(std::initializer_list<std::pair<int, int>> vNumeralFlags);

	[[nodiscard]] int NextPos() const noexcept { return m_GroupSize + 1; }
	[[nodiscard]] bool IsEmpty() const noexcept { return m_GroupSize <= 0; }
	[[nodiscard]] bool HasTitle() const noexcept { return m_HasTitle; }
	[[nodiscard]] bool IsHidden() const noexcept;

	void SetVoteTitleImpl(const char* pCmd, std::vector<std::any> Extras, const char* pText);
	void AddVoteImpl(const char* pCmd, std::vector<std::any> Extras, const char* pText);

	void SetLastVoteCallback(const VoteOptionCallbackImpl& CallbackImpl, void* pUser) noexcept
	{
		if (!m_vpVotelist.empty())
			m_vpVotelist.back().m_Callback = { CallbackImpl, pUser };
	}

	void Reformat(std::string& Buffer);

	void AddLineImpl();
	void AddEmptylineImpl();
	void AddBackpageImpl();
	void AddItemValueImpl(int ItemID);

	template<typename F> requires std::predicate<F, const CVoteOption&, const CVoteOption&>
	void Sort(F&& Comparator)
	{
		if (m_vpVotelist.empty())
			return;
		const auto Begin = m_vpVotelist.begin() + (m_HasTitle ? 1 : 0);
		std::sort(Begin, m_vpVotelist.end(), std::forward<F>(Comparator));
	}

	void SetLastVoteSortPriority(int Priority) noexcept
	{
		if (!m_vpVotelist.empty())
			m_vpVotelist.back().m_SortPriority = Priority;
	}
};

using CVoteGroupPtr = std::unique_ptr<CVoteGroup>;
#define FMT_LOCALIZE_STR(clientid, text, args) fmt_localize(clientid, text, args).c_str()
class VoteWrapper : public MultiworldIdentifiableData<std::map<int, std::deque<CVoteGroupPtr>>>
{
	CVoteGroup* m_pGroup{ nullptr };

	static CVoteGroup* CreateGroup(int ClientID, int Flags)
	{
		dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "Invalid ClientID");
		auto pGroup = std::unique_ptr<CVoteGroup>(new CVoteGroup(ClientID, Flags));
		auto* pRaw = pGroup.get();
		m_pData[ClientID].push_back(std::move(pGroup));
		return pRaw;
	}

public:
	explicit VoteWrapper(int ClientID)
		: m_pGroup(CreateGroup(ClientID, VWF_DISABLED)) {
	}

	VoteWrapper(int ClientID, int Flags)
		: m_pGroup(CreateGroup(ClientID, Flags)) {
	}

	template<typename ... Ts>
	VoteWrapper(int ClientID, const char* pTitle, const Ts&... args)
		: m_pGroup(CreateGroup(ClientID, VWF_DISABLED))
	{
		m_pGroup->SetVoteTitleImpl("null", {}, FMT_LOCALIZE_STR(ClientID, pTitle, args...));
	}

	template<typename ... Ts>
	VoteWrapper(int ClientID, int Flags, const char* pTitle, const Ts&... args)
		: m_pGroup(CreateGroup(ClientID, Flags))
	{
		m_pGroup->SetVoteTitleImpl("null", {}, FMT_LOCALIZE_STR(ClientID, pTitle, args...));
	}

	[[nodiscard]] int NextPos() const noexcept { return m_pGroup->NextPos(); }
	[[nodiscard]] bool IsEmpty() const noexcept { return m_pGroup->IsEmpty(); }
	[[nodiscard]] bool IsTitleSet() const noexcept { return m_pGroup->HasTitle(); }

	template<typename ... Ts>
	VoteWrapper& SetTitle(const char* pTitle, const Ts&... args)
	{
		m_pGroup->SetVoteTitleImpl("null", {}, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pTitle, args...));
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& SetTitle(int Flags, const char* pTitle, const Ts&... args)
	{
		m_pGroup->m_Flags = Flags;
		m_pGroup->SetVoteTitleImpl("null", {}, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pTitle, args...));
		return *this;
	}

	void ReinitNumeralDepthStyles(std::initializer_list<std::pair<int, int>> vNumeralFlags) const
	{
		dbg_assert(m_pGroup != nullptr, "For initialize depth, first needed to initialize vote wrapper");
		m_pGroup->SetNumeralDepthStyles(vNumeralFlags);
	}

	VoteWrapper& MarkList() noexcept { m_pGroup->m_NextMarkedListItem = true; return *this; }
	VoteWrapper& BeginDepth() noexcept { ++m_pGroup->m_CurrentDepth; return *this; }
	VoteWrapper& EndDepth() noexcept { --m_pGroup->m_CurrentDepth; return *this; }
	VoteWrapper& AddLine() noexcept { m_pGroup->AddLineImpl(); return *this; }
	VoteWrapper& AddEmptyline() noexcept { m_pGroup->AddEmptylineImpl(); return *this; }
	VoteWrapper& AddItemValue(int ItemID) { m_pGroup->AddItemValueImpl(ItemID); return *this; }

	template<typename ... Ts>
	VoteWrapper& Add(const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("null", {}, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& AddMenu(int MenuID, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("MENU", MakeAnyList(MenuID), FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& AddMenu(int MenuID, int GroupID, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("MENU", MakeAnyList(MenuID, GroupID), FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& AddOption(const char* pCmd, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl(pCmd, {}, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& AddOption(const char* pCmd, int Extra, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl(pCmd, MakeAnyList(Extra), FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& AddOption(const char* pCmd, std::vector<std::any> Extras, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl(pCmd, std::move(Extras), FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl,
		const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", {}, FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}

	template<typename ... Ts>
	VoteWrapper& AddOptionCallback(void* pUser, const VoteOptionCallbackImpl& CallbackImpl,
		std::vector<std::any> Extras, const char* pText, const Ts&... args)
	{
		m_pGroup->AddVoteImpl("CALLBACK_IMPL", std::move(Extras),
			FMT_LOCALIZE_STR(m_pGroup->m_ClientID, pText, args...));
		m_pGroup->SetLastVoteCallback(CallbackImpl, pUser);
		return *this;
	}

	static void AddLine(int ClientID) { CreateGroup(ClientID, VWF_DISABLED)->AddLineImpl(); }
	static void AddBackpage(int ClientID) { CreateGroup(ClientID, VWF_DISABLED)->AddBackpageImpl(); }
	static void AddEmptyline(int ClientID) { CreateGroup(ClientID, VWF_DISABLED)->AddEmptylineImpl(); }
	static void AddItemValue(int ClientID, int ItemID)
	{
		CreateGroup(ClientID, VWF_DISABLED)->AddItemValueImpl(ItemID);
	}

	template<typename F> requires std::predicate<F, const CVoteOption&, const CVoteOption&>
	VoteWrapper& Sort(F&& Comparator)
	{
		if (m_pGroup)
			m_pGroup->Sort(std::forward<F>(Comparator));
		return *this;
	}

	VoteWrapper& SetSortPriority(int Priority) noexcept
	{
		if (m_pGroup)
			m_pGroup->SetLastVoteSortPriority(Priority);
		return *this;
	}

	static void RebuildVotes(int ClientID);
	static CVoteOption* GetOptionVoteByAction(int ClientID, const char* pActionName);
};
#undef FMT_LOCALIZE_STR

class CVotePlayerData
{
	friend class CVoteGroup;
	friend class VoteWrapper;

	struct VoteGroupHidden
	{
		bool m_State{};
		int m_Flag{};
	};

	enum class STATE_UPDATER : int { WAITING, RUNNING, DONE };

	CGS* m_pGS{};
	CPlayer* m_pPlayer{};
	int m_LastMenuID{ MENU_MAIN };
	int m_CurrentMenuID{ MENU_MAIN };
	std::optional<int> m_ExtraID{};

	std::thread m_VoteUpdater{};
	std::atomic<bool> m_ShuttingDown{ false };
	std::atomic<STATE_UPDATER> m_VoteUpdaterStatus{ STATE_UPDATER::WAITING };

	mystd::string_mapper<int> m_StringMapper{};
	std::unordered_map<int, std::unordered_map<int, VoteGroupHidden>> m_aHiddenGroup{};
	std::unordered_map<int, std::stack<std::optional<int>>> m_aExtraIDHistory{};

	VoteGroupHidden* EmplaceHidden(int ID, int Type);
	VoteGroupHidden* GetHidden(int ID);
	void ResetHidden(int MenuID);
	static void ThreadVoteUpdater(CVotePlayerData* pData);

public:
	CVotePlayerData() = default;

	~CVotePlayerData()
	{
		m_ShuttingDown.store(true, std::memory_order_release);
		if (m_VoteUpdater.joinable())
			m_VoteUpdater.join();

		ClearVotes();
		m_pGS = nullptr;
		m_pPlayer = nullptr;
		m_aHiddenGroup.clear();
		m_StringMapper.clear();
	}

	CVotePlayerData(const CVotePlayerData&) = delete;
	CVotePlayerData& operator=(const CVotePlayerData&) = delete;

	void Init(CGS* pGS, CPlayer* pPlayer) noexcept
	{
		m_pGS = pGS;
		m_pPlayer = pPlayer;
	}

	void ApplyVoteUpdaterData();
	void UpdateVotes(int MenuID);
	void UpdateVotesIf(int MenuID);
	void UpdateCurrentVotes() { UpdateVotes(m_CurrentMenuID); }
	void ClearVotes() const;
	void ResetHidden() { ResetHidden(m_CurrentMenuID); }
	void ResetExtraID() noexcept { m_ExtraID.reset(); }

	[[nodiscard]] mystd::string_mapper<int>& GetStringMapper() noexcept { return m_StringMapper; }

	void SetCurrentMenuID(int MenuID) noexcept { m_CurrentMenuID = MenuID; }
	[[nodiscard]] int GetCurrentMenuID() const noexcept { return m_CurrentMenuID; }
	[[nodiscard]] std::optional<int> GetExtraID() const noexcept { return m_ExtraID; }

	void SetLastMenuID(int MenuID) noexcept { m_LastMenuID = MenuID; }
	[[nodiscard]] int GetLastMenuID() const noexcept { return m_LastMenuID; }

	void PushExtraID(int MenuID, std::optional<int> ExtraID);
	std::optional<int> PopExtraID(int MenuID);
	[[nodiscard]] std::optional<int> PeekExtraID(int MenuID) const;
	[[nodiscard]] bool HasExtraIDHistory(int MenuID) const;
	void ClearExtraIDHistory(int MenuID);

	bool DefaultVoteCommands(const char* pCmd, std::vector<std::any> Extras, int ReasonNumber, const char* pReason);
};

#endif