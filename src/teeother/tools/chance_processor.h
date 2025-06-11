#ifndef TEEOTHER_TOOLS_CHANCE_PROCESSOR_H
#define TEEOTHER_TOOLS_CHANCE_PROCESSOR_H

template<typename T> requires std::equality_comparable<T>
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
	void clear() noexcept
	{
		m_vElements.clear();
		m_TotalChance = 0.0;
	}

	void addElement(const T& Element, double Chance)
	{
		if(Chance <= 0.0) [[unlikely]]
			return;

		m_vElements.push_back({ Element, Chance });
		m_TotalChance += Chance;
	}

	bool removeElement(const T& element)
	{
		auto it = std::ranges::find(m_vElements, element, &ElementWithChance::Element);

		if(it != m_vElements.end())
		{
			m_TotalChance -= it->Chance;
			m_vElements.erase(it);
			return true;
		}

		return false;
	}


	const ElementWithChance* getElement(const T& element) const
	{
		auto it = std::ranges::find(m_vElements, element, &ElementWithChance::Element);
		return (it != m_vElements.end()) ? &(*it) : nullptr;
	}

	ElementWithChance* getElement(const T& element)
	{
		auto it = std::ranges::find(m_vElements, element, &ElementWithChance::Element);
		return (it != m_vElements.end()) ? &(*it) : nullptr;
	}

	bool hasElement(const T& element) const
	{
		const auto it = std::ranges::find(m_vElements, element, &ElementWithChance::Element);
		return it != m_vElements.end();
	}

	void normalizeChances()
	{
		if(m_vElements.empty() || m_TotalChance <= 0.0) [[unlikely]]
		{
			m_TotalChance = 0.0;
			return;
		}

		const double scale = 100.0 / m_TotalChance;
		std::ranges::for_each(m_vElements, [scale](auto& element)
		{
			element.Chance *= scale;
		});

		m_TotalChance = 100.0;
	}

	void setEqualChance(double newChance)
	{
		if(newChance <= 0.0 || m_vElements.empty()) [[unlikely]]
			return;

		for(auto& e : m_vElements)
			e.Chance = newChance;

		m_TotalChance = newChance * static_cast<double>(m_vElements.size());
	}

	T getRandomElement() const
	{
		if(m_vElements.empty()) [[unlikely]]
			return {};

		// using thread_local for not create this every execute
		thread_local std::mt19937 gen { std::random_device{}() };
		std::uniform_real_distribution<double> dis(0.0, m_TotalChance);

		auto randomValue = dis(gen);
		auto cumulativeChance = 0.0;

		for(const auto& e : m_vElements)
		{
			cumulativeChance += e.Chance;
			if(randomValue <= cumulativeChance)
				return e.Element;
		}

		return m_vElements.back().Element;
	}

	void sortElementsByChance()
	{
		if(m_vElements.empty()) [[unlikely]]
			return;

		std::sort(m_vElements.begin(), m_vElements.end(), [](const ElementWithChance& a, const ElementWithChance& b) {
			return a.Chance > b.Chance;
		});
	}

	// iterator types
	using Iterator = typename std::vector<ElementWithChance>::iterator;
	using ConstIterator = typename std::vector<ElementWithChance>::const_iterator;

	// begin and end iterators (non-const)
	Iterator begin() noexcept { return m_vElements.begin(); }
	Iterator end() noexcept { return m_vElements.end(); }
	ConstIterator begin() const noexcept { return m_vElements.begin(); }
	ConstIterator end() const noexcept { return m_vElements.end(); }

	// reverse iterators
	std::reverse_iterator<Iterator> rbegin() noexcept { return m_vElements.rbegin(); }
	std::reverse_iterator<Iterator> rend() noexcept { return m_vElements.rend(); }
	std::reverse_iterator<ConstIterator> rbegin() const noexcept { return m_vElements.rbegin(); }
	std::reverse_iterator<ConstIterator> rend() const noexcept { return m_vElements.rend(); }

	// checking
	bool isEmpty() const noexcept { return m_vElements.empty(); }
	std::size_t size() const noexcept { return m_vElements.size(); }
};

#endif