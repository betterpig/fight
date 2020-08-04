#ifndef STRBLOBPTR_H
#define STRBLOBPTR_H

#include <vector>
#include <string>
#include <memory>
#include "str_blob.h"
using namespace std;

class StrBlobPtr
{
public:
    StrBlobPtr():curr(0)  { }
    StrBlobPtr(StrBlob& a,size_t sz=0):wptr(a.data),curr(sz) { }//用a对象中的共享指针初始化本类的弱指针，弱指针现在指向共享指针所指向的vector
    string& deref() const;
    StrBlobPtr& operator++();
    StrBlobPtr& operator--();
    StrBlobPtr operator++(int);
    StrBlobPtr operator--(int);
    StrBlobPtr& operator+(const int);
    StrBlobPtr& operator-(const int);
    string& operator*() const;
    string* operator->() const;

private:
    shared_ptr<vector<string>> check(size_t,const string&) const;
    weak_ptr<vector<string>> wptr;//弱指针，指向共享指针管理的对象，但不会增加共享指针的引用计数
    size_t curr;
};

shared_ptr<vector<string>> StrBlobPtr::check(size_t i,const string& msg) const
{
    auto ret=wptr.lock();//lock函数返回一个指向弱指针所管理对象的shared_ptr，若对象为空，则返回空的shared_ptr
    if(!ret)
        throw runtime_error("unbound StrBlobPtr");
    if(i>=ret->size())
        throw out_of_range(msg);
    return ret;
}

string& StrBlobPtr::deref() const
{
    auto p=check(curr,"dereference past end");//check成功时返回指向共享对象的shared_ptr指针
    return (*p)[curr];//对shared_ptr指针解引用，就得到vector<string>,然后再按下标，返回对应位置的string的引用
}

StrBlobPtr& StrBlobPtr::operator++()
{
    check(curr,"increment past end of StrBlobPtr");
    ++curr;
    return *this;
}

StrBlobPtr& StrBlobPtr::operator--()
{
    --curr;
    check(curr,"decrement past begin of StrBlobPtr");
    return *this;
}

StrBlobPtr StrBlobPtr::operator++(int)
{
    StrBlobPtr ret=*this;
    ++(*this);
    return ret;
}

StrBlobPtr StrBlobPtr::operator--(int)
{
    StrBlobPtr ret=*this;
    --(*this);
    return ret;
}

StrBlobPtr& StrBlobPtr::operator+(const int offset)
{
    StrBlobPtr ret=*this;
    ret.check(curr+offset,"increment past end of StrBlobPtr");
    ret.curr+=offset;
    return ret;
}

StrBlobPtr& StrBlobPtr::operator+(const int offset)
{
    StrBlobPtr ret=*this;
    ret.check(curr-offset,"increment past end of StrBlobPtr");
    ret.curr-=offset;
    return ret;
}

string& StrBlobPtr::operator*() const
{
    shared_ptr<vector<string>> ret=check(curr,"dereference past end");
    return (*ret)[curr];
}

string* StrBlobPtr::operator->() const
{
    return & this->operator*();
}

#endif