/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_VOTE_EVENT_OPTIONAL_H
#define GAME_SERVER_VOTE_EVENT_OPTIONAL_H

class CGS;
class CPlayer;

class CVoteOptional : public MultiworldIdentifiableStaticData< std::map<int, std::queue<CVoteOptional>>>
{
	CGS* GS() const;
	CPlayer* GetPlayer() const;

	using OptionEventCallback = std::function<void(CPlayer*, int, int, bool)>;

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
	static CVoteOptional* Create(int ForCID, int MiscValue1, int MiscValue2, int Secound, const char* pInformation, const Ts&... args)
	{
		// Create an instance and push it
		CVoteOptional Optional;
		Optional.m_CloseTime = time_get() + time_freq() * Secound;
		Optional.m_MiscValue1 = MiscValue1;
		Optional.m_MiscValue2 = MiscValue2;
		Optional.m_ClientID = ForCID;
		Optional.m_Description = fmt_handle_def(ForCID, pInformation, args...);
		m_pData[ForCID].push(Optional);
		return &m_pData[ForCID].back();
	}

	int m_ClientID {};
	int m_MiscValue1 {};
	int m_MiscValue2 {};
	bool m_Working {};
	std::string m_Description {};
	time_t m_CloseTime {};

	// This function registers a callback function for the voting event
	void RegisterCallback(OptionEventCallback Callback) { m_Callback = std::move(Callback); }

	// This function is used to run a callback function with the given parameters.
	// It takes a pointer to a CPlayer object, a boolean state, and returns a boolean value.
	bool Run(bool VoteState);

private:
	OptionEventCallback m_Callback; // Callback function for the voting event
};

#endif
