#ifndef GAME_SERVER_CORE_SCENARIOS_BASE_COMPONENT_REGISTRY_H
#define GAME_SERVER_CORE_SCENARIOS_BASE_COMPONENT_REGISTRY_H

#include "scenario_base.h"

template<typename T>
concept IsComponent = std::is_base_of_v<IStepComponent, T>&& std::is_constructible_v<T, const nlohmann::json&>;

class ComponentRegistry
{
	ComponentRegistry() = default;
	~ComponentRegistry() = default;
	ComponentRegistry(const ComponentRegistry&) = delete;
	ComponentRegistry& operator=(const ComponentRegistry&) = delete;

public:
	using FactoryFunc = std::function<std::unique_ptr<IStepComponent>(const nlohmann::json&)>;
	static ComponentRegistry& GetInstance()
	{
		static ComponentRegistry Instance;
		return Instance;
	}

	template<IsComponent T>
	void Register(std::string_view name)
	{
		m_Factories[std::string(name)] = [](const nlohmann::json& j) -> std::unique_ptr<IStepComponent>
		{
			return std::make_unique<T>(j);
		};
	}

	std::unique_ptr<IStepComponent> Create(std::string_view name, const nlohmann::json& j, ScenarioBase* pScenario) const
	{
		auto it = m_Factories.find(std::string(name));
		if(it != m_Factories.end())
		{
			auto pComponent = it->second(j);
			if(pComponent)
			{
				pComponent->Init(pScenario);
			}

			return pComponent;
		}

		return nullptr;
	}

private:
	std::map<std::string, FactoryFunc> m_Factories;
};

template<IsComponent T>
class ComponentRegistrar
{
public:
	explicit ComponentRegistrar(std::string_view name)
	{
		ComponentRegistry::GetInstance().Register<T>(name);
	}
};

#endif