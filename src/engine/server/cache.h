#ifndef ENGINE_SERVER_CBROWSERCACHE_H
#define ENGINE_SERVER_CBROWSERCACHE_H

class CBrowserCache
{
public:
	class CChunk
	{
	public:
		CChunk(const void* pData, int Size);
		CChunk(const CChunk&) = delete;
		CChunk(CChunk&&) = default;

		std::vector<uint8_t> m_vData;
	};

	std::vector<CChunk> m_vCache;

	CBrowserCache();
	~CBrowserCache();

	void AddChunk(const void* pData, int Size);
	void Clear();
};

#endif