/**
 * any类：想要实现一个类，能够存放任意类型的数据，同时也能够进行任意类型的赋值
 *      example：
 *      {
 *          Any a;
 *          a = 10;
 *          int *pa = a.get<int>();
 *          a = std::string("hello world");
 *          std::string *ps = a.get<std::string>();
 *      }
 * any类的设计思想：主要思想是利用继承和多态的机制，在any类中保存父类指针，父类和子类可以无痛转换，赋值的时候销毁之前的对象，然后保存当前对象的父类指针
 * 在any类中创建一个基类holder和一个派生类placeholder，
 * 基类holder声明为抽象类，把对应的clone和type函数设置为纯虚函数，以便确认后面在继承的时候一定要重写该函数
 * 在派生类中保存对应的对象，在any类中保存基类的指针，就可以通过这个指针找到基类对象，转化为派生类对象做相关操作
*/

#include <iostream>
#include <utility>
#include <typeinfo>
#include <string>
#include <unistd.h>

class Any
{
public:                          // 暴露给外界的成员函数
    Any() : _content(nullptr) {} // 默认构造

    template <class T>
    Any(const T &val) : _content(new placeholder<T>(val)) {} // 通过对应类型构造

    Any(const Any &other) : _content(other._content ? other._content->clone() : nullptr) {} // 拷贝构造

    ~Any() { delete _content; } // 析构函数

    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }
    template <class T>
    Any &operator=(const T &val) // 运算符重载，通过T类型给Any赋值
    {
        Any(val).swap(*this);
        return *this;
    }
    Any &operator=(Any other) // 运算符重载，通过Any类型给Any赋值
    {
        swap(other);
        return *this;
    }
    template <class T>
    T *get() // 获取当前容器保存的对象指针
    {
        if (typeid(T) != _content->type())
            return nullptr;
        return &dynamic_cast<placeholder<T> *>(_content)->_val;
    }

private: // 内部类
    class holder
    {
    public:
        virtual ~holder() {}
        virtual const std::type_info &type() = 0;
        virtual holder *clone() = 0;
    };
    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T &val) : _val(val) {}
        ~placeholder() {}
        virtual const std::type_info &type() { return typeid(T); }
        virtual holder *clone() { return new placeholder(_val); }

    public:
        T _val;
    };

private: // 类内成员变量
    holder *_content;
};

class Test
{
public:
    Test() { std::cout << "构造" << std::endl; }
    Test(const Test &t) { std::cout << "拷贝构造" << std::endl; }
    ~Test() { std::cout << "析构" << std::endl; }
};

int main()
{
    Any a;
    {
        Test t;
        a = t;
    }
    // a = 10;
    // int *pa = a.get<int>();
    // std::cout << *pa << std::endl;
    // a = std::string("你好");
    // std::string *ps = a.get<std::string>();
    // std::cout << *ps << std::endl;
    while(true) sleep(1);
    return 0;
}