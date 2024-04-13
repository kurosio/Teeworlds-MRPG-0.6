#include "vote_wrapper.h"

#include <game/server/gamecontext.h>

// Formatter vote wrapper

namespace Formatter
{
	const char* g_VoteStrLineDef = "\u257E\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u257C";
	static int gs_GroupsNumeral[MAX_CLIENTS] = {};
	constexpr int g_NumeralNum = 10;

	// Numeral
	class Numeral
	{
		inline static constexpr const char* g_pNDefault[g_NumeralNum] = { "\0", "1. ", "2. ", "3. ", "4. ", "5. ", "6. ", "7. ", "8. ", "9. " };
		inline static constexpr const char* g_pNRoman[g_NumeralNum] = { "\0", "\u2160. ", "\u2161. ", "\u2162. ", "\u2163. ", "\u2164. ", "\u2165. ", "\u2166. ", "\u2167. ", "\u2168. " };
		inline static constexpr const char* g_pNBold[g_NumeralNum] = { "\0", "\uFF11. ", "\uFF12. ", "\uFF13. ", "\uFF14. ", "\uFF15. ", "\uFF16. ", "\uFF17. ", "\uFF18. ", "\uFF19. " };
		inline static constexpr const char* g_pNCyrcle[g_NumeralNum] = { "\0", "\u24F5. ", "\u24F6. ", "\u24F7. ", "\u24F8. ", "\u24F9. ", "\u24FA. ", "\u24FB. ", "\u24FC. ", "\u24FD. " };

	public:
		static constexpr const char* get(int Number, int Style)
		{
			if(Style & DEPTH_LIST_STYLE_ROMAN) { return g_pNRoman[clamp(Number, 1, 9)]; }
			if(Style & DEPTH_LIST_STYLE_BOLD) { return g_pNBold[clamp(Number, 1, 9)]; }
			if(Style & DEPTH_LIST_STYLE_CYRCLE) { return g_pNCyrcle[clamp(Number, 1, 9)]; }
			return g_pNDefault[clamp(Number, 1, 9)];
		}
	};

	// Border
	class Border
	{
	public:
		enum Type { Beggin, Middle, MiddleOption, Level, End, Num };
		static constexpr const char* get(Type Border, int Flags)
		{
			if(Flags & VWF_STYLE_SIMPLE) { return g_pBSimple[Border]; }
			if(Flags & VWF_STYLE_DOUBLE) { return g_pBDouble[Border]; }
			if(Flags & VWF_STYLE_STRICT) { return g_pBStrict[Border]; }
			if(Flags & VWF_STYLE_STRICT_BOLD) { return g_pBStrictBold[Border]; }
			return "";
		}

	private:
		inline static constexpr const char* g_pBSimple[Num] = { "\u256D", "\u2502", "\u251C", "\u2508", "\u2570" };
		inline static constexpr const char* g_pBDouble[Num] = { "\u2554", "\u2551", "\u2560", "\u2550", "\u255A" };
		inline static constexpr const char* g_pBStrict[Num] = { "\u250C", "\u2502", "\u251C", "\u2508", "\u2514" };
		inline static constexpr const char* g_pBStrictBold[Num] = { "\u250F", "\u2503", "\u2523", "\u2509", "\u2517" };
	};
}

CVoteGroup::CVoteGroup(int ClientID, int Flags) : m_Flags(Flags), m_ClientID(ClientID)
{
	m_pGS = (CGS*)Instance::GameServerPlayer(ClientID);
	m_TitleIsSet = false;
	m_CurrentDepth = 0;
	m_GroupSize = 0;
	m_HiddenID = (int)CVoteWrapper::Data()[ClientID].size();
	m_pPlayer = m_pGS->GetPlayer(ClientID);
	dbg_assert(m_pPlayer != nullptr, "player is null");

	// init default numeral lists
	m_vDepthNumeral[DEPTH_LVL1].m_Style = DEPTH_LIST_STYLE_ROMAN;
	m_vDepthNumeral[DEPTH_LVL2].m_Style = DEPTH_LIST_STYLE_BOLD;
	m_vDepthNumeral[DEPTH_LVL3].m_Style = DEPTH_LIST_STYLE_BOLD;
}

