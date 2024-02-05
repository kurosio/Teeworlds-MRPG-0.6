#include "vote_wrapper.h"

#include <game/server/gamecontext.h>

const char* VOTE_EMPTY_LINE_DEF = "\u257E\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u257C";

CVoteWrapper::~CVoteWrapper()
{
	RebuildByFlags();
}

bool CVoteWrapper::IsEmpty() const
{
	return m_ButtonSize <= 0;
}

CVoteWrapper& CVoteWrapper::AddIfItemValue(bool Checker, int ItemID)
{
	if(CPlayer* pPlayer = GS()->GetPlayer(m_ClientID); pPlayer)
	{
		return Checker ? AddVoteImpl("null", NOPE, NOPE, "You have {VAL} {STR}",
			pPlayer->GetItem(ItemID)->GetValue(), GS()->GetItemInfo(ItemID)->GetName()) : *this;
	}

	return *this;
}

void CVoteWrapper::InitWrapper()
{
	IServer* pServer = Instance::GetServer();
	m_pGS = (CGS*)pServer->GameServer(pServer->GetClientWorldID(m_ClientID));
}


void CVoteWrapper::RebuildByFlags() const
{
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS)
		return;

	if(m_Flags & (BORDER_SIMPLE | DOUBLE_BORDER | BORDER_STRICT | BORDER_STRICT_BOLD) && !Data()[m_ClientID].empty())
	{
		CPlayer* pPlayer = GS()->m_apPlayers[m_ClientID];
		CVoteGroupHidden* pHidden = pPlayer->GetHidden(m_HideHash);
		if(!pHidden || !pHidden->m_Value)
		{
			const char* m_pBorder[4];
			if(m_Flags & BORDER_SIMPLE)
			{
				m_pBorder[0] = "╭";
				m_pBorder[1] = "│";
				m_pBorder[2] = "├";
				m_pBorder[3] = "╰";
			}
			else if(m_Flags & DOUBLE_BORDER)
			{
				m_pBorder[0] = "╔";
				m_pBorder[1] = "║";
				m_pBorder[2] = "╠";
				m_pBorder[3] = "╚";
			}
			else if(m_Flags & BORDER_STRICT)
			{
				m_pBorder[0] = "┌";
				m_pBorder[1] = "│";
				m_pBorder[2] = "├";
				m_pBorder[3] = "└";
			}
			else
			{
				m_pBorder[0] = "┏";
				m_pBorder[1] = "┃";
				m_pBorder[2] = "┣";
				m_pBorder[3] = "┗";
			}

			auto& pVoteFront = m_vpVoteGroup.front();
			auto& pVoteBack = m_vpVoteGroup.back();
			for(auto& pVote : m_vpVoteGroup)
			{
				char aReBuf[VOTE_DESC_LENGTH];
				if(pVote == pVoteFront)
				{
					const char* pAppend = (str_comp(pVote->m_aDescription, VOTE_EMPTY_LINE_DEF) != 0 && str_comp(pVote->m_aCommand, "null") == 0) ? " " : "\0";
					str_format(aReBuf, sizeof(aReBuf), "%s%s%s", m_pBorder[0], pAppend, pVote->m_aDescription);
				}
				else if(pVote == pVoteBack)
				{
					const char* pAppend = (str_comp(pVote->m_aDescription, VOTE_EMPTY_LINE_DEF) != 0 && str_comp(pVote->m_aCommand, "null") == 0) ? " " : "\0";
					str_format(aReBuf, sizeof(aReBuf), "%s%s%s", m_pBorder[3], pAppend, pVote->m_aDescription);
				}
				else if(str_comp(pVote->m_aCommand, "null") == 0)
				{
					str_format(aReBuf, sizeof(aReBuf), "%s %s", m_pBorder[1], pVote->m_aDescription);
				}
				else
				{
					str_format(aReBuf, sizeof(aReBuf), "%s%s", m_pBorder[2], pVote->m_aDescription);
				}

				str_copy(pVote->m_aDescription, aReBuf, sizeof(pVote->m_aDescription));
			}
		}
	}
}

