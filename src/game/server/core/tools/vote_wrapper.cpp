#include "vote_wrapper.h"

#include <game/server/gamecontext.h>

namespace
{
	constexpr auto g_VoteStrLineDef = "\u257E\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u257C";

	constexpr int MaxNumber = 9;
	constexpr std::array<std::array<const char*, MaxNumber>, NUM_DEPTH_LIST_STYLES> NumeralStyles = {{
		{ "1. ", "2. ", "3. ", "4. ", "5. ", "6. ", "7. ", "8. ", "9. " },
		{ "\u2160. ", "\u2161. ", "\u2162. ", "\u2163. ", "\u2164. ", "\u2165. ", "\u2166. ", "\u2167. ", "\u2168. " },
		{ "\uFF11. ", "\uFF12. ", "\uFF13. ", "\uFF14. ", "\uFF15. ", "\uFF16. ", "\uFF17. ", "\uFF18. ", "\uFF19. " },
		{ "\u24F5. ", "\u24F6. ", "\u24F7. ", "\u24F8. ", "\u24F9. ", "\u24FA. ", "\u24FB. ", "\u24FC. ", "\u24FD. " }
	}};

	const char* GetNumeralStyle(int Number, int Style) noexcept
	{
		if(Style >= 0 && Style < NUM_DEPTH_LIST_STYLES)
		{
			return NumeralStyles[Style][std::clamp(Number, 0, MaxNumber - 1)];
		}
		return "";
	}

	enum class BorderType : int { Beggin, Middle, MiddleOption, Level, End };
	constexpr std::array SimpleBorders = { "\u256D", "\u2502", "\u251C", "\u2508", "\u2570" };
	constexpr std::array DoubleBorders = { "\u2554", "\u2551", "\u2560", "\u2550", "\u255A" };
	constexpr std::array StrictBorders = { "\u250C", "\u2502", "\u251C", "\u2508", "\u2514" };
	constexpr std::array StrictBoldBorders = { "\u250F", "\u2503", "\u2523", "\u2509", "\u2517" };

	const char* GetBorderStyle(BorderType Border, int Flags) noexcept
	{
		if(Flags & VWF_STYLE_SIMPLE) return SimpleBorders[static_cast<int>(Border)];
		if(Flags & VWF_STYLE_DOUBLE) return DoubleBorders[static_cast<int>(Border)];
		if(Flags & VWF_STYLE_STRICT) return StrictBorders[static_cast<int>(Border)];
		if(Flags & VWF_STYLE_STRICT_BOLD) return StrictBoldBorders[static_cast<int>(Border)];
		return "";
	}
}

CVoteGroup::CVoteGroup(int ClientID, int Flags) : m_Flags(Flags), m_ClientID(ClientID)
{
	m_pGS = (CGS*)Instance::GameServerPlayer(ClientID);
	m_HasTitle = false;
	m_CurrentDepth = 0;
	m_GroupSize = 0;
	m_HiddenID = (int)VoteWrapper::Data()[ClientID].size();
	m_pPlayer = m_pGS->GetPlayer(ClientID);
	dbg_assert(m_pPlayer != nullptr, "player is null");

	// init default numeral lists
	m_vDepthNumeral[DEPTH_LVL1].m_Style = DEPTH_LIST_STYLE_ROMAN;
	m_vDepthNumeral[DEPTH_LVL2].m_Style = DEPTH_LIST_STYLE_BOLD;
	m_vDepthNumeral[DEPTH_LVL3].m_Style = DEPTH_LIST_STYLE_BOLD;
}

void CVoteGroup::SetNumeralDepthStyles(std::initializer_list<std::pair<int, int>> vNumeralFlags)
{
	for(const auto& [Depth, Flag] : vNumeralFlags)
		m_vDepthNumeral[Depth].m_Style = Flag;
}

