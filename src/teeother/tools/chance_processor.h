#ifndef TEEOTHER_TOOLS_CHANCE_PROCESSOR_H
#define TEEOTHER_TOOLS_CHANCE_PROCESSOR_H

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

	bool removeElement(const T& element)
	{
		auto it = std::ranges::find_if(m_vElements, [&element](const ElementWithChance& e)
		{
			return e.Element == element;
		});

		if(it != m_vElements.end())
		{
			m_TotalChance -= it->Chance;
			m_vElements.erase(it);
			return true;
		}

		return false;
	}

	bool hasElement(const T& element)
	{
		auto it = std::ranges::find_if(m_vElements, [&element](const ElementWithChance& e)
		{
			return e.Element == element;
		});

		return it != m_vElements.end();
	}

	void normalizeChances()
	{
		if(m_TotalChance == 0.0)
			return;

		for(auto& e : m_vElements)
			e.Chance = (e.Chance / m_TotalChance) * 100.0;

		m_TotalChance = 100.0;
	}

	void setEqualChance(double newChance)
	{
		if(newChance <= 0)
			return;

		m_TotalChance = newChance * m_vElements.size();

		for(auto& e : m_vElements)
			e.Chance = newChance;
	}

	T getRandomElement() const
	{
		if(m_vElements.empty())
			return {};

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> dis(0.0, (double)m_TotalChance);

		auto randomValue = dis(gen);
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

	// iterator types
	using Iterator = typename std::vector<ElementWithChance>::iterator;
	using ConstIterator = typename std::vector<ElementWithChance>::const_iterator;

	// begin and end iterators (non-const)
	Iterator begin() { return m_vElements.begin(); }
	Iterator end() { return m_vElements.end(); }
	ConstIterator begin() const { return m_vElements.begin(); }
	ConstIterator end() const { return m_vElements.end(); }

	// reverse iterators
	std::reverse_iterator<Iterator> rbegin() { return m_vElements.rbegin(); }
	std::reverse_iterator<Iterator> rend() { return m_vElements.rend(); }
	std::reverse_iterator<ConstIterator> rbegin() const { return m_vElements.rbegin(); }
	std::reverse_iterator<ConstIterator> rend() const { return m_vElements.rend(); }

	bool isEmpty() const { return m_vElements.empty(); }
	std::size_t size() const { return m_vElements.size(); }
};

#endif