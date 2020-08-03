#include <mutex>

class Singleton_lazy
{
private:
    static Singleton_lazy* instance;
    Singleton_lazy(){}
public:
    static Singleton_lazy* GetInstance()
    {
        if(instance==nullptr)
            instance=new Singleton_lazy();
        return instance;
    }
};
Singleton_lazy* Singleton_lazy::instance=nullptr;


class Singleton_lazy_double_lock
{
private:
    static Singleton_lazy_double_lock* instance;
    static std::mutex mu;
    Singleton_lazy_double_lock(){}
public:
    static Singleton_lazy_double_lock* GetInstance()
    {
        if(instance==nullptr)
        {
            mu.lock();
            if(instance==nullptr)
                instance=new Singleton_lazy_double_lock();
            mu.unlock();
        }
        return instance;
    }
};
Singleton_lazy_double_lock* Singleton_lazy_double_lock::instance=nullptr;
std::mutex Singleton_lazy_double_lock::mu;


class Singleton_lazy_inner_static
{
private:
    Singleton_lazy_inner_static(){}
public:
    static Singleton_lazy_inner_static* GetInstance()
    {
        static Singleton_lazy_inner_static* instance;
        return instance;
    }
};


class Singleton_hungry
{
private:
    static Singleton_hungry* instance;
    Singleton_hungry(){}
public:
    static Singleton_hungry* GetInstance()
    {
        return instance;
    }
};
Singleton_hungry* Singleton_hungry::instance=new Singleton_hungry();