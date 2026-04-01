/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "connection_flood_protection.h"

#include "config.h"

void CConnectionFloodProtection::Reset()
{
	m_GlobalTokens = g_Config.m_SvConnFloodGlobalBurst;
	m_LastTokenUpdate = time_get();
	m_GlobalBlockUntil = 0;
	m_GlobalStrikes = 0;
	for(auto &Bucket : m_aSubnetBuckets)
	{
		Bucket.m_Key = 0;
		Bucket.m_Tokens = g_Config.m_SvConnFloodSubnetBurst;
		Bucket.m_LastUpdate = 0;
		Bucket.m_BlockUntil = 0;
		Bucket.m_Strikes = 0;
	}
	for(auto &Entry : m_aTrustEntries)
	{
		Entry.m_Key = 0;
		Entry.m_LastSeen = 0;
	}
}

void CConnectionFloodProtection::UpdateGlobalTokens(int64_t Now)
{
	if(m_LastTokenUpdate == 0)
	{
		m_LastTokenUpdate = Now;
		m_GlobalTokens = g_Config.m_SvConnFloodGlobalBurst;
		return;
	}

	const double PassedSeconds = (double)(Now - m_LastTokenUpdate) / (double)time_freq();
	m_LastTokenUpdate = Now;
	m_GlobalTokens = minimum((double)(m_GlobalTokens + PassedSeconds * g_Config.m_SvConnFloodGlobalRate), (double)g_Config.m_SvConnFloodGlobalBurst);
}

double CConnectionFloodProtection::TokenCost(int OccupiedSlots, int MaxSlots, bool TrustedAddress) const
{
	if(MaxSlots <= 0)
		return TrustedAddress ? 0.35 : 1.0;

	const double Occupancy = (double)OccupiedSlots * 100.0 / (double)MaxSlots;
	const double BaseCost = TrustedAddress ? 0.35 : 1.0;
	if(Occupancy <= g_Config.m_SvConnFloodPressure)
		return BaseCost;

	const double Denominator = maximum(1, 100 - g_Config.m_SvConnFloodPressure);
	const double OverPressure = Occupancy - g_Config.m_SvConnFloodPressure;
	return BaseCost + 2.0 * clamp(OverPressure / Denominator, 0.0, 1.0);
}

uint64_t CConnectionFloodProtection::MakeSubnetKey(const NETADDR &Addr) const
{
	uint64_t Key = 1469598103934665603ULL; // FNV offset basis
	Key ^= (uint64_t)(unsigned int)Addr.type;
	Key *= 1099511628211ULL;

	int PrefixBytes = 2;
	if(Addr.type == NETTYPE_IPV4)
		PrefixBytes = 3; // /24
	else if(Addr.type == NETTYPE_IPV6)
		PrefixBytes = 7; // /56

	for(int i = 0; i < PrefixBytes; i++)
	{
		Key ^= Addr.ip[i];
		Key *= 1099511628211ULL;
	}
	return Key;
}

uint64_t CConnectionFloodProtection::MakeAddressKey(const NETADDR &Addr) const
{
	uint64_t Key = 1469598103934665603ULL;
	Key ^= (uint64_t)(unsigned int)Addr.type;
	Key *= 1099511628211ULL;
	for(int i = 0; i < 16; i++)
	{
		Key ^= Addr.ip[i];
		Key *= 1099511628211ULL;
	}
	return Key;
}

void CConnectionFloodProtection::UpdateSubnetTokens(CSubnetBucket &Bucket, int64_t Now) const
{
	if(Bucket.m_LastUpdate == 0)
	{
		Bucket.m_LastUpdate = Now;
		Bucket.m_Tokens = g_Config.m_SvConnFloodSubnetBurst;
		return;
	}

	const double Rate = g_Config.m_SvConnFloodSubnetWindow > 0 ?
							(double)g_Config.m_SvConnFloodSubnetLimit / (double)g_Config.m_SvConnFloodSubnetWindow :
							0.0;
	const double PassedSeconds = (double)(Now - Bucket.m_LastUpdate) / (double)time_freq();
	Bucket.m_LastUpdate = Now;
	Bucket.m_Tokens = minimum<double>(g_Config.m_SvConnFloodSubnetBurst, Bucket.m_Tokens + PassedSeconds * Rate);
}

