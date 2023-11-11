/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_VOTE_EVENT_OPTIONAL_H
#define GAME_SERVER_VOTE_EVENT_OPTIONAL_H

class CVoteEventOptional
{
	typedef void (*OptionEventCallback)(class CPlayer*, int, int, int);

public:
	int m_OptionID {}; // The reserve id of the vote
	int m_OptionID2 {}; // The reserve id of the vote
	bool m_Working {}; // Indicates if the voting event is currently active
	std::string m_Description {}; // The description of the voting event
	time_t m_CloseTime {}; // The time when the voting event will close
	OptionEventCallback m_Callback {}; // The callback function for the event

	// This function registers a callback function for the voting event
	void RegisterCallback(OptionEventCallback Callback) { m_Callback = std::move(Callback); }
};

#endif
