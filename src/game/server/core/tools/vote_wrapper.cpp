#include "vote_wrapper.h"

#include <game/server/gamecontext.h>
#include <generated/server_data.h>

namespace
{
	// default line for vote options
	constexpr const char* VOTE_LINE_DEFAULT =
		"\u257E\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500"
		"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u257C";

	// styles of numerals for depth levels
	constexpr int MAX_NUMBER = 9;
	constexpr std::array<std::array<const char*, MAX_NUMBER>, NUM_DEPTH_LIST_STYLES> NUMERAL_STYLES = { {
		{ "1. ",       "2. ",       "3. ",       "4. ",       "5. ",       "6. ",       "7. ",       "8. ",       "9. "       },
		{ "\u2160. ",  "\u2161. ",  "\u2162. ",  "\u2163. ",  "\u2164. ",  "\u2165. ",  "\u2166. ",  "\u2167. ",  "\u2168. "  },
		{ "\uFF11. ",  "\uFF12. ",  "\uFF13. ",  "\uFF14. ",  "\uFF15. ",  "\uFF16. ",  "\uFF17. ",  "\uFF18. ",  "\uFF19. "  },
		{ "\u24F5. ",  "\u24F6. ",  "\u24F7. ",  "\u24F8. ",  "\u24F9. ",  "\u24FA. ",  "\u24FB. ",  "\u24FC. ",  "\u24FD. "  }
	} };

	[[nodiscard]] const char* GetNumeralStyle(int Number, int Style) noexcept
	{
		if (Style < 0 || Style >= NUM_DEPTH_LIST_STYLES)
			return "";
		return NUMERAL_STYLES[Style][std::clamp(Number, 0, MAX_NUMBER - 1)];
	}

	// styles of borders for vote options
	enum class BorderType : int { Begin = 0, Middle, MiddleOption, Level, End, COUNT };

	using BorderSet = std::array<const char*, static_cast<size_t>(BorderType::COUNT)>;
	constexpr BorderSet SIMPLE_BORDERS{ "\u256D", "\u2502", "\u251C", "\u2508", "\u2570" };
	constexpr BorderSet DOUBLE_BORDERS{ "\u2554", "\u2551", "\u2560", "\u2550", "\u255A" };
	constexpr BorderSet STRICT_BORDERS{ "\u250C", "\u2502", "\u251C", "\u2508", "\u2514" };
	constexpr BorderSet STRICT_BOLD_BORDERS{ "\u250F", "\u2503", "\u2523", "\u2509", "\u2517" };

	[[nodiscard]] const char* GetBorderStyle(BorderType Border, int Flags) noexcept
	{
		const auto idx = static_cast<size_t>(Border);
		if (Flags & VWF_STYLE_SIMPLE)      return SIMPLE_BORDERS[idx];
		if (Flags & VWF_STYLE_DOUBLE)      return DOUBLE_BORDERS[idx];
		if (Flags & VWF_STYLE_STRICT)      return STRICT_BORDERS[idx];
		if (Flags & VWF_STYLE_STRICT_BOLD) return STRICT_BOLD_BORDERS[idx];
		return "";
	}

	// check for need for transliteration based on language code
	[[nodiscard]] bool NeedsTransliteration(std::string_view lang) noexcept
	{
		return lang == "ru" || lang == "uk";
	}

	// safely copies a string to a destination buffer, ensuring null-termination
	inline void SafeCopy(char* pDest, const char* pSrc, size_t Size) noexcept
	{
		if (pSrc)
			str_copy(pDest, pSrc, Size);
		else
			pDest[0] = '\0';
	}
}

// =====================================================================
// CVoteGroup
// =====================================================================