CVoteWrapper& CVoteWrapper::AddVoteImpl(const char* pCmd, int TempInt, int TempInt2, const char* pText, ...)
{
	// Check if the client ID is valid and if the player exists
	if(m_ClientID < 0 || m_ClientID >= MAX_PLAYERS || !GS()->m_apPlayers[m_ClientID])
		return *this;

	// Get the player hidden vote object associated with the client ID
	CPlayer* pPlayer = GS()->m_apPlayers[m_ClientID];
	CVoteGroupHidden* pHidden = pPlayer->GetHidden(m_HideHash);

	// Check if the hidden vote object exists and if its value is true
	if(pHidden && pHidden->m_Value)
		return *this;

	char aBufText[VOTE_DESC_LENGTH];
	const auto& vPlayerVotes = Data()[m_ClientID];

	// Check if pText is empty or null
	if(!pText || pText[0] == '\0')
	{
		str_copy(aBufText, VOTE_EMPTY_LINE_DEF, sizeof(aBufText));

		// Check if the description of the last player vote is equal to the given text
		if(!vPlayerVotes.empty() && str_comp(vPlayerVotes.back().m_aDescription, aBufText) == 0)
		{
			m_vpVoteGroup.push_back(&Data()[m_ClientID].back());
			return *this;
		}
	}
	else
	{
		// Format the text using the player's language and additional arguments
		va_list VarArgs;
		va_start(VarArgs, pText);
		dynamic_string Buffer;
		Instance::GetServer()->Localization()->Format_VL(Buffer, pPlayer->GetLanguage(), pText, VarArgs);
		va_end(VarArgs);

		const char* pAppend = "\0";

		// Check if pHidden is false and m_Flags is not HIDE_DISABLE
		if(!pHidden && m_Flags & (HIDE_DEFAULT_CLOSE | HIDE_DEFAULT_OPEN | HIDE_UNIQUE))
		{
			// Hash the buffer and set pHidden to the result
			HashHide(Buffer.buffer());
			pHidden = pPlayer->EmplaceHidden(m_HideHash, m_Flags);

			// Set pAppend based on the value of HiddenTab
			const bool HiddenTab = pHidden->m_Value;
			pAppend = HiddenTab ? "\u21BA " : "\u27A4 ";
			TempInt = m_HideHash;
			pCmd = "HIDDEN";
		}
		// Check if pCmd is not equal to "null"
		else if(str_comp(pCmd, "null") != 0)
		{
			pAppend = "╾ ";
		}

		str_format(aBufText, sizeof(aBufText), "%s%s", pAppend, Buffer.buffer());
		Buffer.clear();

		// Check if the player's language is "ru" or "uk" and convert aBufText to Latin if true
		if(str_comp(pPlayer->GetLanguage(), "ru") == 0 || str_comp(pPlayer->GetLanguage(), "uk") == 0)
			str_translation_cyrlic_to_latin(aBufText);
	}

	// Create a new VoteOption with the values from aBufText, pCmd, TempInt, and TempInt2
	CVoteOption Vote;
	str_copy(Vote.m_aDescription, aBufText, sizeof(Vote.m_aDescription));
	str_copy(Vote.m_aCommand, pCmd, sizeof(Vote.m_aCommand));
	Vote.m_SettingID = TempInt;
	Vote.m_SettingID2 = TempInt2;

	// Increment m_ButtonSize by 1 First it's title
	if(!m_vpVoteGroup.empty() && pText && pText[0] != '\0')
		m_ButtonSize++;

	// Add the VoteOption to the player's votes
	Data()[m_ClientID].emplace_back(Vote);
	m_vpVoteGroup.emplace_back(&Data()[m_ClientID].back());

	return *this;
}