void CVoteGroup::SetNumeralDepthStyles(std::initializer_list<std::pair<int, int>>&& vNumeralFlags)
{
	for(const auto& [Depth, Flag] : vNumeralFlags)
		m_vDepthNumeral[Depth].m_Style = Flag;
}

// Function to add a vote title implementation with variable arguments
void CVoteGroup::SetVoteTitleImpl(const char* pCmd, int SettingsID1, int SettingsID2, const char* pText, ...)
{
	// Check if the player is valid
	if(!m_pPlayer)
		return;

	// Format the text using the player's language and additional arguments
	va_list VarArgs;
	va_start(VarArgs, pText);
	dynamic_string Buffer;
	Instance::Server()->Localization()->Format_VL(Buffer, m_pPlayer->GetLanguage(), pText, VarArgs);
	va_end(VarArgs);

	const char* pAppend = "\0";
	if(m_Flags & (VWF_CLOSED | VWF_OPEN | VWF_UNIQUE))
	{
		const bool HiddenTab = m_pPlayer->m_VotesData.EmplaceHidden(m_HiddenID, m_Flags)->m_Value;
		pAppend = HiddenTab ? "\u21BA " : "\u27A4 ";
		SettingsID1 = m_HiddenID;
		pCmd = "HIDDEN";
	}

	// Reformatting
	Reformatting(Buffer);

	// Check if the player's language is "ru" or "uk" and convert aBufText to Latin if true
	if(str_comp(m_pPlayer->GetLanguage(), "ru") == 0 || str_comp(m_pPlayer->GetLanguage(), "uk") == 0)
		str_translation_cyrlic_to_latin(Buffer.buffer());

	// End text format
	char aBufText[VOTE_DESC_LENGTH];
	str_format(aBufText, sizeof(aBufText), "%s%s", pAppend, Buffer.buffer());
	Buffer.clear();

	// Create a new VoteOption with the values from aBufText, pCmd, SettingsID1, and SettingsID2
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, aBufText, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_SettingID = SettingsID1;
	Vote.m_SettingID2 = SettingsID2;

	// Add title to front or update the title if it already exists
	if(m_vpVotelist.empty() || !m_TitleIsSet)
	{
		m_TitleIsSet = true;
		m_vpVotelist.push_front(Vote);
	}
	else
	{
		m_vpVotelist.front() = Vote;
	}
}

// Function to add a vote implementation with variable arguments
void CVoteGroup::AddVoteImpl(const char* pCmd, int Settings1, int Settings2, const char* pText, ...)
{
	// Check if the player is valid
	if(!m_pPlayer || IsHidden())
		return;

	// Format the text using the player's language and additional arguments
	va_list VarArgs;
	va_start(VarArgs, pText);
	dynamic_string Buffer;
	Instance::Server()->Localization()->Format_VL(Buffer, m_pPlayer->GetLanguage(), pText, VarArgs);
	va_end(VarArgs);

	// Reformatting
	Reformatting(Buffer);

	// Reformat cyrlic to latin
	if(str_comp(m_pPlayer->GetLanguage(), "ru") == 0 || str_comp(m_pPlayer->GetLanguage(), "uk") == 0)
		str_translation_cyrlic_to_latin(Buffer.buffer());

	// End text format
	char aBufText[VOTE_DESC_LENGTH] {};
	str_format(aBufText, sizeof(aBufText), "%s%s", str_comp(pCmd, "null") != 0 ? "\u257E " : "\0", Buffer.buffer());
	Buffer.clear();

	// Create a new VoteOption with the values from aBufText, pCmd, Settings1, and Settings2
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, aBufText, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_SettingID = Settings1;
	Vote.m_SettingID2 = Settings2;
	Vote.m_Depth = m_CurrentDepth;

	// Add the VoteOption to the player's votes
	m_GroupSize++;
	m_vpVotelist.emplace_back(Vote);
}

void CVoteGroup::Reformatting(dynamic_string& Buffer)
{
	// Numeral list format
	if(m_NextMarkedListItem || m_Flags & VWF_GROUP_NUMERAL)
	{
		char aTempBuf[VOTE_DESC_LENGTH]{};
		NumeralDepth& Numeral = m_vDepthNumeral[m_CurrentDepth];

		if(m_Flags & VWF_GROUP_NUMERAL)
		{
			str_format(aTempBuf, sizeof(aTempBuf), "%s%s", Formatter::Numeral::get(Formatter::gs_GroupsNumeral[m_ClientID] + 1, DEPTH_LIST_STYLE_BOLD), Buffer.buffer());
			Formatter::gs_GroupsNumeral[m_ClientID]++;
			m_Flags &= ~VWF_GROUP_NUMERAL;
		}
		else
		{
			str_format(aTempBuf, sizeof(aTempBuf), "%s%s", Formatter::Numeral::get(Numeral.m_Value + 1, Numeral.m_Style), Buffer.buffer());
			Numeral.m_Value++;
			m_NextMarkedListItem = false;
		}

		Buffer.copy(aTempBuf);
	}
}

