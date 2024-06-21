#include "vote_optional.h"

#include <game/server/gamecontext.h>

CGS* CVoteOptional::GS() const { return (CGS*)Instance::GameServerPlayer(m_ClientID); }
CPlayer* CVoteOptional::GetPlayer() const { return GS()->GetPlayer(m_ClientID); }

bool CVoteOptional::Run(bool VoteState)
{
	// Check if the callback function is valid
	if(m_Callback)
	{
		// Call the callback function with the given parameters
		m_Callback(GetPlayer(), m_MiscValue1, m_MiscValue2, VoteState);

		// Create a new network message to update the vote status
		CNetMsg_Sv_VoteStatus Msg;
		Msg.m_Total = 1;
		Msg.m_Yes = VoteState;
		Msg.m_No = !VoteState;
		Msg.m_Pass = 0;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, m_ClientID);

		// Set the close time of pOptional to half a second from the current time
		m_CloseTime = time_get() + (time_freq() / 2);
		return true;
	}

	return false;
}

