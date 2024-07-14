#ifndef GAME_SERVER_VOTE_OPTIONAL_H
#define GAME_SERVER_VOTE_OPTIONAL_H

class CGS;
class CPlayer;

class CVoteOptional : public MultiworldIdentifiableData<std::map<int, std::queue<CVoteOptional>>>
{
    CGS* GS() const;
    CPlayer* GetPlayer() const;

    using OptionEventCallback = std::function<void(CPlayer*, int, int, bool)>;

public:
    // Factory method to create a new vote event
    template<typename ... Args>
    static CVoteOptional* Create(int clientID, int miscValue1, int miscValue2, int durationSeconds, const char* descriptionFormat, const Args& ... args)
	{
        CVoteOptional voteEvent;
        voteEvent.m_CloseTime = time_get() + time_freq() * durationSeconds;
        voteEvent.m_MiscValue1 = miscValue1;
        voteEvent.m_MiscValue2 = miscValue2;
        voteEvent.m_ClientID = clientID;
        voteEvent.m_Description = fmt_localize(clientID, descriptionFormat, args...);
        m_pData[clientID].push(voteEvent);
        return &m_pData[clientID].back();
    }

    // Register a callback to be triggered when the vote executes
    void RegisterCallback(OptionEventCallback callback)
	{
        m_Callback = std::move(callback);
    }

    // Execute the vote and return whether the callback was called
    bool ExecuteVote(bool voteState);

    // Handle optional voting options for a player
    static void HandleVoteOptionals(int clientID);

private:
    int m_ClientID {};
    int m_MiscValue1 {};
    int m_MiscValue2 {};
    bool m_Active { false };
    std::string m_Description {};
    time_t m_CloseTime {};
    OptionEventCallback m_Callback;
};

#endif // GAME_SERVER_VOTE_OPTIONAL_H