bool CConnectionFloodProtection::IsTrustedAddress(const NETADDR &Addr, int64_t Now) const
{
	if(g_Config.m_SvConnFloodTrustSeconds <= 0)
		return false;
	const uint64_t Key = MakeAddressKey(Addr);
	const int Index = (int)(Key % TRUST_BUCKETS);
	const auto &Entry = m_aTrustEntries[Index];
	if(Entry.m_Key != Key)
		return false;
	return (Now - Entry.m_LastSeen) <= (int64_t)g_Config.m_SvConnFloodTrustSeconds * time_freq();
}

CConnectionFloodProtection::EDecision CConnectionFloodProtection::OnConnectionAttempt(const NETADDR &Addr, int OccupiedSlots, int MaxSlots)
{
	if(g_Config.m_SvConnFloodGlobalRate <= 0 || g_Config.m_SvConnFloodGlobalBurst <= 0 ||
		g_Config.m_SvConnFloodSubnetLimit <= 0 || g_Config.m_SvConnFloodSubnetWindow <= 0 || g_Config.m_SvConnFloodSubnetBurst <= 0)
	{
		return DECISION_ALLOW;
	}

	const int64_t Now = time_get();
	UpdateGlobalTokens(Now);
	const bool TrustedAddress = IsTrustedAddress(Addr, Now);

	if(m_GlobalBlockUntil > Now && !TrustedAddress)
		return DECISION_DROP_GLOBAL;

	const double Cost = TokenCost(OccupiedSlots, MaxSlots, TrustedAddress);
	const double TrustedGraceCost = Cost * 0.45;
	if(m_GlobalTokens < Cost && (!TrustedAddress || m_GlobalTokens < TrustedGraceCost))
	{
		m_GlobalStrikes = minimum(m_GlobalStrikes + 1, 10);
		m_GlobalBlockUntil = Now + (int64_t)(g_Config.m_SvConnFloodGlobalBlockSeconds + m_GlobalStrikes) * time_freq();
		return DECISION_DROP_GLOBAL;
	}
	m_GlobalStrikes = maximum(m_GlobalStrikes - 1, 0);
	m_GlobalTokens -= Cost;

	const uint64_t Key = MakeSubnetKey(Addr);
	int Candidate = (int)(Key % SUBNET_BUCKETS);
	int Oldest = Candidate;
	bool Found = false;
	for(int i = 0; i < SUBNET_BUCKETS; i++)
	{
		const int Index = (Candidate + i) % SUBNET_BUCKETS;
		auto &Bucket = m_aSubnetBuckets[Index];
		if(Bucket.m_Key == 0 || Bucket.m_Key == Key)
		{
			Candidate = Index;
			Found = true;
			break;
		}

			if(Bucket.m_LastUpdate < m_aSubnetBuckets[Oldest].m_LastUpdate)
				Oldest = Index;
		}
	if(!Found)
		Candidate = Oldest;

	auto &Bucket = m_aSubnetBuckets[Candidate];
	if(Bucket.m_Key != Key)
		Bucket = {};
	Bucket.m_Key = Key;
	UpdateSubnetTokens(Bucket, Now);

	if(Bucket.m_BlockUntil > Now && !TrustedAddress)
		return DECISION_DROP_SUBNET;

	const double SubnetCost = TrustedAddress ? 0.40 : 1.0;
	if(Bucket.m_Tokens < SubnetCost)
	{
		Bucket.m_Strikes = minimum(Bucket.m_Strikes + 1, 10);
		Bucket.m_BlockUntil = Now + (int64_t)(g_Config.m_SvConnFloodSubnetBlockSeconds + Bucket.m_Strikes) * time_freq();
		return DECISION_DROP_SUBNET;
	}
	Bucket.m_Strikes = maximum(Bucket.m_Strikes - 1, 0);
	Bucket.m_Tokens -= SubnetCost;

	return DECISION_ALLOW;
}

void CConnectionFloodProtection::OnSuccessfulConnect(const NETADDR &Addr)
{
	const int64_t Now = time_get();
	const uint64_t Key = MakeAddressKey(Addr);
	const int Index = (int)(Key % TRUST_BUCKETS);
	m_aTrustEntries[Index].m_Key = Key;
	m_aTrustEntries[Index].m_LastSeen = Now;
}