CVoteGroup::CVoteGroup(int ClientID, int Flags)
	: m_pGS(static_cast<CGS*>(Instance::GameServerPlayer(ClientID)))
	, m_Flags(Flags)
	, m_ClientID(ClientID)
{
	dbg_assert(m_pGS != nullptr, "GameServer is null");
	m_pPlayer = m_pGS->GetPlayer(ClientID);
	dbg_assert(m_pPlayer != nullptr, "player is null");

	m_HiddenID = static_cast<int>(VoteWrapper::Data()[ClientID].size());

	// default numeral styles for depth levels
	m_vDepthNumeral[DEPTH_LVL1].m_Style = DEPTH_LIST_STYLE_ROMAN;
	m_vDepthNumeral[DEPTH_LVL2].m_Style = DEPTH_LIST_STYLE_BOLD;
	m_vDepthNumeral[DEPTH_LVL3].m_Style = DEPTH_LIST_STYLE_BOLD;
}

void CVoteGroup::SetNumeralDepthStyles(std::initializer_list<std::pair<int, int>> vNumeralFlags)
{
	for (const auto& [Depth, Flag] : vNumeralFlags)
		m_vDepthNumeral[Depth].m_Style = Flag;
}

void CVoteGroup::SetVoteTitleImpl(const char* pCmd, std::vector<std::any> Extras, const char* pText)
{
	if (!m_pPlayer || !pText)
		return;

	std::string Prefix;
	std::string Suffix;
	auto* pHidden = m_pPlayer->m_VotesData.GetHidden(m_HiddenID);

	// title alignment (if applicable)
	if ((m_Flags & VWF_ALIGN_TITLE) && (!pHidden || !pHidden->m_State))
	{
		const int TextLength = str_length(pText);
		const int SpaceLength = (VOTE_DESC_LENGTH - TextLength) / 2;
		const bool Styled = (m_Flags & VWF_STYLE_MASK) != 0;
		const int DashCount = std::max(0, (SpaceLength / 4) - 2);

		// reserve memory
		Prefix.reserve(DashCount * 3 + 8);
		Suffix.reserve(DashCount * 3 + 8);

		if (!Styled)
			Prefix += "\u257E";
		for (int i = 0; i < DashCount; ++i)
			Prefix += "\u2500";
		Prefix += "\u257C";

		Suffix += "\u257E";
		for (int i = 0; i < DashCount; ++i)
			Suffix += "\u2500";
		Suffix += "\u257C";
	}

	// hidden state indicator (if applicable)
	if (m_Flags & VWF_HIDABLE_MASK)
	{
		pHidden = m_pPlayer->m_VotesData.EmplaceHidden(m_HiddenID, m_Flags);
		Prefix += pHidden->m_State ? "\u21BA" : "\u27A4";
		Extras.emplace_back(m_HiddenID);
		pCmd = "HIDDEN";
	}

	// finally, create the full description with prefix and suffix
	std::string Buffer;
	Buffer.reserve(Prefix.size() + Suffix.size() + str_length(pText) + 2);
	Buffer.append(Prefix).append(1, ' ').append(pText).append(1, ' ').append(Suffix);
	Reformat(Buffer);

	CVoteOption Vote;
	SafeCopy(Vote.m_aDescription, Buffer.c_str(), sizeof(Vote.m_aDescription));
	SafeCopy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_Extras = std::move(Extras);
	Vote.m_Title = true;

	if (m_vpVotelist.empty() || !m_HasTitle)
	{
		m_HasTitle = true;
		m_vpVotelist.push_front(std::move(Vote));
	}
	else
	{
		m_vpVotelist.front() = std::move(Vote);
	}
}

void CVoteGroup::AddVoteImpl(const char* pCmd, std::vector<std::any> Extras, const char* pText)
{
	if (!m_pPlayer || IsHidden() || !pText || !pCmd)
		return;

	const bool WithArrow = (str_comp(pCmd, "null") != 0);
	std::string Buffer;
	Buffer.reserve(str_length(pText) + 4);
	if (WithArrow)
		Buffer.append("\u257E ");
	Buffer.append(pText);
	Reformat(Buffer);

	CVoteOption Vote;
	SafeCopy(Vote.m_aDescription, Buffer.c_str(), sizeof(Vote.m_aDescription));
	SafeCopy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_Extras = std::move(Extras);
	Vote.m_Depth = m_CurrentDepth;

	m_vpVotelist.emplace_back(std::move(Vote));
	++m_GroupSize;
}

