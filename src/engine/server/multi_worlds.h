#ifndef ENGINE_SERVER_MULTI_WORLDS_H
#define ENGINE_SERVER_MULTI_WORLDS_H
#include <base/hash.h>
#include <engine/shared/world_detail.h>

class IKernel;
class IEngineMap;
class CWorld;

class CMapDetail
{
	friend class CMultiWorlds;
	CWorld* m_pWorldDetail {};

	IEngineMap* m_pMap{};
	SHA256_DIGEST m_aSha256{};
	unsigned m_aCrc{};
	unsigned char* m_apData{};
	unsigned int m_aSize{};

public:
	CMapDetail(CWorld* pWorldDetail)
	{
		m_pWorldDetail = pWorldDetail;
		m_pMap = nullptr;
		m_aCrc = 0;
		m_apData = nullptr;
		m_aSize = 0;
	}
	~CMapDetail();

	bool Load(IStorageEngine* pStorage);
	void Unload();

	IEngineMap* GetMap() const { return m_pMap; }
	bool IsLoaded() const { return m_pMap != nullptr; }
	unsigned GetCrc() const { return m_aCrc; }
	const SHA256_DIGEST& GetSha256() const { return m_aSha256; }
	unsigned char* GetData() const { return m_apData; }
	unsigned int GetSize() const { return m_aSize; }
};

class CWorld
{
	friend class CMultiWorlds;

	int m_WorldID {};
	char m_aName[64] {};
	char m_aPath[512] {};
	class IGameServer* m_pGameServer {};
	CMapDetail* m_pMapDetail {};
	CWorldDetail m_Detail{};

public:
	CWorld(int WorldID, const std::string& Name, const std::string& Path, CWorldDetail&& Data)
	{
		m_WorldID = WorldID;
		m_Detail = std::move(Data);
		m_pMapDetail = new CMapDetail(this);

		str_copy(m_aName, Name.c_str(), sizeof(m_aName));
		str_copy(m_aPath, Path.c_str(), sizeof(m_aPath));
	}
	~CWorld();

	IGameServer* GameServer() const { return m_pGameServer; }
	CMapDetail* MapDetail() const { return m_pMapDetail; }

	const char* GetName() const { return m_aName; }
	const char* GetPath() const { return m_aPath; }
	CWorldDetail* GetDetail() { return &m_Detail; }
};

class CMultiWorlds
{
	int m_WasInitilized {};
	bool m_NextIsReloading {};
	CWorld* m_apWorlds[ENGINE_MAX_WORLDS] {};

public:
	CMultiWorlds()
	{
		m_WasInitilized = 0;
		m_NextIsReloading = false;
	}
	~CMultiWorlds()
	{
		Clear(true);
	}
	
	CWorld* GetWorld(int WorldID) const { return m_apWorlds[WorldID]; }
	bool IsValid(int WorldID) const { return WorldID >= 0 && WorldID < ENGINE_MAX_WORLDS && m_apWorlds[WorldID] && m_apWorlds[WorldID]->m_pGameServer; }
	int GetSizeInitilized() const { return m_WasInitilized; }
	bool LoadWorlds(class IKernel* pKernel, class IStorageEngine* pStorage, class IConsole* pConsole);

private:
	bool Init(CWorld* pNewWorld, class IKernel* pKernel);
	void Clear(bool Shutdown = true);
};


#endif