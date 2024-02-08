#include "vote_wrapper.h"

#include <game/server/gamecontext.h>

const char* VOTE_LINE_DEF = "\u257E\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u257C";

namespace Border
{
	enum BorderSymbol { Beggin, Middle, MiddleOption, End, Num };

	inline constexpr const char* g_pSimpleBorder[Num] = { "╭", "│", "├", "╰" };
	inline constexpr const char* g_pDoubleBorder[Num] = { "╔", "║", "╠", "╚" };
	inline constexpr const char* g_pStrictBorder[Num] = { "┌", "│", "├", "└" };
	inline constexpr const char* g_pStrictBoldBorder[Num] = { "┏", "┃", "┣", "┗" };

	constexpr const char* get(BorderSymbol Border, int Flags)
	{
		if(Flags & BORDER_SIMPLE) { return g_pSimpleBorder[Border]; }
		if(Flags & DOUBLE_BORDER) { return g_pDoubleBorder[Border]; }
		if(Flags & BORDER_STRICT) { return g_pStrictBorder[Border]; }
		if(Flags & BORDER_STRICT_BOLD) { return g_pStrictBoldBorder[Border]; }
		return "";
	}
}

// This function calculates the hash value for a given string and stores it in the specified output array.
inline constexpr void HasherVoteGroup(const char* pStr, int* Out) noexcept
{
	*Out = 0;
	while(*pStr) { *Out = *Out << 1 ^ *pStr++; }
}

CVoteGroup::CVoteGroup(int ClientID, int Flags) : m_Flags(Flags), m_ClientID(ClientID)
{
	IServer* pServer = Instance::GetServer();
	int ClientWorldID = pServer->GetClientWorldID(m_ClientID);
	m_pGS = (CGS*)pServer->GameServer(ClientWorldID);
}

// Function to add a vote title implementation with variable arguments
void CVoteGroup::AddVoteTitleImpl(const char* pCmd, int SettingsID1, int SettingsID2, const char* pText, ...)
{
	// Check if the player is valid
	CPlayer* pPlayer = GS()->GetPlayer(m_ClientID);
	if(!pPlayer)
		return;

	// Format the text using the player's language and additional arguments
	va_list VarArgs;
	va_start(VarArgs, pText);
	dynamic_string Buffer;
	Instance::GetServer()->Localization()->Format_VL(Buffer, pPlayer->GetLanguage(), pText, VarArgs);
	va_end(VarArgs);

	const char* pAppend = "\0";
	if(m_Flags & (HIDE_DEFAULT_CLOSE | HIDE_DEFAULT_OPEN | HIDE_UNIQUE))
	{
		// Hash the buffer and set pHidden to the result
		HasherVoteGroup(Buffer.buffer(), &m_GroupHash);
		const bool HiddenTab = pPlayer->EmplaceHidden(m_GroupHash, m_Flags)->m_Value;

		// Set pAppend based on the value of HiddenTab
		pAppend = HiddenTab ? "\u21BA " : "\u27A4 ";
		SettingsID1 = m_GroupHash;
		pCmd = "HIDDEN";
	}

	char aBufText[VOTE_DESC_LENGTH];
	str_format(aBufText, sizeof(aBufText), "%s%s", pAppend, Buffer.buffer());
	Buffer.clear();

	// Check if the player's language is "ru" or "uk" and convert aBufText to Latin if true
	if(str_comp(pPlayer->GetLanguage(), "ru") == 0 || str_comp(pPlayer->GetLanguage(), "uk") == 0)
		str_translation_cyrlic_to_latin(aBufText);

	// Create a new VoteOption with the values from aBufText, pCmd, SettingsID1, and SettingsID2
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, aBufText, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_SettingID = SettingsID1;
	Vote.m_SettingID2 = SettingsID2;
	Vote.m_Title = true;

	// Add the VoteOption to the player's votes
	m_vpVotelist.emplace_back(Vote);
}

// Function to add a vote implementation with variable arguments
void CVoteGroup::AddVoteImpl(const char* pCmd, int Settings1, int Settings2, const char* pText, ...)
{
	// Check if the client ID is valid and if the player exists
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS || !GS()->m_apPlayers[m_ClientID])
		return;

	// Get the player hidden vote object associated with the client ID
	CPlayer* pPlayer = GS()->m_apPlayers[m_ClientID];
	CVoteGroupHidden* pHidden = pPlayer->GetHidden(m_GroupHash);

	// Check if the hidden vote object exists and if its value is true
	if(pHidden && pHidden->m_Value)
		return;

	// Format the text using the player's language and additional arguments
	va_list VarArgs;
	va_start(VarArgs, pText);
	dynamic_string Buffer;
	Instance::GetServer()->Localization()->Format_VL(Buffer, pPlayer->GetLanguage(), pText, VarArgs);
	va_end(VarArgs);

	char aBufText[VOTE_DESC_LENGTH];
	str_format(aBufText, sizeof(aBufText), "%s%s", str_comp(pCmd, "null") != 0 ? "╾ " : "\0", Buffer.buffer());
	Buffer.clear();

	// Check if the player's language is "ru" or "uk" and convert aBufText to Latin if true
	if(str_comp(pPlayer->GetLanguage(), "ru") == 0 || str_comp(pPlayer->GetLanguage(), "uk") == 0)
		str_translation_cyrlic_to_latin(aBufText);

	// Create a new VoteOption with the values from aBufText, pCmd, Settings1, and Settings2
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, aBufText, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_SettingID = Settings1;
	Vote.m_SettingID2 = Settings2;

	// Add the VoteOption to the player's votes
	m_ButtonSize++;
	m_vpVotelist.emplace_back(Vote);
}