// This function is used to add a line
void CVoteGroup::AddLineImpl()
{
	// Check player and hidden status
	if(!m_pPlayer || IsHidden())
		return;

	// Create a new VoteOption with the values
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, Formatter::g_VoteStrLineDef, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));
	Vote.m_Line = true;

	// Add the VoteOption to the player's votes
	m_GroupSize++;
	m_vpVotelist.emplace_back(Vote);
}

// This function adds the implementation for the AddBackpage method in the CVoteGroup class.
void CVoteGroup::AddBackpageImpl()
{
	// Check player and hidden status
	if(!m_pPlayer || IsHidden())
		return;

	// Create a new VoteOption with the values
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, "\u21A9 Backpage", sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, "BACK", sizeof(Vote.m_aCommand));

	// Add the VoteOption to the player's votes
	AddLineImpl();
	m_vpVotelist.emplace_back(Vote);
}

// This function adds an empty line
void CVoteGroup::AddEmptylineImpl()
{
	// Check player and hidden status
	if(!GS()->GetPlayer(m_ClientID) || IsHidden())
		return;

	// Create a new VoteOption with the values
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, "\0", sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));

	// Add the VoteOption to the player's votes
	m_vpVotelist.emplace_back(Vote);
}

// Function to add an item value information to the CVoteGroup
void CVoteGroup::AddItemValueImpl(int ItemID)
{
	// Check player and hidden status
	CPlayer* pPlayer = GS()->GetPlayer(m_ClientID);
	if(!pPlayer || IsHidden())
		return;

	AddVoteImpl("null", NOPE, NOPE, "You have {VAL} {STR}", pPlayer->GetItem(ItemID)->GetValue(), GS()->GetItemInfo(ItemID)->GetName());
}

// Function for check hidden status
bool CVoteGroup::IsHidden() const
{
	// For special flags hidden does not used
	if(m_Flags & (VWF_CLOSED | VWF_OPEN | VWF_UNIQUE))
	{
		// Check if the player is valid
		if(m_pPlayer)
		{
			// Check if the hidden vote object exists and if its value is true
			CVotePlayerData::VoteGroupHidden* pHidden = m_pPlayer->m_VotesData.GetHidden(m_HiddenID);
			return pHidden && pHidden->m_Value;
		}
	}

	// Default it's openned
	return false;
}

