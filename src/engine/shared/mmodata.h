/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/storage.h>
#include "datafile.h"

class CDataMMO
{
	int m_CurrentSize;
	unsigned char* m_pCurrentData;
	CDataFileReader m_DataFile;

public:
	CDataMMO() : m_CurrentSize(0), m_pCurrentData(0x0) {}
	~CDataMMO()
	{
		if(m_DataFile.IsOpen())
			m_DataFile.Close();

		mem_free(m_pCurrentData);
		m_pCurrentData = 0x0;
	}

	void SetCurrentSize(int Size) { m_CurrentSize = Size; }
	int GetCurrentSize() const { return m_CurrentSize; }

	const char* GetJsonItem(int Index)
	{
		const char* pItem = (const char*)m_DataFile.FindItem(Index, 0);
		return pItem;
	};

	void SetCurrentData(unsigned char* CurrentData) { m_pCurrentData = CurrentData; }
	unsigned char* GetCurrentData() const { return m_pCurrentData; }
	void Unload()
	{
		m_DataFile.Close();
		m_CurrentSize = 0;
		mem_free(m_pCurrentData);
		m_pCurrentData = 0x0;
	}

	bool Load(IStorageEngine *pStorage)
	{
		if (!pStorage)
			return false;

		if (!m_DataFile.Open(pStorage, MMO_DATA_FILE, IStorageEngine::TYPE_ALL))
			return false;

		IOHANDLE File = pStorage->OpenFile(MMO_DATA_FILE, IOFLAG_READ, IStorageEngine::TYPE_ALL);
		SetCurrentSize((int)io_length(File));
		SetCurrentData((unsigned char*)mem_alloc(GetCurrentSize(), 1));
		io_read(File, GetCurrentData(), GetCurrentSize());
		io_close(File);
		return true;
	}

	bool IsLoaded() const
	{
		return m_DataFile.IsOpen();
	}

	SHA256_DIGEST Sha256() const
	{
		return m_DataFile.Sha256();
	}

	unsigned Crc() const
	{
		return m_DataFile.Crc();
	}
};