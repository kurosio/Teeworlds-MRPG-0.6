#ifndef ENGINE_SERVER_SNAPSHOT_IDS_POOL_CONTEXT_H
#define ENGINE_SERVER_SNAPSHOT_IDS_POOL_CONTEXT_H

class CSnapIDPool
{
	enum
	{
		MAX_IDS = 32 * 1024,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = allocated, 2 = timed
		int m_Timeout;
	};

	CID m_aIDs[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:
	CSnapIDPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewID();
	void TimeoutIDs();
	void FreeID(int ID);
};


#endif