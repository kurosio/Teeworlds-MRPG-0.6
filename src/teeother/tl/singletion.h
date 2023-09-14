#ifndef SHARED_TL_SINGLETON_H
#define	SHARED_TL_SINGLETON_H

#include <memory>

template<class T>
class CSingleton
{
protected:
	static std::unique_ptr<T> m_pSingleton;

public:
	virtual ~CSingleton() = default;

	static T* Get()
	{
		if(!m_pSingleton)
			m_pSingleton.reset(new T());
		return m_pSingleton.get();
	}

	static void Reset()
	{
		if(m_pSingleton)
			m_pSingleton.reset();
	}
};

template<class T>
std::unique_ptr<T> CSingleton<T>::m_pSingleton = 0;

#endif