void CVoteGroup::Reformat(std::string& Buffer)
{
	// numeration for marked list items
	if (m_NextMarkedListItem)
	{
		auto& Numeral = m_vDepthNumeral[m_CurrentDepth];
		Buffer.insert(0, GetNumeralStyle(Numeral.m_Value, Numeral.m_Style));
		++Numeral.m_Value;
		m_NextMarkedListItem = false;
	}

	// transliteration for specific languages (e.g., Russian, Ukrainian)
	if (NeedsTransliteration(m_pPlayer->GetLanguage()))
		mystd::string::str_transliterate(Buffer.data());
}

void CVoteGroup::AddLineImpl()
{
	if (!m_pPlayer || IsHidden())
		return;

	CVoteOption Vote;
	SafeCopy(Vote.m_aDescription, VOTE_LINE_DEFAULT, sizeof(Vote.m_aDescription));
	SafeCopy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));
	Vote.m_Line = true;
	m_vpVotelist.emplace_back(std::move(Vote));
	++m_GroupSize;
}

void CVoteGroup::AddBackpageImpl()
{
	if (!m_pPlayer || IsHidden())
		return;

	AddLineImpl();
	CVoteOption Vote;
	SafeCopy(Vote.m_aDescription,
		Instance::Localize(m_ClientID, "\u21A9 Backpage"),
		sizeof(Vote.m_aDescription));
	SafeCopy(Vote.m_aCommand, "BACK", sizeof(Vote.m_aCommand));
	m_vpVotelist.emplace_back(std::move(Vote));
}

void CVoteGroup::AddEmptylineImpl()
{
	if (!m_pPlayer || IsHidden())
		return;

	CVoteOption Vote;
	Vote.m_aDescription[0] = '\0';
	SafeCopy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));
	m_vpVotelist.emplace_back(std::move(Vote));
}

void CVoteGroup::AddItemValueImpl(int ItemID)
{
	if (!m_pPlayer || IsHidden())
		return;

	const auto* pInfo = GS()->GetItemInfo(ItemID);
	if (!pInfo)
		return;

	if (ItemID == itGold)
	{
		AddVoteImpl("null", {},
			fmt_localize(m_ClientID, "You have {$} {}(including bank)",
				m_pPlayer->Account()->GetTotalGold(), pInfo->GetName()).c_str());
		return;
	}

	AddVoteImpl("null", {},
		fmt_localize(m_ClientID, "You have {} {}",
			m_pPlayer->GetItem(ItemID)->GetValue(), pInfo->GetName()).c_str());
}

bool CVoteGroup::IsHidden() const noexcept
{
	if (!m_pPlayer || !(m_Flags & VWF_HIDABLE_MASK))
		return false;

	const auto* pHidden = m_pPlayer->m_VotesData.GetHidden(m_HiddenID);
	return pHidden && pHidden->m_State;
}

// =====================================================================
// VoteWrapper
// =====================================================================

