/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GROUP_DATA_H
#define GAME_SERVER_COMPONENT_GROUP_DATA_H
#include <game/server/core/tools/dbset.h>

#define TW_GROUPS_TABLE "tw_groups"

using GroupIdentifier = int;
class GroupData : public MultiworldIdentifiableStaticData< std::map< int, GroupData > >
{
	GroupIdentifier m_ID {};
	int m_LeaderUID {};
	int m_TeamColor {};
	ska::unordered_set<int> m_vAccountIds {};

public:
	GroupData() = default;
	GroupData(GroupIdentifier ID) : m_ID(ID)
	{
		m_vAccountIds.reserve(MAX_GROUP_MEMBERS);
	}

	// Initialize the group with the owner's UID, color, and a set of account IDs
	void Init(int LeaderUID, int Color, class DBSet&& SetAccountIDs)
	{
		// init access list
		for(auto& p : SetAccountIDs.GetDataItems())
		{
			// Convert the data item to an integer
			if(int UID = std::atoi(p.first.c_str()); UID > 0)
			{
				// If the integer is greater than 0, add it to the access user IDs set
				m_vAccountIds.insert(UID);
			}
		}

		m_TeamColor = Color;
		m_LeaderUID = LeaderUID;
		m_pData[m_ID] = *this;
	}

	// Add an account to the group
	// Returns true if the account was successfully added, false otherwise
	bool Add(int AccountID);

	// Remove an account from the group
	// Returns true if the account was successfully removed, false otherwise
	bool Remove(int AccountID);

	// Disband the group, removing all accounts and resetting the owner and color
	void Disband();

	// Check if the group has a specific account ID
	bool HasAccountID(int AccountID) const { return m_vAccountIds.find(AccountID) != m_vAccountIds.end(); }

	// Change the owner of the group to a different account
	void ChangeLeader(int AccountID);

	// Change the color of the group
	void ChangeColor(int NewColor);
	
	bool IsFull() const { return (int)m_vAccountIds.size() >= (int)MAX_GROUP_MEMBERS; } // This function checks if the group is full or not
	int GetTeamColor() const { return m_TeamColor; } // Get the color of the group
	GroupIdentifier GetID() const { return m_ID; } // Get the ID of the group
	int GetLeaderUID() const { return m_LeaderUID; } // Get the owner's UID
	int IsLeader(int AccountID) const { return m_LeaderUID == AccountID; } // Check leader state
	const ska::unordered_set<int>& GetAccounts() const { return m_vAccountIds; } // Get a reference to the map of account IDs in the group

private:
	void Save() const;
};

#endif