// This function is responsible for rebuilding the votes for a specific client.
void CVoteWrapper::RebuildVotes(int ClientID)
{
	CGS* pGS = (CGS*)Instance::GameServerPlayer(ClientID);
	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	// If the player has no votes, give a chance to come back
	// Code format: {INT}x{INT} (CurrentMenuID, LastMenuID)
	if(m_pData[ClientID].empty())
	{
		CVotePlayerData* pVotesData = &pPlayer->m_VotesData;
		CVoteWrapper VError(ClientID, VWF_STYLE_SIMPLE, "Error");
		VError.Add("The voting list is empty")
			.BeginDepth()
				.Add("Probably a server error")
			.EndDepth()
		.Add("Report the error code #{INT}x{INT}", pVotesData->GetCurrentMenuID(), pVotesData->GetLastMenuID());
		pVotesData->SetLastMenuID(MENU_MAIN);
		CVoteWrapper::AddBackpage(ClientID);
	}

	// Prepare group for hidden vote
	CVoteOption* pLastVoteOption = nullptr;
	for(auto iterGroup = m_pData[ClientID].cbegin(); iterGroup != m_pData[ClientID].cend(); ++iterGroup)
	{
		CVoteGroup* pGroup = *iterGroup;

		// if the group is an expandable list type, it is empty if there is no element in it
		if(pGroup->m_TitleIsSet && pGroup->IsEmpty() && !pGroup->IsHidden())
			pGroup->AddVoteImpl("null", NOPE, NOPE, "The list is empty");

		// Group separator with line
		if(pGroup->m_Flags & VWF_SEPARATE && pGroup != m_pData[ClientID].back())
		{
			if(pGroup->IsHidden())
			{
				auto pVoteGroup = new CVoteGroup(ClientID, VWF_DISABLED);
				pVoteGroup->AddLineImpl();
				iterGroup = std::prev(m_pData[ClientID].insert(std::next(iterGroup), pVoteGroup));
			}
			else if(!pGroup->m_vpVotelist.empty() && !pGroup->m_vpVotelist.back().m_Line)
			{
				pGroup->AddLineImpl();
			}
		}

		// There should not be two lines in a row, or if there are three lines, the middle should be empty, aesthetics.
		for(auto iterVote = pGroup->m_vpVotelist.begin(); iterVote != pGroup->m_vpVotelist.end();)
		{
			if(pLastVoteOption && pLastVoteOption->m_Line && (*iterVote).m_Line)
			{
				iterVote = pGroup->m_vpVotelist.erase(iterVote);
				continue;
			}
			pLastVoteOption = &(*iterVote);
			++iterVote;
		}
	}

	// Rebuild the votes from group
	for(const auto pGroup : m_pData[ClientID])
	{
		for(auto& Option : pGroup->m_vpVotelist)
		{
			// Rebuild the vote options to aesthetic style
			if(pGroup->m_Flags & (VWF_STYLE_SIMPLE | VWF_STYLE_DOUBLE | VWF_STYLE_STRICT | VWF_STYLE_STRICT_BOLD) && !pGroup->IsHidden())
			{
				const int& Flags = pGroup->m_Flags;
				auto pBack = &pGroup->m_vpVotelist.back();
				auto pFront = &pGroup->m_vpVotelist.front();

				// Append border to the vote option (can outside but)
				dynamic_string Buffer;
				if(&Option == pFront)
					Buffer.append(Formatter::Border::get(Formatter::Border::Beggin, Flags));
				else if(&Option == pBack)
					Buffer.append(Formatter::Border::get(Formatter::Border::End, Flags));
				else if(str_comp(Option.m_aCommand, "null") == 0 && Option.m_Depth <= 0 && !Option.m_Line)
					Buffer.append(Formatter::Border::get(Formatter::Border::Middle, Flags));
				else
					Buffer.append(Formatter::Border::get(Formatter::Border::MiddleOption, Flags));

				// Display the level of the vote option
				// Dissable for line is a line as it is
				if(!Option.m_Line && Option.m_Depth > 0)
				{
					for(int i = 0; i < Option.m_Depth; i++)
						Buffer.append(Formatter::Border::get(Formatter::Border::Level, Flags));
				}

				if(str_comp(Option.m_aDescription, Formatter::g_VoteStrLineDef) != 0 && str_comp(Option.m_aCommand, "null") == 0)
					Buffer.append(" ");

				// Save changes
				char aRebuildBuffer[VOTE_DESC_LENGTH];
				str_copy(aRebuildBuffer, Option.m_aDescription, sizeof(aRebuildBuffer));
				str_format(Option.m_aDescription, sizeof(Option.m_aDescription), "%s%s", Buffer.buffer(), aRebuildBuffer);
			}

			// Send the vote option to the client
			CNetMsg_Sv_VoteOptionAdd OptionMsg;
			OptionMsg.m_pDescription = Option.m_aDescription;
			pGS->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
		}
	}
}

// This function returns the vote option for a specific action for a given client
CVoteOption* CVoteWrapper::GetOptionVoteByAction(int ClientID, const char* pActionName)
{
	// Get a reference to the data associated with the given ClientID
	auto& vData = CVoteWrapper::Data()[ClientID];

	// Find the vote option with the matching action name
	for(auto& iterGroup : vData)
	{
		auto iter = std::find_if(iterGroup->m_vpVotelist.begin(), iterGroup->m_vpVotelist.end(), [pActionName](const CVoteOption& vote)
		{
			return str_comp(pActionName, vote.m_aDescription) == 0;
		});

		if(iter != iterGroup->m_vpVotelist.end())
		{
			return &(*iter);
		}
	}

	// If no matching vote option is found, return nullptr
	return nullptr;
}