void VoteWrapper::RebuildVotes(int ClientID)
{
	auto* pGS = static_cast<CGS*>(Instance::GameServerPlayer(ClientID));
	if (!pGS)
		return;

	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if (!pPlayer)
		return;

	// empty menu list check
	auto& ClientVoteGroups = m_pData[ClientID];
	if (ClientVoteGroups.empty())
	{
		CVotePlayerData* pVotesData = &pPlayer->m_VotesData;
		pVotesData->SetLastMenuID(MENU_MAIN);
		VoteWrapper VError(ClientID, VWF_STYLE_SIMPLE, "Error");
		VError.Add("The voting list is empty")
			.BeginDepth()
			.Add("Probably a server error")
			.EndDepth()
			.Add("Report the error code #{} x{}",
				pVotesData->GetCurrentMenuID(),
				pVotesData->GetLastMenuID())
			.AddBackpage(ClientID);
	}

	// phase 1: add lines for empty groups and hidden groups
	for (size_t i = 0; i < ClientVoteGroups.size(); ++i)
	{
		CVoteGroup* pGroup = ClientVoteGroups[i].get();
		if (!pGroup)
			continue;

		// is empty group with title? add placeholder
		if (pGroup->m_HasTitle && pGroup->IsEmpty() && !pGroup->IsHidden())
			pGroup->AddVoteImpl("null", {}, "Is empty");

		if (!(pGroup->m_Flags & VWF_SEPARATE))
			continue;

		// check next group for meaningful content
		bool nextIsMeaningful = false;
		if (i + 1 < ClientVoteGroups.size())
		{
			const CVoteGroup* pNext = ClientVoteGroups[i + 1].get();
			if (pNext && !pNext->m_vpVotelist.empty())
			{
				const auto& back = pNext->m_vpVotelist.back();
				nextIsMeaningful = !back.m_Line && back.m_aDescription[0] != '\0';
			}
		}

		if (pGroup->IsHidden() && nextIsMeaningful)
		{
			// if current group is hidden
			auto pNewGroup = std::unique_ptr<CVoteGroup>(new CVoteGroup(ClientID, VWF_DISABLED));
			pNewGroup->AddLineImpl();
			ClientVoteGroups.insert(ClientVoteGroups.begin() + i + 1, std::move(pNewGroup));
			++i;
		}
		else if (!pGroup->m_vpVotelist.empty() && !pGroup->m_vpVotelist.back().m_Line)
		{
			pGroup->AddLineImpl();
		}
	}

	// phase 2: delete duplicate lines (if any)
	const CVoteOption* pLastOption = nullptr;
	for (auto& upGroup : ClientVoteGroups)
	{
		CVoteGroup* pGroup = upGroup.get();
		if (!pGroup)
			continue;

		auto& votes = pGroup->m_vpVotelist;
		for (auto it = votes.begin(); it != votes.end();)
		{
			if (pLastOption && pLastOption->m_Line && it->m_Line)
			{
				it = votes.erase(it);
			}
			else
			{
				pLastOption = &(*it);
				++it;
			}
		}
	}

	// phase 3: send for client
	for (auto& upGroup : ClientVoteGroups)
	{
		CVoteGroup* pGroup = upGroup.get();
		if (!pGroup || pGroup->m_vpVotelist.empty())
			continue;

		const bool hasStyle = (pGroup->m_Flags & VWF_STYLE_MASK) && !pGroup->IsHidden();
		const CVoteOption* pFront = &pGroup->m_vpVotelist.front();
		const CVoteOption* pBack = &pGroup->m_vpVotelist.back();
		const int Flags = pGroup->m_Flags;

		for (auto& Option : pGroup->m_vpVotelist)
		{
			if (hasStyle)
			{
				std::string Buffer;
				Buffer.reserve(VOTE_DESC_LENGTH + 16);

				// select symbols for the first, last and middle options
				if (&Option == pFront)
					Buffer += GetBorderStyle(BorderType::Begin, Flags);
				else if (&Option == pBack)
					Buffer += GetBorderStyle(BorderType::End, Flags);
				else if (str_comp(Option.m_aCommand, "null") == 0 && Option.m_Depth <= 0 && !Option.m_Line)
					Buffer += GetBorderStyle(BorderType::Middle, Flags);
				else
					Buffer += GetBorderStyle(BorderType::MiddleOption, Flags);

				// separate depth list
				if (!Option.m_Line && Option.m_Depth > 0)
				{
					const char* pLevelStr = GetBorderStyle(BorderType::Level, Flags);
					for (int d = 0; d < Option.m_Depth; ++d)
						Buffer += pLevelStr;
				}

				if (!Option.m_Line && !Option.m_Title && str_comp(Option.m_aCommand, "null") == 0)
					Buffer += ' ';

				Buffer += Option.m_aDescription;
				SafeCopy(Option.m_aDescription, Buffer.c_str(), sizeof(Option.m_aDescription));
			}

			// send the option to the client
			CNetMsg_Sv_VoteOptionAdd OptionMsg;
			OptionMsg.m_pDescription = Option.m_aDescription;
			pGS->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
		}
	}
}