// This function is used to add a line
void CVoteGroup::AddLineImpl()
{
	// Check if the client ID is valid
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		// Create a new VoteOption with the values
		CVoteOption Vote;
		str_copy(Vote.m_aDescription, VOTE_LINE_DEF, sizeof(Vote.m_aDescription));
		str_copy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));
		Vote.m_Line = true;

		// Add the VoteOption to the player's votes
		m_vpVotelist.emplace_back(Vote);
	}
}

// This function adds the implementation for the AddBackpage method in the CVoteGroup class.
void CVoteGroup::AddBackpageImpl()
{
	// Check if the client ID is valid
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		// Create a new VoteOption with the values
		CVoteOption Vote;
		str_copy(Vote.m_aDescription, "\u21A9 Backpage", sizeof(Vote.m_aDescription));
		str_copy(Vote.m_aCommand, "back", sizeof(Vote.m_aCommand));

		// Add the VoteOption to the player's votes
		AddLineImpl();
		m_vpVotelist.emplace_back(Vote);
	}
}

// This function adds an empty line
void CVoteGroup::AddEmptylineImpl()
{
	// Check if the client ID is valid
	if(m_ClientID >= 0 && m_ClientID < MAX_PLAYERS)
	{
		// Check if the last player vote is an line
		CVoteOption Vote;
		str_copy(Vote.m_aDescription, "\0", sizeof(Vote.m_aDescription));
		str_copy(Vote.m_aCommand, "null", sizeof(Vote.m_aCommand));

		// Add the VoteOption to the player's votes
		m_vpVotelist.emplace_back(Vote);
	}
}

// Function to add an item value information to the CVoteGroup
void CVoteGroup::AddItemValueImpl(int ItemID)
{
	if(CPlayer* pPlayer = GS()->GetPlayer(m_ClientID))
		AddVoteImpl("null", NOPE, NOPE, "You have {VAL} {STR}", pPlayer->GetItem(ItemID)->GetValue(), GS()->GetItemInfo(ItemID)->GetName());
}

// This function is responsible for rebuilding the votes for a specific client.
void CVoteWrapper::RebuildVotes(int ClientID)
{
	IServer* pServer = Instance::GetServer();
	int ClientWorldID = pServer->GetClientWorldID(ClientID);
	CGS* pGS = (CGS*)pServer->GameServer(ClientWorldID);
	CPlayer* pPlayer = pGS->GetPlayer(ClientID);
	if(!pPlayer)
		return;

	bool BorderNext = false;
	CVoteOption* pLastVoteOption = nullptr;
	for(auto pItem : m_pData[ClientID])
	{
		for(auto iter = pItem->m_vpVotelist.begin(); iter != pItem->m_vpVotelist.end();)
		{
			// There should not be two lines in a row, or if there are three lines, the middle should be empty, aesthetics.
			// Rebuild the vote option to have an empty line aesthetic
			if(pLastVoteOption && pLastVoteOption->m_Line && (*iter).m_Line)
			{
				iter = pItem->m_vpVotelist.erase(iter);
				continue;
			}

			// Rebuild the vote options to aesthetic style
			if(pItem->m_Flags & (BORDER_SIMPLE | DOUBLE_BORDER | BORDER_STRICT | BORDER_STRICT_BOLD))
			{
				// Skip the vote option if the player has hidden the vote option
				CVoteGroupHidden* pHidden = pPlayer->GetHidden(pItem->m_GroupHash);
				if(!pHidden || (pHidden && !pHidden->m_Value))
				{
					char aRebuildedBuf[VOTE_DESC_LENGTH];
					const int& Flags = pItem->m_Flags;
					auto itpFront = &pItem->m_vpVotelist.front();
					auto itpBack = &pItem->m_vpVotelist.back();

					if(str_comp((*iter).m_aDescription, itpFront->m_aDescription) == 0)
					{
						const char* pAppend = (str_comp((*iter).m_aDescription, VOTE_LINE_DEF) != 0 && str_comp((*iter).m_aCommand, "null") == 0) ? " " : "\0";
						str_format(aRebuildedBuf, sizeof(aRebuildedBuf), "%s%s%s", get(Border::Beggin, Flags), pAppend, (*iter).m_aDescription);
					}
					else if(str_comp((*iter).m_aDescription, itpBack->m_aDescription) == 0)
					{
						const char* pAppend = (str_comp((*iter).m_aDescription, VOTE_LINE_DEF) != 0 && str_comp((*iter).m_aCommand, "null") == 0) ? " " : "\0";
						str_format(aRebuildedBuf, sizeof(aRebuildedBuf), "%s%s%s", get(Border::End, Flags), pAppend, (*iter).m_aDescription);
					}
					else if(str_comp((*iter).m_aCommand, "null") == 0)
					{
						str_format(aRebuildedBuf, sizeof(aRebuildedBuf), "%s %s", get(Border::Middle, Flags), (*iter).m_aDescription);
					}
					else
					{
						str_format(aRebuildedBuf, sizeof(aRebuildedBuf), "%s%s", get(Border::MiddleOption, Flags), (*iter).m_aDescription);
					}

					str_copy((*iter).m_aDescription, aRebuildedBuf, sizeof((*iter).m_aDescription));
				}
			}

			// Send the vote option to the client
			CNetMsg_Sv_VoteOptionAdd OptionMsg;
			OptionMsg.m_pDescription = (*iter).m_aDescription;
			pGS->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);

			// Set the last vote option to the current vote option
			pLastVoteOption = &(*iter);
			++iter;
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
			return (str_comp_nocase(pActionName, vote.m_aDescription) == 0);
		});

		if(iter != iterGroup->m_vpVotelist.end())
		{
			return &(*iter);
		}
	}

	// If no matching vote option is found, return nullptr
	return nullptr;
}
