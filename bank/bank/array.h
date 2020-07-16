#pragma once
#include <cassert>

template<class T>
class Array
{
private:
	T* list;
	int size;
	int capacity;
public:
	Array(int capacity=50);
	Array(const Array<T>& a);
	~Array();
	Array<T>& operator = (const Array<T>& rhs);
	T& operator [] (int i) ;
	const T& operator [] (int i) const;
	operator T* ();
	operator const T* () const;
	int getSize() const;
	int getCapacity() const;
	void resize(int capacity_);
	void push_back(const T& item);
};

template<class T>
Array<T>::Array(int capacity_ )
{
	assert(capacity_ >= 0);
	capacity = capacity_;
	list = new T[capacity];
}

template<class T>
Array<T>::Array(const Array<T>& a)
{
	size = a.size;
	capacity = a.capacity;
	list = new T[capacity];
	for (int i = 0; i < size; i++)
		list[i] = a.list[i];
}

template<class T>
Array<T>::~Array()
{
	delete[]list;
}

template<class T>
Array<T>& Array<T>::operator = (const Array<T>& rhs)
{
	if (&rhs != this)
	{
		if (capacity != rhs.capacity)
		{
			delete[]list;
			capacity = rhs.capacity;
			list = new T[capacity];
		}
		size = rhs.size;
		for (int i = 0; i < size; i++)
			list[i] = rhs.list[i];
	}
	return *this;
}

template<class T>
T& Array<T>::operator[] (int n)
{
	assert(n >= 0 && n < size);
	return list[n];
}

template<class T>
const T& Array<T>::operator[] (int n) const
{
	assert(n >= 0 && n < size);
	return list[n];
}

template<class T>
Array<T>::operator T* ()
{
	return list;
}

template<class T>
Array<T>::operator const T* () const
{
	return list;
}


template<class T>
int Array<T>::getSize() const
{
	return size;
}

template<class T>
int Array<T>::getCapacity() const
{
	return capacity;
}

template<class T>
void Array<T>::resize(int capacity_)
{
	assert(capacity_ >= 0);
	if (capacity_ == capacity)
		return;
	T* newList = new T[capacity_];
	size = (capacity_ < size)?capacity_:size;
	for (int i = 0; i < size; i++)
		newList[i] = list[i];
	delete[]list;
	list = newList;
}

template<class T>
void Array<T>::push_back(const T& item)
{
	if (size == capacity)
		resize(2 * capacity);
	list[size] = item;
	size++;
}