CVoteOption* VoteWrapper::GetOptionVoteByAction(int ClientID, const char* pActionName)
{
	if (!pActionName || pActionName[0] == '\0')
		return nullptr;

	const int ActionLen = str_length(pActionName);
	for (auto& upGroup : Data()[ClientID])
	{
		CVoteGroup* pGroup = upGroup.get();
		if (!pGroup)
			continue;

		auto it = std::ranges::find_if(pGroup->m_vpVotelist,
			[pActionName, ActionLen](const CVoteOption& vote)
			{
				return str_utf8_comp_nocase_num(pActionName, vote.m_aDescription, ActionLen) == 0;
			});

		if (it != pGroup->m_vpVotelist.end())
			return &(*it);
	}
	return nullptr;
}

// =====================================================================
// CVotePlayerData
// =====================================================================

CVotePlayerData::VoteGroupHidden* CVotePlayerData::EmplaceHidden(int ID, int Type)
{
	auto& vmHiddens = m_aHiddenGroup[m_CurrentMenuID];

	if (auto it = vmHiddens.find(ID); it != vmHiddens.end() && it->second.m_Flag == Type)
		return &it->second;

	const bool Value = (Type & VWF_CLOSED) || (Type & VWF_UNIQUE);
	auto [it, _] = vmHiddens.insert_or_assign(ID, VoteGroupHidden{ Value, Type });
	return &it->second;
}

CVotePlayerData::VoteGroupHidden* CVotePlayerData::GetHidden(int ID)
{
	auto& vmHiddens = m_aHiddenGroup[m_CurrentMenuID];
	auto it = vmHiddens.find(ID);
	return it != vmHiddens.end() ? &it->second : nullptr;
}

void CVotePlayerData::ResetHidden(int MenuID)
{
	auto it = m_aHiddenGroup.find(MenuID);
	if (it == m_aHiddenGroup.end())
		return;

	for (auto& [ID, Hide] : it->second)
	{
		if (Hide.m_Flag & VWF_UNIQUE)
			Hide.m_State = true;
	}
}

void CVotePlayerData::ThreadVoteUpdater(CVotePlayerData* pData)
{
	if (!pData)
		return;

	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	if (pData->m_ShuttingDown.load(std::memory_order_acquire) || !pData->m_pPlayer)
		return;

	pData->m_VoteUpdaterStatus.store(STATE_UPDATER::DONE, std::memory_order_release);
}

void CVotePlayerData::ApplyVoteUpdaterData()
{
	if (m_VoteUpdaterStatus.load(std::memory_order_acquire) != STATE_UPDATER::DONE)
		return;

	ClearVotes();
	m_pGS->Core()->OnSendMenuVotes(m_pPlayer, m_CurrentMenuID);
	VoteWrapper::RebuildVotes(m_pPlayer->GetCID());
	m_VoteUpdaterStatus.store(STATE_UPDATER::WAITING, std::memory_order_release);
}

void CVotePlayerData::UpdateVotes(int MenuID)
{
	m_CurrentMenuID = MenuID;

	STATE_UPDATER expected = STATE_UPDATER::WAITING;
	if (m_VoteUpdaterStatus.compare_exchange_strong(expected, STATE_UPDATER::RUNNING,
		std::memory_order_acq_rel))
	{
		if (m_VoteUpdater.joinable())
			m_VoteUpdater.join();
		m_VoteUpdater = std::thread(&CVotePlayerData::ThreadVoteUpdater, this);
	}
}

