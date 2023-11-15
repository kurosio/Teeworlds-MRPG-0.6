/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_VOTE_EVENT_OPTIONAL_H
#define GAME_SERVER_VOTE_EVENT_OPTIONAL_H

class CVoteEventOptional
{
	typedef void (*OptionEventCallback)(class CPlayer*, int, int, int);
	OptionEventCallback m_Callback {}; // The callback function for the event

public:
	int m_OptionID {}; // The reserve id of the vote
	int m_OptionID2 {}; // The reserve id of the vote
	bool m_Working {}; // Indicates if the voting event is currently active
	std::string m_Description {}; // The description of the voting event
	time_t m_CloseTime {}; // The time when the voting event will close

	// This function registers a callback function for the voting event
	void RegisterCallback(OptionEventCallback Callback) { m_Callback = std::move(Callback); }

	// This function is used to run a callback function with the given parameters.
	// It takes a pointer to a CPlayer object, a boolean state, and returns a boolean value.
	bool Run(class CPlayer* pPlayer, bool State) const
	{
		// Check if the callback function is valid
		if(m_Callback)
		{
			// Call the callback function with the given parameters
			m_Callback(pPlayer, m_OptionID, m_OptionID2, State);
			return true;
		}

		// Return false to indicate that the callback function was not called
		return false;
	}
};

#endif
