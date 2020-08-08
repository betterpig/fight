#include <string>
#include <iostream>
using namespace std;

class Base
{
public:
    string tri;
    Base():tri("my class is ") {}
    virtual ~Base() {}
    virtual void print() {}
};

class A: public Base
{
public:
    string name;
    A():name("A") {}
    virtual ~A() override {} 
    virtual void print() override;
};

void A::print()
{
    cout<<tri<<name<<endl;
}

class B: public Base
{
public:
    string name;
    B():name("B") {}
    virtual ~B() override {}
    virtual void print() override;
};

void B::print()
{
    cout<<tri<<name<<endl;
}

class FactoryBase
{
public:
    Base* ptr;
    FactoryBase(Base* p):ptr(p) {}
    ~FactoryBase() {delete ptr;}
    void print() {ptr->print();}
};

class FactoryA: public FactoryBase
{
public:
    FactoryA():FactoryBase(new A) {}
};

class FactoryB: public FactoryBase
{
public:
    FactoryB():FactoryBase(new B) {}
};

int main()
{
    FactoryA a;
    FactoryB b;
    a.print();
    b.print();
}