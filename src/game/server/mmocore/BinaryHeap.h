/* Binary heap class for pathfind by Sushi */
#ifndef GAME_SERVER_BINARYHEAP_H
#define GAME_SERVER_BINARYHEAP_H
#include <base/tl/array.h>

template <class T>
class CBinaryHeap
{
public:
	CBinaryHeap()
	{
		m_CurrentSize = 0;
		m_lItems.clear();
	}

	void SetSize(int NewSize)
	{
		m_lItems.set_size(NewSize + 1);
	}

	const T* GetMin()
	{
		return &m_lItems[1];
	}

	void RemoveMin()
	{
		m_lItems[1] = m_lItems[m_CurrentSize--];
		PercolateDown(1);
	}

	void Insert(const T& Item)
	{
		// Percolate up
		int Hole = ++m_CurrentSize;
		for (; Hole > 1 && Item < m_lItems[Hole / 2]; Hole /= 2)
			m_lItems[Hole] = m_lItems[Hole / 2];
		m_lItems[Hole] = Item;
	}

	void Replace(const T& Item)
	{
		int Index = 0;
		// replace
		for (int i = 0; i < m_CurrentSize; i++)
		{
			if (Item == m_lItems[i])
			{
				m_lItems[i] = Item;
				Index = i;
				break;
			}
		}

		// Percolate up
		int Hole = Index;
		for (; Hole > 1 && m_lItems[Index] < m_lItems[Hole / 2]; Hole /= 2)
			m_lItems[Hole] = m_lItems[Hole / 2];
		m_lItems[Hole] = m_lItems[Index];
	}

	void MakeEmpty()
	{
		m_CurrentSize = 0;
	}

	int GetSize() const
	{
		return m_CurrentSize;
	}

private:
	array<T> m_lItems;

	// fake current size
	int m_CurrentSize;

	void PercolateDown(int Hole)
	{
		int Child;
		T Tmp = m_lItems[Hole];

		for (; Hole * 2 <= m_CurrentSize; Hole = Child)
		{
			Child = Hole * 2;
			if (Child != m_CurrentSize && m_lItems[Child + 1] < m_lItems[Child])
				Child++;
			if (m_lItems[Child] < Tmp)
				m_lItems[Hole] = m_lItems[Child];
			else
				break;
		}
		m_lItems[Hole] = Tmp;
	}
};

#endif
