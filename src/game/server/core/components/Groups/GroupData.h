/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_COMPONENT_GROUP_DATA_H
#define GAME_SERVER_COMPONENT_GROUP_DATA_H
#include <unordered_set>

#define TW_GROUPS_TABLE "tw_groups"

using GroupIdentifier = int;
class GroupData : public MultiworldIdentifiableStaticData< std::map< int, GroupData > >
{
	GroupIdentifier m_ID {};
	int m_OwnerUID {};
	int m_TeamColor {};
	ska::unordered_set<int> m_AccountIds {};

public:
	GroupData() = default;
	GroupData(GroupIdentifier ID) : m_ID(ID) { }

	// Initialize the group with the owner's UID, color, and a set of account IDs
	void Init(int OwnerUID, int Color, class DBSet&& SetAccountIDs);

	// Add an account to the group
	// Returns true if the account was successfully added, false otherwise
	bool Add(int AccountID);

	// Remove an account from the group
	// Returns true if the account was successfully removed, false otherwise
	bool Remove(int AccountID);

	// Disband the group, removing all accounts and resetting the owner and color
	void Disband();

	// Check if the group has a specific account ID
	// Returns true if the account ID is in the group, false otherwise
	bool HasAccountID(int AccountID) const { return m_AccountIds.find(AccountID) != m_AccountIds.end(); }

	// Change the owner of the group to a different account
	void ChangeOwner(int AccountID);

	// Change the color of the group
	void ChangeColor(int NewColor);

	// Get the color of the group
	int GetTeamColor() const { return m_TeamColor; }

	// Get the ID of the group
	GroupIdentifier GetID() const { return m_ID; }

	// Get the owner's UID
	int OwnerUID() const { return m_OwnerUID; }

	// Get a reference to the map of account IDs in the group
	const ska::unordered_set<int>& GetAccounts() const { return m_AccountIds; }

private:
	void Save() const;
};

#endif