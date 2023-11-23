#include "cache.h"

CBrowserCache::CBrowserCache()
{
	m_vCache.clear();
}

CBrowserCache::~CBrowserCache()
{
	Clear();
}

CBrowserCache::CChunk::CChunk(const void* pData, int Size)
{
	m_vData.assign((const uint8_t*)pData, (const uint8_t*)pData + Size);
}

void CBrowserCache::AddChunk(const void* pData, int Size)
{
	m_vCache.emplace_back(pData, Size);
}

void CBrowserCache::Clear()
{
	m_vCache.clear();
}