void CVoteGroup::SetVoteTitleImpl(const char* pCmd, int Extra1, int Extra2, const char* pText)
{
	// check player valid
	if(!m_pPlayer)
		return;

	// initialize variables
	std::string Prefix {};
	std::string Suffix {};
	auto pHidden = m_pPlayer->m_VotesData.GetHidden(m_HiddenID);

	// check flag align title
	if(m_Flags & VWF_ALIGN_TITLE && ((pHidden && !pHidden->m_State) || !pHidden))
	{
		// initialize variables
		std::string StrSpace = " ";
		const int TextLength = str_length(pText);
		const int SpaceLength = (VOTE_DESC_LENGTH - TextLength) / 2;
		const bool Styled = m_Flags & (VWF_STYLE_SIMPLE | VWF_STYLE_DOUBLE | VWF_STYLE_STRICT | VWF_STYLE_STRICT_BOLD);

		// prefix
		if(!Styled)
			Prefix += "\u257E";
		for(int Space = 0; Space < (SpaceLength / 4) - 2; Space++)
			Prefix += "\u2500";
		Prefix += "\u257C";

		// suffix
		Suffix += "\u257E";
		for(int Space = 0; Space < (SpaceLength / 4) - 2; Space++)
			Suffix += "\u2500";
		Suffix += "\u257C";
	}

	// check flag is tab type
	if(m_Flags & (VWF_CLOSED | VWF_OPEN | VWF_UNIQUE))
	{
		pHidden = m_pPlayer->m_VotesData.EmplaceHidden(m_HiddenID, m_Flags);
		Prefix += pHidden->m_State ? "\u21BA" : "\u27A4";
		Extra1 = m_HiddenID;
		pCmd = "HIDDEN";
	}

	// reformating
	std::string Buffer(Prefix + " " + std::string(pText) + " " + Suffix);
	Reformat(Buffer);

	// new option
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, Buffer.c_str(), sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_Extra1 = Extra1;
	Vote.m_Extra2 = Extra2;
	Vote.m_Title = true;

	if(m_vpVotelist.empty() || !m_HasTitle)
	{
		m_HasTitle = true;
		m_vpVotelist.push_front(Vote);
	}
	else
	{
		m_vpVotelist.front() = Vote;
	}
}

void CVoteGroup::AddVoteImpl(const char* pCmd, int Extra1, int Extra2, const char* pText)
{
	// check player valid and hidden status
	if(!m_pPlayer || IsHidden())
		return;

	// reformating
	std::string Buffer((str_comp(pCmd, "null") != 0 ? "\u257E " : "") + std::string(pText));
	Reformat(Buffer);

	// new option
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, Buffer.c_str(), sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_Extra1 = Extra1;
	Vote.m_Extra2 = Extra2;
	Vote.m_Depth = m_CurrentDepth;
	m_vpVotelist.emplace_back(Vote);
	m_GroupSize++;
}

void CVoteGroup::Reformat(std::string& Buffer)
{
	// numeral list format
	if(m_NextMarkedListItem)
	{
		auto& Numeral = m_vDepthNumeral[m_CurrentDepth];
		Buffer.insert(0, GetNumeralStyle(Numeral.m_Value, Numeral.m_Style));
		Numeral.m_Value++;
		m_NextMarkedListItem = false;
	}

	// optimize text for buffer size
	if(str_comp(m_pPlayer->GetLanguage(), "ru") == 0 || str_comp(m_pPlayer->GetLanguage(), "uk") == 0)
		mystd::string::str_transliterate(Buffer.data());
}

void CVoteGroup::AddLineImpl()
{
	// check player valid and hidden status
	if(!m_pPlayer || IsHidden())
		return;

	// new option
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, g_VoteStrLineDef, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));
	Vote.m_Line = true;
	m_vpVotelist.emplace_back(Vote);
	m_GroupSize++;
}

