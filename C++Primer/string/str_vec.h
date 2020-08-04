#include <string>
#include <memory>
#include <utility>

using namespace std;

class StrVec
{
public:
    StrVec():elements(nullptr),first_free(nullptr),cap(nullptr){}
    StrVec(initializer_list<string> il);
    StrVec& operator=(initializer_list<string> il);
    StrVec(const StrVec&);
    StrVec& operator=(const StrVec&);
    StrVec::StrVec(StrVec&& s) noexcept;
    StrVec& StrVec::operator=(StrVec&& ) noexcept;
    ~StrVec();
    string& operator[](size_t n) {return elements[n];}
    const string& operator[] (size_t n) const {return elements[n];}
    
    
    size_t size() const {return first_free-elements;}
    size_t capacity() const {return cap-elements;}
    string* begin() const {return elements;}
    string* end() const {return first_free;}

    void push_back(const string&);
    void push_back(string&&);
    void reserve(size_t n);
    void resize(size_t n);
    void resize(size_t n,const string& s);

private:
    static allocator<string> alloc;
    void check_alloc()
    {
        if(size()==capacity())
            reallocate();
    }
    pair<string*,string*> alloc_copy(const string*,const string*);
    void free();
    void reallocate();
    string* elements;
    string* first_free;
    string* cap;

};

StrVec::StrVec(initializer_list<string> il)
{
    pair<string*,string*> data=alloc_copy(il.begin(),il.end());
    elements=data.first;
    first_free=cap=data.second;
}

StrVec& StrVec::operator=(initializer_list<string> il)
{
    pair<string*,string*> data=alloc_copy(il.begin(),il.end());
    elements=data.first;
    first_free=cap=data.second;
    return *this;
}

StrVec::StrVec(const StrVec& s)
{
    pair<string*,string*> newdata=alloc_copy(s.begin(),s.end());
    elements=newdata.first;
    first_free=cap=newdata.second;
}

StrVec& StrVec::operator=(const StrVec& rhs)
{
    pair<string*,string*> newdata=alloc_copy(rhs.begin(),rhs.end());
    free();
    elements=newdata.first;
    first_free=cap=newdata.second;
    return *this;
}

StrVec::StrVec(StrVec&& s) noexcept :elements(s.elements),first_free(s.first_free),cap(s.cap)
{
    s.elements=s.first_free=s.cap=nullptr;
}

StrVec& StrVec::operator=(StrVec&& rhs) noexcept
{
    if(this!=&rhs)
    {
        elements=rhs.elements;
        first_free=rhs.first_free;
        cap=rhs.cap;
        rhs.elements=rhs.first_free=rhs.cap=nullptr;
    }
    return *this;
}

StrVec::~StrVec() {free();}

void StrVec::push_back(const string& s)
{
    check_alloc();
    alloc.construct(first_free++,s);
}

void StrVec::push_back(string&& s)
{
    check_alloc();
    alloc.construct(first_free++,std::move(s));
}

void StrVec::reserve(size_t n)
{
    if(n>capacity())
    {
        string* newdata=alloc.allocate(n);
        string* dest=newdata;
        string* elem=elements;
        for(size_t i=0;i!=size();i++)
            alloc.construct(dest++,std::move(*elem++));
        free();
        elements=newdata;
        first_free=dest;
        cap=elements+n;
    }
}

void StrVec::resize(size_t n)
{
    resize(n,"");
}

void StrVec::resize(size_t n,const string& s)
{
    string* newdata=alloc.allocate(n);
    string* dest=newdata;
    string* elem=elements;
    size_t origin_size=size();
    size_t newcapacity=n<origin_size ? n:origin_size;
    for(size_t i=0;i!=newcapacity;i++)
        alloc.construct(dest++,std::move(*elem++));
    if(n>origin_size)
    {
        for(size_t i=newcapacity;i!=origin_size;i++)
            alloc.construct(dest++,s);
    }
    free();
    
    elements=newdata;
    first_free=dest;
    cap=elements+n;
}

pair<string*,string*> StrVec::alloc_copy(const string* b,const string* e)
{
    string* data=alloc.allocate(e-b);
    return {data,uninitialized_copy(b,e,data)};
}

void StrVec::free()
{
    if(elements)
    {
        for(string* p=first_free;p!=elements;)
            alloc.destroy(--p);
        alloc.deallocate(elements,cap-elements);
    }
}



void StrVec::reallocate()
{
    size_t newcapacity=size() ? 2*size():1;
    string* newdata=alloc.allocate(newcapacity);
    string* dest=newdata;
    string* elem=elements;
    for(size_t i=0;i!=size();i++)
        alloc.construct(dest++,std::move(*elem++));
    free();
    elements=newdata;
    first_free=dest;
    cap=elements+newcapacity;
}

