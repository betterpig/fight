#include "blob.h"

template<typename T>
class BlobPtr
{
public:
    BlobPtr():curr(0) {}
    BlobPtr(Blob<T>& A,size_t sz=0):(wptr(a.data),curr(sz)) {}
    T& operator*() const
    {
        auto p=check(curr,"deference past end");
        return (*p)[curr];
    }
    BlobPtr<T>& operator++();
    BlobPtr<T>& operator++(int);
    BlobPtr<T>& operator--();
    BlobPtr<T>& operator--(int);

private:
    shared_ptr<vector<T>> check(size_t,const string&) const;
    weak_ptr<vector<T>> wptr;
    size_t curr;
};

template<typename T>
BlobPtr<T>& BlobPtr<T>::operator++()
{

}

template<typename T>
BlobPtr<T>& BlobPtr<T>::operator++(int)
{
    BlobPtr<T> ret=*this;
    ++(*this);
    return ret;
}