void CVoteGroup::AddBackpageImpl()
{
	// check player valid and hidden status
	if(!m_pPlayer || IsHidden())
		return;

	// new option
	AddLineImpl();
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, Instance::Localize(m_ClientID, "\u21A9 Backpage"), sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, "BACK", sizeof(Vote.m_aCommand));
	m_vpVotelist.emplace_back(Vote);
}

void CVoteGroup::AddEmptylineImpl()
{
	// check player valid and hidden status
	if(!m_pPlayer || IsHidden())
		return;

	// new option
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, "\0", sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));
	m_vpVotelist.emplace_back(Vote);
}

void CVoteGroup::AddItemValueImpl(int ItemID)
{
	// check player valid and hidden status
	if(!m_pPlayer || IsHidden())
		return;

	// show gold include bank
	if(ItemID == itGold)
	{
		AddVoteImpl("null", NOPE, NOPE, fmt_localize(m_ClientID, "You have {$} {}(including bank)",
			m_pPlayer->Account()->GetTotalGold(), GS()->GetItemInfo(ItemID)->GetName()).c_str());
		return;
	}

	// add item value
	AddVoteImpl("null", NOPE, NOPE, fmt_localize(m_ClientID, "You have {} {}",
		m_pPlayer->GetItem(ItemID)->GetValue(), GS()->GetItemInfo(ItemID)->GetName()).c_str());
}

bool CVoteGroup::IsHidden() const
{
	// check player valid
	if(!m_pPlayer)
		return false;

	// check flag hidden
	if(m_Flags & (VWF_CLOSED | VWF_OPEN | VWF_UNIQUE))
	{
		const auto* pHidden = m_pPlayer->m_VotesData.GetHidden(m_HiddenID);
		return pHidden && pHidden->m_State;
	}

	return false;
}

