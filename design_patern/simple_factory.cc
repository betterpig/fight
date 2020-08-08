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

class Factory
{
public:
    Base* ptr;
    Factory(string type)
    {
        if(type=="A")
            ptr=new A;
        if(type=="B")
            ptr=new B;
    }
    ~Factory() {delete ptr;}
    void print() {ptr->print();}
};

int main()
{
    Factory a("A");
    Factory b("B");
    a.print();
    b.print();
}