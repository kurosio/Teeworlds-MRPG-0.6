#include <gtest/gtest.h>
#include <thread>

#include "event_listener.h"

class MockListener : public IEventListener
{
public:
    bool onCharacterDamageCalled = false;
    bool onCharacterDeathCalled = false;

    void OnCharacterDamage(int Damage) override {
        onCharacterDamageCalled = true;
    }

    void OnCharacterDeath(int Weapon) override {
        onCharacterDeathCalled = true;
    }
};

TEST(EventListeners, RegisterAndUnregisterListener)
{
    CEventListenerManager manager;
    MockListener listener;

    manager.RegisterListener(IEventListener::CharacterDamage, &listener);
    manager.Notify<IEventListener::CharacterDamage>(10);
    EXPECT_TRUE(listener.onCharacterDamageCalled);

    manager.UnregisterListener(IEventListener::CharacterDamage, &listener);
    listener.onCharacterDamageCalled = false;

    manager.Notify<IEventListener::CharacterDamage>(10);

    EXPECT_FALSE(listener.onCharacterDamageCalled);
}

TEST(EventListeners, NotifyMultipleListeners)
{
    CEventListenerManager manager;
    MockListener listener1;
    MockListener listener2;

    manager.RegisterListener(IEventListener::CharacterDamage, &listener1);
    manager.RegisterListener(IEventListener::CharacterDamage, &listener2);
    manager.Notify<IEventListener::CharacterDamage>(20);

    EXPECT_TRUE(listener1.onCharacterDamageCalled);
    EXPECT_TRUE(listener2.onCharacterDamageCalled);
}

TEST(EventListeners, NotifyInMultipleThreads)
{
    CEventListenerManager manager;
    MockListener listener;

    manager.RegisterListener(IEventListener::CharacterDamage, &listener);
    std::thread t1([&](){
        manager.Notify<IEventListener::CharacterDamage>(30);
    });
    std::thread t2([&](){
        manager.Notify<IEventListener::CharacterDamage>(40);
    });

    t1.join();
    t2.join();

    EXPECT_TRUE(listener.onCharacterDamageCalled);
}

TEST(EventListeners, NotifyDifferentEventTypes)
{
    CEventListenerManager manager;
    MockListener listener;

    manager.RegisterListener(IEventListener::CharacterDamage, &listener);
    manager.RegisterListener(IEventListener::CharacterDeath, &listener);

    manager.Notify<IEventListener::CharacterDamage>(50);
    EXPECT_TRUE(listener.onCharacterDamageCalled);
    listener.onCharacterDamageCalled = false;

    manager.Notify<IEventListener::CharacterDeath>(0);
    EXPECT_TRUE(listener.onCharacterDeathCalled);
}

TEST(EventListeners, NotifyWithNoListeners)
{
    CEventListenerManager manager;
    MockListener listener;

    manager.Notify<IEventListener::CharacterDamage>(50);
    EXPECT_FALSE(listener.onCharacterDamageCalled);
}

CEventListenerManager recursiveManager;
class MockRecursiveListener : public IEventListener
{
public:
    bool onCharacterDamageCalled = false;
    bool onCharacterDeathCalled = false;

    void OnCharacterDamage(int Damage) override
    {
        onCharacterDamageCalled = true;
        recursiveManager.Notify<IEventListener::CharacterDeath>(0);
    }

    void OnCharacterDeath(int Weapon) override
    {
        onCharacterDeathCalled = true;
    }
};

TEST(EventListeners, RecursiveNotify)
{
    MockRecursiveListener listener;
    recursiveManager.RegisterListener(IEventListener::CharacterDamage, &listener);
    recursiveManager.RegisterListener(IEventListener::CharacterDeath, &listener);
    recursiveManager.Notify<IEventListener::CharacterDamage>(60);
    EXPECT_TRUE(listener.onCharacterDamageCalled);
    EXPECT_TRUE(listener.onCharacterDeathCalled);
}