// Function to create or update a hidden vote group
CVotePlayerData::VoteGroupHidden* CVotePlayerData::EmplaceHidden(int ID, int Type)
{
	// Check if the vote group already exists and has the same type
	auto& CurrentHidden = m_aHiddenGroup[m_CurrentMenuID];
	if(CurrentHidden.find(ID) != CurrentHidden.end() && CurrentHidden[ID].m_Flag == Type)
		return &CurrentHidden[ID];

	bool Value = (Type & VWF_CLOSED) || (Type & VWF_UNIQUE);
	CurrentHidden[ID] = { Value, Type };
	return &CurrentHidden[ID];
}

// Function to get a hidden vote group
CVotePlayerData::VoteGroupHidden* CVotePlayerData::GetHidden(int ID)
{
	auto& CurrentHidden = m_aHiddenGroup[m_CurrentMenuID];
	auto it = CurrentHidden.find(ID);
	if(it != CurrentHidden.end())
		return &it->second;

	return nullptr;
}

// Function to reset the hidden vote groups for a menu
void CVotePlayerData::ResetHidden(int MenuID)
{
	// If the hidden group is empty, return
	auto& HiddenGroup = m_aHiddenGroup[MenuID];
	if(HiddenGroup.empty())
		return;

	// Iterate over each hidden vote group
	for(auto& [ID, Hide] : HiddenGroup)
	{
		if(Hide.m_Flag & VWF_UNIQUE)
			Hide.m_Value = true;
	}
}

// This function is responsible for updating the vote data for a player in a separate thread.
void CVotePlayerData::ThreadVoteUpdater(CVotePlayerData* pData)
{
	// Required sleep for to work properly and correct output of texts and data
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	if(!pData || !pData->m_pPlayer)
		return;

	// TODO: Same prepared vote thread update
	pData->m_VoteUpdaterStatus.store(STATE_UPDATER::DONE);
}

// This function applies the vote updater data to the player's vote data
void CVotePlayerData::ApplyVoteUpdaterData()
{
	if(m_VoteUpdaterStatus == STATE_UPDATER::DONE)
	{
		ClearVotes();
		int ClientID = m_pPlayer->GetCID();
		m_pGS->Core()->OnPlayerHandleMainMenu(ClientID, m_CurrentMenuID);
		CVoteWrapper::RebuildVotes(ClientID);
		m_VoteUpdaterStatus.store(STATE_UPDATER::WAITING);
	}
}

void CVotePlayerData::UpdateVotes(int MenuID)
{
	m_CurrentMenuID = MenuID;

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
	int ClientID = m_pPlayer->GetCID();
	CVoteWrapper::Data()[ClientID].clear();
	Formatter::gs_GroupsNumeral[ClientID] = 0;

	// send vote options
	CNetMsg_Sv_VoteClearOptions ClearMsg;
	Instance::Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);
}

// Function to parse default system commands
bool CVotePlayerData::ParsingDefaultSystemCommands(const char* CMD, const int VoteID, const int VoteID2, int Get, const char* Text)
{
	// Check if the command is "MENU"
	if(PPSTR(CMD, "MENU") == 0)
	{
		// Set the temporary menu integer to the specified value
		m_TempMenuInteger = VoteID2;

		// Update the votes for the player
		ResetHidden(VoteID);
		UpdateVotes(VoteID);
		return true;
	}

	// Check if the command is "BACK"
	if(PPSTR(CMD, "BACK") == 0)
	{
		// Update the votes for the player
		UpdateVotes(m_LastMenuID);
		return true;
	}

	// Check if the command is "HIDDEN"
	if(PPSTR(CMD, "HIDDEN") == 0)
	{
		// If the hidden vote group does not exist, return true
		if(VoteGroupHidden* pHidden = GetHidden(VoteID))
		{
			// If the hidden vote group has the VWF_UNIQUE flag and its ID is not the specified ID, set its value to true
			if(pHidden->m_Flag & VWF_UNIQUE)
			{
				for(auto& [ID, Hide] : m_aHiddenGroup[m_CurrentMenuID])
				{
					if(Hide.m_Flag & VWF_UNIQUE && ID != VoteID)
						Hide.m_Value = true;
				}
			}

			// Toggle the value of the hidden vote group
			pHidden->m_Value = !pHidden->m_Value;
			UpdateCurrentVotes();
		}
		return true;
	}

	return false;
}