/**
 * 时间轮的设计思想：封装一个类，在定时任务创建的时候创建这个对象
 * 在定时任务将要被执行的时候，让这个对象被释放
 * 将定时任务需要执行的函数放在这个类的析构函数中调用，当对象被释放的时候会自动调用任务
 *
 * 手动管理比较麻烦，所以创建一个时间轮，存放某一时刻需要执行的定时任务
 * 当走到这个时刻的时候，执行这个时刻要执行的所有任务（释放这个时刻的所有对象）
 *
 * 由于存在延时、取消定时任务的需求，所以需要经常操作这个时间轮，比较麻烦，这里采用了智能指针管理
 * 使用shared_ptr管理对象指针，当引用计数为0时自动销毁
 *
 * 延时定时任务的时候就在时间轮上对应位置再加上一个该对象的shared_ptr，就能够实现延时的目的
 * 取消任务的时候
 */

#include <cstdint>
#include <iostream>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;

class TimerTask
{
public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb)
        : _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
    ~TimerTask()
    {
        if(_canceled == false)
            _task_cb();
        _release();
    }
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }
    uint32_t DelayTime() { return _timeout; }
    void Cancel() { _canceled = true; }
private:
    uint64_t _id;         // 定时器任务对象id
    uint32_t _timeout;    // 定时任务超时时间
    bool _canceled;       // false表示没有被取消，true表示被取消了
    TaskFunc _task_cb;    // 定时器对象要执行的任务
    ReleaseFunc _release; // 用于删除TimeWheel中保存的定时器对象信息
};

class TimerWheel
{
    using WeakTask = std::weak_ptr<TimerTask>;
    using PtrTask = std::shared_ptr<TimerTask>;

public:
    TimerWheel() : _capacity(60), _tick(0), _wheel(_capacity) {}
    ~TimerWheel() {}
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) // 添加定时任务
    {
        PtrTask pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        _wheel[(_tick + delay) % _capacity].push_back(pt);
        _timers[id] = WeakTask(pt);
    }
    void TimerRefresh(uint64_t id) // 刷新、延迟定时任务
    {
        // 通过保存的定时器对象_timers找到对应的weak_ptr，构造shared_ptr，添加到时间轮
        auto it = _timers.find(id);
        if (it == _timers.end())
            return;               // 不存在定时任务，没办法刷新延迟
        PtrTask pt = it->second.lock(); // 获取到weak_ptr对象对应的shared_ptr
        int delay = pt->DelayTime();
        _wheel[(_tick + delay) % _capacity].push_back(pt);
    }
    void TimerCancel(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
            return;               // 不存在定时任务，没办法取消
        it->second.lock()->Cancel();
    }
    void RunTimerTask() // 这个函数相当于每秒钟执行一次，相当于秒针向后走
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空当前位置的数组就会自动销毁对应的对象，此时如果有引用计数为0的对象，将会自动调用析构函数，执行定时任务
    }
    
private:
    /* 用这个函数的时候，一定是shared_ptr的引用已经=0，此时weak_ptr也不需要存在了 */
    void RemoveTimer(uint64_t id) // 从_timers中删除定时任务
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
            _timers.erase(id);
    }

private:
    int _tick;                                      // 当前的秒针，走到哪里就释放哪里
    int _capacity;                                  // 表盘的最大数量（最大延迟时间）
    std::vector<std::vector<PtrTask>> _wheel;       // 时间轮
    std::unordered_map<uint64_t, WeakTask> _timers; // 保存定时任务
};

class Test
{
public:
    Test() { std::cout << "构造" << std::endl; }
    ~Test() { std::cout << "析构" << std::endl; }
};

void DelTest(Test *t)
{
    delete t;
}

#include <unistd.h>

int main()
{
    TimerWheel tw;

    Test *t = new Test();

    tw.TimerAdd(888, 5, std::bind(DelTest, t));

    for (int i = 0; i < 5; ++i)
    {
        sleep(1);
        tw.TimerRefresh(888);
        tw.RunTimerTask();
        std::cout << "refrash the task, wait 5s" << std::endl;
    }
    while (true)
    {
        sleep(1);
        // tw.TimerCancel(888);
        std::cout << "-------------------------" << std::endl;
        tw.RunTimerTask();
    }
    return 0;
}