void CVotePlayerData::UpdateVotesIf(int MenuID)
{
	if (m_CurrentMenuID == MenuID)
		UpdateVotes(MenuID);
}

void CVotePlayerData::ClearVotes() const
{
	if (!m_pPlayer)
		return;

	const int ClientID = m_pPlayer->GetCID();
	mystd::freeContainer(VoteWrapper::Data()[ClientID]);

	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Instance::Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);
}

void CVotePlayerData::PushExtraID(int MenuID, std::optional<int> ExtraID)
{
	m_aExtraIDHistory[MenuID].push(ExtraID);
}

std::optional<int> CVotePlayerData::PopExtraID(int MenuID)
{
	auto it = m_aExtraIDHistory.find(MenuID);
	if (it == m_aExtraIDHistory.end() || it->second.empty())
		return std::nullopt;

	auto ExtraID = it->second.top();
	it->second.pop();
	return ExtraID;
}

std::optional<int> CVotePlayerData::PeekExtraID(int MenuID) const
{
	auto it = m_aExtraIDHistory.find(MenuID);
	if (it == m_aExtraIDHistory.end() || it->second.empty())
		return std::nullopt;
	return it->second.top();
}

bool CVotePlayerData::HasExtraIDHistory(int MenuID) const
{
	auto it = m_aExtraIDHistory.find(MenuID);
	return it != m_aExtraIDHistory.end() && !it->second.empty();
}

void CVotePlayerData::ClearExtraIDHistory(int MenuID)
{
	auto it = m_aExtraIDHistory.find(MenuID);
	if (it != m_aExtraIDHistory.end())
		it->second = {};
}

bool CVotePlayerData::DefaultVoteCommands(const char* pCmd, std::vector<std::any> Extras, int, const char*)
{
	if (!pCmd || PPSTR(pCmd, "null") == 0)
		return true;

	// === MENU ===
	if (PPSTR(pCmd, "MENU") == 0)
	{
		const int MenuID = GetIfExists<int>(Extras, 0, NOPE);
		const int GroupID = GetIfExists<int>(Extras, 1, NOPE);
		std::optional<int> NewExtraID = (GroupID <= NOPE) ? std::nullopt : std::make_optional(GroupID);

		if (MenuID == m_CurrentMenuID)
			PushExtraID(MenuID, m_ExtraID);
		else
			ClearExtraIDHistory(MenuID);

		m_ExtraID = NewExtraID;
		m_pGS->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_UI_MENU_CLICK);
		ResetHidden(MenuID);
		UpdateVotes(MenuID);
		return true;
	}

	// === BACK ===
	if (PPSTR(pCmd, "BACK") == 0)
	{
		m_pGS->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_UI_MENU_CLICK);
		if (HasExtraIDHistory(m_CurrentMenuID))
		{
			m_ExtraID = PopExtraID(m_CurrentMenuID);
			UpdateCurrentVotes();
		}
		else
		{
			m_ExtraID = std::nullopt;
			UpdateVotes(m_LastMenuID);
		}
		return true;
	}

	// click sound for any other command
	m_pGS->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_UI_MENU_ITEM_CLICK);

	// === HIDDEN ===
	if (PPSTR(pCmd, "HIDDEN") == 0)
	{
		const int HiddenID = GetIfExists<int>(Extras, 0, NOPE);
		if (VoteGroupHidden* pHidden = GetHidden(HiddenID))
		{
			const bool prevState = pHidden->m_State;

			if (pHidden->m_Flag & VWF_UNIQUE)
				ResetHidden(m_CurrentMenuID);

			pHidden->m_State = !prevState;
			UpdateCurrentVotes();
		}
		return true;
	}

	return false;
}