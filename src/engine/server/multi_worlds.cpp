#include "multi_worlds.h"

#include <engine/map.h>
#include <engine/server.h>
#include <engine/storage.h>

CMapDetail::~CMapDetail()
{
	Unload();
	delete m_pMap;
}

bool CMapDetail::Load(IStorageEngine* pStorage)
{
	char aBuf[IO_MAX_PATH_LENGTH];
	str_format(aBuf, sizeof(aBuf), "maps/%s", m_pWorldDetail->GetPath());

	if(!m_pMap->Load(aBuf))
		return false;

	// load complete map into memory for download
	{
		void* pData;
		pStorage->ReadFile(aBuf, IStorageEngine::TYPE_ALL, &pData, &m_aSize);
		m_apData = (unsigned char*)pData;
		m_aSha256 = m_pMap->Sha256();
		m_aCrc = m_pMap->Crc();
	}

	return true;
}

void CMapDetail::Unload()
{
	if(m_pMap && m_pMap->IsLoaded())
	{
		m_pMap->Unload();
		m_aSize = 0;
		m_aCrc = 0;
		m_aSha256 = {};
		free(m_apData);
	}
}

CWorld::~CWorld()
{
	delete m_pGameServer;
	delete m_pMapDetail;
}

bool CMultiWorlds::Init(CWorld* pNewWorld, IKernel* pKernel)
{
	pNewWorld->m_pGameServer = CreateGameServer();

	bool RegisterFail = false;
	if(m_NextIsReloading) // reregister
	{
		RegisterFail = RegisterFail || !pKernel->ReregisterInterface(pNewWorld->m_pMapDetail->m_pMap, pNewWorld->m_ID);
		RegisterFail = RegisterFail || !pKernel->ReregisterInterface(static_cast<IMap*>(pNewWorld->m_pMapDetail->m_pMap), pNewWorld->m_ID);
		RegisterFail = RegisterFail || !pKernel->ReregisterInterface(pNewWorld->m_pGameServer, pNewWorld->m_ID);
	}
	else // register
	{
		pNewWorld->m_pMapDetail->m_pMap = CreateEngineMap();
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pNewWorld->m_pMapDetail->m_pMap, false, pNewWorld->m_ID);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap*>(pNewWorld->m_pMapDetail->m_pMap), false, pNewWorld->m_ID);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pNewWorld->m_pGameServer, false, pNewWorld->m_ID);
	}

	m_WasInitilized++;
	return RegisterFail;
}

bool CMultiWorlds::LoadFromDB(IKernel* pKernel)
{
	// clear old worlds
	 Clear(false);

	ResultPtr pRes = Database->Execute<DB::SELECT>("*", "tw_worlds");
 	while(pRes->next())
	{
		const int WorldID = pRes->getInt("ID");
		dbg_assert(WorldID < ENGINE_MAX_WORLDS, "exceeded pool of allocated memory for worlds");

		std::string Name = pRes->getString("Name");
		std::string Path = pRes->getString("Path");
		std::string Type = pRes->getString("Type");
		const int RespawnWorldID = pRes->getInt("RespawnWorldID");
		const int JailWorldID = pRes->getInt("JailWorldID");
		const int RequiredLevel = pRes->getInt("RequiredLevel");

		CWorldDetail WorldDetail(Type, RespawnWorldID, JailWorldID, RequiredLevel);
		if(m_apWorlds[WorldID])
		{
			str_copy(m_apWorlds[WorldID]->m_aName, Name.c_str(), sizeof(m_apWorlds[WorldID]->m_aName));
			str_copy(m_apWorlds[WorldID]->m_aPath, Path.c_str(), sizeof(m_apWorlds[WorldID]->m_aPath));
			m_apWorlds[WorldID]->m_ID = WorldID;
			m_apWorlds[WorldID]->m_Detail = WorldDetail;
		}
		else
		{
			m_apWorlds[WorldID] = new CWorld(WorldID, Name, Path, WorldDetail);
		}

		Init(m_apWorlds[WorldID], pKernel);

	}

	m_NextIsReloading = true;
	return true;
}

void CMultiWorlds::Clear(bool Shutdown)
{
	for(int i = 0; i < m_WasInitilized; i++)
	{
		if(Shutdown)
		{
			delete m_apWorlds[i];
			continue;
		}
	
		m_apWorlds[i]->m_pMapDetail->Unload();

		delete m_apWorlds[i]->m_pGameServer;
		m_apWorlds[i]->m_pGameServer = nullptr;
	}

	m_WasInitilized = 0;
}
