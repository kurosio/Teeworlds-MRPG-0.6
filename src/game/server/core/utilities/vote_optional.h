/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_VOTE_EVENT_OPTIONAL_H
#define GAME_SERVER_VOTE_EVENT_OPTIONAL_H

#include "format.h"

class CGS;
class CPlayer;

class CVoteOptional : public MultiworldIdentifiableStaticData< std::map<int, std::queue<CVoteOptional>>>
{
	CGS* GS() const;
	CPlayer* GetPlayer() const;

	typedef void (*OptionEventCallback)(class CPlayer*, int, int, int);
	OptionEventCallback m_Callback {}; // The callback function for the event

public:
	// Function: CreateVoteOptional
	// Parameters:
	//    - MiscValue1: an integer value representing the vote option
	//    - MiscValue2: an integer value representing the vote option
	//    - Secound: an integer value representing the duration of the vote event
	//    - pInformation: a pointer to a character array containing optional information for the vote
	//    - ...: additional optional arguments for the information text format
	// Return:
	//    - a pointer to a CVoteOptional object representing the created vote event
	template< typename ... Ts>
	static CVoteOptional* Create(int ClientID, int MiscValue1, int MiscValue2, int Secound, const char* pInformation, const Ts&... args)
	{
		// Create an instance and push it
		CVoteOptional Optional;
		Optional.m_CloseTime = time_get() + time_freq() * Secound;
		Optional.m_MiscValue1 = MiscValue1;
		Optional.m_MiscValue2 = MiscValue2;
		Optional.m_ClientID = ClientID;
		Optional.m_Description = Tools::String::FormatLocalize(ClientID, pInformation).c_str();
		m_pData[ClientID].push(Optional);
		return &m_pData[ClientID].back();
	}

	int m_ClientID {}; // The client ID of the player who created the vote
	int m_MiscValue1 {}; // The reserve misc value of the vote
	int m_MiscValue2 {}; // The reserve misc value of the vote
	bool m_Working {}; // Indicates if the voting event is currently active
	std::string m_Description {}; // The description of the voting event
	time_t m_CloseTime {}; // The time when the voting event will close

	// This function registers a callback function for the voting event
	void RegisterCallback(OptionEventCallback Callback) { m_Callback = Callback; }

	// This function is used to run a callback function with the given parameters.
	// It takes a pointer to a CPlayer object, a boolean state, and returns a boolean value.
	bool Run(bool VoteState);
};

#endif
