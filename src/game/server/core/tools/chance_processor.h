#ifndef GAME_SERVER_CORE_TOOLS_CHANCE_PROCESSOR_H
#define GAME_SERVER_CORE_TOOLS_CHANCE_PROCESSOR_H

#include <base/math.h>

template<typename T>
class ChanceProcessor
{
private:
	struct ElementWithChance
	{
		T Element;
		double Chance;
	};

	std::vector<ElementWithChance> m_vElements {};
	double m_TotalChance {};

public:
	void clear()
	{
		m_vElements.clear();
		m_TotalChance = 0.0;
	}

	void addElement(const T& Element, double Chance)
	{
		if(Chance <= 0)
			return;

		m_vElements.push_back({ Element, Chance });
		m_TotalChance += Chance;
	}

	T getRandomElement()
	{
		if(m_vElements.empty())
			return {};

		auto randomValue = random_float(0.f, (float)m_TotalChance);
		double cumulativeChance = 0.0;

		for(const auto& e : m_vElements)
		{
			cumulativeChance += e.Chance;
			if(randomValue <= cumulativeChance)
			{
				return e.Element;
			}
		}

		return m_vElements.back().Element;
	}

	bool isEmpty() const { return m_vElements.empty(); }
	std::size_t size() const { return m_vElements.size(); }
};

#endif