void VoteWrapper::RebuildVotes(int ClientID)
{
	// check player valid
	CGS* pGS = (CGS*)Instance::GameServerPlayer(ClientID);
	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	// check is empty option's
	if(m_pData[ClientID].empty())
	{
		CVotePlayerData* pVotesData = &pPlayer->m_VotesData;
		pVotesData->SetLastMenuID(MENU_MAIN);
		VoteWrapper VError(ClientID, VWF_STYLE_SIMPLE, "Error");
		VError.Add("The voting list is empty")
		.BeginDepth()
			.Add("Probably a server error")
		.EndDepth()
		.Add("Report the error code #{} x{}", pVotesData->GetCurrentMenuID(), pVotesData->GetLastMenuID())
		.AddBackpage(ClientID);
	}

	auto& ClientVoteGroups = m_pData[ClientID];

	// first changes (adding/removing lines/groups)
	for(size_t i = 0; i < ClientVoteGroups.size(); ++i)
	{
		CVoteGroup* pGroup = ClientVoteGroups[i];

		// if the group is an expandable list type, it is empty if there is no element in it
		if(pGroup->m_HasTitle && pGroup->IsEmpty() && !pGroup->IsHidden())
			pGroup->AddVoteImpl("null", NOPE, NOPE, "Is empty");

		// check flag end with line
		if(pGroup->m_Flags & VWF_SEPARATE)
		{
			bool nextGroupExists = (i + 1 < ClientVoteGroups.size());
			bool nextNotEmptyline = false;
			if(nextGroupExists)
			{
				CVoteGroup* pNextGroup = ClientVoteGroups[i + 1];
				if(pNextGroup && !pNextGroup->m_vpVotelist.empty() && (!pNextGroup->m_vpVotelist.back().m_Line && pNextGroup->m_vpVotelist.back().m_aDescription[0] != '\0'))
					nextNotEmptyline = true;
			}

			if(pGroup->IsHidden() && nextNotEmptyline)
			{
				auto pNewVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
				pNewVoteGroup->AddLineImpl();
				ClientVoteGroups.insert(ClientVoteGroups.begin() + i + 1, pNewVoteGroup);
				++i;
			}
			else if(!pGroup->m_vpVotelist.empty() && !pGroup->m_vpVotelist.back().m_Line)
			{
				pGroup->AddLineImpl();
			}
		}
	}

	// delete consecutive lines
	CVoteOption* pLastVoteOption = nullptr;
	for(CVoteGroup* pGroup : ClientVoteGroups)
	{
		for(auto iterVote = pGroup->m_vpVotelist.begin(); iterVote != pGroup->m_vpVotelist.end();)
		{
			if(pLastVoteOption && pLastVoteOption->m_Line && iterVote->m_Line)
			{
				iterVote = pGroup->m_vpVotelist.erase(iterVote);
				continue;
			}
			pLastVoteOption = &(*iterVote);
			++iterVote;
		}
	}

	// second format and send options
	for(const auto pGroup : ClientVoteGroups)
	{
		if(pGroup->m_vpVotelist.empty())
			continue;

		const CVoteOption* pFrontOption = &pGroup->m_vpVotelist.front();
		const CVoteOption* pBackOption = &pGroup->m_vpVotelist.back();

		for(auto& Option : pGroup->m_vpVotelist)
		{
			// style and border
			if(pGroup->m_Flags & (VWF_STYLE_SIMPLE | VWF_STYLE_DOUBLE | VWF_STYLE_STRICT | VWF_STYLE_STRICT_BOLD) && !pGroup->IsHidden())
			{
				std::string Buffer;
				Buffer.reserve(VOTE_DESC_LENGTH + 10);

				const int& Flags = pGroup->m_Flags;

				if(&Option == pFrontOption)
					Buffer += GetBorderStyle(BorderType::Beggin, Flags);
				else if(&Option == pBackOption)
					Buffer += GetBorderStyle(BorderType::End, Flags);
				else if(str_comp(Option.m_aCommand, "null") == 0 && Option.m_Depth <= 0 && !Option.m_Line)
					Buffer += GetBorderStyle(BorderType::Middle, Flags);
				else
					Buffer += GetBorderStyle(BorderType::MiddleOption, Flags);

				// level of the vote option
				if(!Option.m_Line && Option.m_Depth > 0)
				{
					for(int d = 0; d < Option.m_Depth; d++)
						Buffer += GetBorderStyle(BorderType::Level, Flags);
				}

				if(!Option.m_Line && !Option.m_Title && str_comp(Option.m_aCommand, "null") == 0)
					Buffer += " ";

				Buffer += Option.m_aDescription;
				str_copy(Option.m_aDescription, Buffer.c_str(), sizeof(Option.m_aDescription));
			}

			// send the vote option to the client
			CNetMsg_Sv_VoteOptionAdd OptionMsg;
			OptionMsg.m_pDescription = Option.m_aDescription;
			pGS->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
		}
	}
}

CVoteOption* VoteWrapper::GetOptionVoteByAction(int ClientID, const char* pActionName)
{
	// find the vote option with the matching action name
	for(const auto& iterGroup : Data()[ClientID])
	{
		auto iter = std::ranges::find_if(iterGroup->m_vpVotelist, [pActionName](const CVoteOption& vote)
		{
			return str_utf8_comp_nocase_num(pActionName, vote.m_aDescription, str_length(pActionName)) == 0;
		});

		if(iter != iterGroup->m_vpVotelist.end())
		{
			return &(*iter);
		}
	}

	return nullptr;
}

CVotePlayerData::VoteGroupHidden* CVotePlayerData::EmplaceHidden(int ID, int Type)
{
	auto& vmHiddens = m_aHiddenGroup[m_CurrentMenuID];
	if(vmHiddens.find(ID) != vmHiddens.end() && vmHiddens[ID].m_Flag == Type)
		return &vmHiddens[ID];

	bool Value = (Type & VWF_CLOSED) || (Type & VWF_UNIQUE);
	vmHiddens[ID] = { Value, Type };
	return &vmHiddens[ID];
}

