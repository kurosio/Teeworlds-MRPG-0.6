#include "vote_optional.h"
#include <game/server/gamecontext.h>

CGS* CVoteOptional::GS() const
{
    return (CGS*)Instance::GameServerPlayer(m_ClientID);
}

CPlayer* CVoteOptional::GetPlayer() const
{
    return GS()->GetPlayer(m_ClientID);
}

bool CVoteOptional::ExecuteVote(bool voteState)
{
    if(m_Callback)
    {
        if(CPlayer* pPlayer = GetPlayer())
            m_Callback(pPlayer, voteState);

        CNetMsg_Sv_VoteStatus voteStatusMsg;
        voteStatusMsg.m_Total = 1;
        voteStatusMsg.m_Yes = voteState;
        voteStatusMsg.m_No = !voteState;
        voteStatusMsg.m_Pass = 0;
        Server()->SendPackMsg(&voteStatusMsg, MSGFLAG_VITAL, m_ClientID);

        m_CloseTime = time_get() + (time_freq() / 2);
        return true;
    }

    return false;
}

void CVoteOptional::HandleVoteOptionals(int clientID)
{
    if(Data()[clientID].empty())
        return;

    CVoteOptional* pOptional = &Data()[clientID].front();

    if(!pOptional->m_Active)
    {
        CNetMsg_Sv_VoteSet msg;
        msg.m_Timeout = (pOptional->m_CloseTime - time_get()) / time_freq();
        msg.m_pDescription = pOptional->m_Description.c_str();
        msg.m_pReason = "\0";
        Instance::Server()->SendPackMsg(&msg, MSGFLAG_VITAL, clientID);

        pOptional->m_Active = true;
    }

    if((pOptional->m_CloseCondition && pOptional->m_CloseCondition(pOptional->GetPlayer())) || pOptional->m_CloseTime < time_get())
    {
        CNetMsg_Sv_VoteSet msg;
        msg.m_Timeout = 0;
        msg.m_pDescription = "";
        msg.m_pReason = "";
        Instance::Server()->SendPackMsg(&msg, MSGFLAG_VITAL, clientID);

        Data()[clientID].pop();
    }
}