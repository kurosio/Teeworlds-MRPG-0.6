#ifndef THREAD_POOL_H
#define THREAD_POOL_H

class ThreadPool
{
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;
    void wait();
    ~ThreadPool();

private:
    // need to keep track of threads so we can join them
    std::vector<std::thread> workers;
    // the task queue
    std::queue<std::function<void()>> tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads) : stop(false)
{
    for(size_t i = 0; i < threads; ++i)
    {
        workers.emplace_back([this]
        {
            for(;;)
            {
                std::function<void()> task;

                {
                    std::unique_lock lock(this->queue_mutex);
                    this->condition.wait(lock,
                        [this]{ return this->stop || !this->tasks.empty(); });
                    if(this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
        });
    }
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
{
    using return_type = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

inline void ThreadPool::wait()
{
    std::unique_lock lock(queue_mutex);
    condition.wait(lock, [this]{ return this->tasks.empty(); });
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread& worker : workers)
        worker.join();
}

#define sleep_pause(microsec) set_pause_function([](){}, microsec)

template <typename T>
void set_timer_detach(T header, int milliseconds)
{
    std::thread t([=]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        header();
    });
    t.detach();
}

template <typename T>
void set_pause_function(T header, int milliseconds)
{
    std::thread t([=]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        header();
    });
    t.join();
}

#endif
