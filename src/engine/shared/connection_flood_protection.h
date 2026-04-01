/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SHARED_CONNECTION_FLOOD_PROTECTION_H
#define ENGINE_SHARED_CONNECTION_FLOOD_PROTECTION_H

#include <base/system.h>

class CConnectionFloodProtection
{
public:
	enum EDecision
	{
		DECISION_ALLOW = 0,
		DECISION_DROP_GLOBAL,
		DECISION_DROP_SUBNET,
	};

	void Reset();
	EDecision OnConnectionAttempt(const NETADDR &Addr, int OccupiedSlots, int MaxSlots);
	void OnSuccessfulConnect(const NETADDR &Addr);

private:
	struct CSubnetBucket
	{
		uint64_t m_Key;
		double m_Tokens;
		int64_t m_LastUpdate;
		int64_t m_BlockUntil;
		int m_Strikes;
	};

	struct CTrustEntry
	{
		uint64_t m_Key;
		int64_t m_LastSeen;
	};

	static constexpr int SUBNET_BUCKETS = 2048;
	static constexpr int TRUST_BUCKETS = 4096;

	double m_GlobalTokens = 0.0;
	int64_t m_LastTokenUpdate = 0;
	int64_t m_GlobalBlockUntil = 0;
	int m_GlobalStrikes = 0;
	CSubnetBucket m_aSubnetBuckets[SUBNET_BUCKETS]{};
	CTrustEntry m_aTrustEntries[TRUST_BUCKETS]{};

	uint64_t MakeSubnetKey(const NETADDR &Addr) const;
	uint64_t MakeAddressKey(const NETADDR &Addr) const;
	void UpdateGlobalTokens(int64_t Now);
	void UpdateSubnetTokens(CSubnetBucket &Bucket, int64_t Now) const;
	double TokenCost(int OccupiedSlots, int MaxSlots, bool TrustedAddress) const;
	bool IsTrustedAddress(const NETADDR &Addr, int64_t Now) const;
};

#endif