CVotePlayerData::VoteGroupHidden* CVotePlayerData::GetHidden(int ID)
{
	auto& vmHiddens = m_aHiddenGroup[m_CurrentMenuID];
	auto it = vmHiddens.find(ID);
	if(it != vmHiddens.end())
		return &it->second;

	return nullptr;
}

void CVotePlayerData::ResetHidden(int MenuID)
{
	auto& vmHiddens = m_aHiddenGroup[MenuID];
	if(vmHiddens.empty())
		return;

	for(auto& [ID, Hide] : vmHiddens)
	{
		if(Hide.m_Flag & VWF_UNIQUE)
			Hide.m_State = true;
	}
}

void CVotePlayerData::ThreadVoteUpdater(CVotePlayerData* pData)
{
	// sleep updater for getting some data
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	if(!pData || !pData->m_pPlayer)
		return;

	// apply the vote updater data
	pData->m_VoteUpdaterStatus.store(STATE_UPDATER::DONE);
}

void CVotePlayerData::ApplyVoteUpdaterData()
{
	// check if the vote updater is done
	if(m_VoteUpdaterStatus == STATE_UPDATER::DONE)
	{
		ClearVotes();
		m_pGS->Core()->OnSendMenuVotes(m_pPlayer, m_CurrentMenuID);
		VoteWrapper::RebuildVotes(m_pPlayer->GetCID());
		m_VoteUpdaterStatus.store(STATE_UPDATER::WAITING);
	}
}

void CVotePlayerData::UpdateVotes(int MenuID)
{
	m_CurrentMenuID = MenuID;

	// check if the vote updater is waiting
	if(m_VoteUpdaterStatus == STATE_UPDATER::WAITING)
	{
		m_VoteUpdaterStatus.store(STATE_UPDATER::RUNNING);
		m_VoteUpdater = std::thread(&CVotePlayerData::ThreadVoteUpdater, this);
		m_VoteUpdater.detach();
	}
}

void CVotePlayerData::UpdateVotesIf(int MenuID)
{
	if(m_CurrentMenuID == MenuID)
		UpdateVotes(MenuID);
}

void CVotePlayerData::ClearVotes() const
{
	// free container data
	int ClientID = m_pPlayer->GetCID();
	mystd::freeContainer(VoteWrapper::Data()[ClientID]);

	// send vote options
	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Instance::Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);
}

bool CVotePlayerData::DefaultVoteCommands(const char* pCmd, const int Extra1, const int Extra2, int, const char*)
{
	// is empty
	if(PPSTR(pCmd, "null") == 0)
		return true;

	// command menu
	if(PPSTR(pCmd, "MENU") == 0)
	{
		m_ExtraID = Extra2 <= NOPE ? std::nullopt : std::make_optional(Extra2);

		m_pGS->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_VOTE_MENU_CLICK);
		ResetHidden(Extra1);
		UpdateVotes(Extra1);
		return true;
	}

	// command back
	if(PPSTR(pCmd, "BACK") == 0)
	{
		m_pGS->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_VOTE_MENU_CLICK);
		UpdateVotes(m_LastMenuID);
		return true;
	}

	// sound effect
	m_pGS->CreatePlayerSound(m_pPlayer->GetCID(), SOUND_VOTE_ITEM_CLICK);

	// command hidden
	if(PPSTR(pCmd, "HIDDEN") == 0)
	{
		if(VoteGroupHidden* pHidden = GetHidden(Extra1))
		{
			const bool Value = pHidden->m_State;

			// check if the hidden vote group is unique
			if(pHidden->m_Flag & VWF_UNIQUE)
				ResetHidden(m_CurrentMenuID);

			// toggle the hidden
			pHidden->m_State = !Value;
			UpdateCurrentVotes();
		}
		return true;
	}

	return false;
}