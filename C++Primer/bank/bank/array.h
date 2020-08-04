#pragma once
#include <cassert>

template<class T>
class Array
{
private:
	T* list;//指向T类型的指针
	int size;//元素个数
	int capacity;//动态数组大小
public:
	Array(int capacity=50);//默认构造函数，动态数组默认大小50个元素
	Array(const Array<T>& a);//复制构造函数
	~Array();//析构函数
	Array<T>& operator = (const Array<T>& rhs);//重载赋值运算符= ，返回值是引用，左值
	T& operator [] (int i) ;//重载下标运算符[]，返回值是引用，左值，才能进行后续的修改
	const T& operator [] (int i) const;//重载下标运算符[]，返回值是常引用类型，
	operator T* ();
	operator const T* () const;
	int getSize() const;
	int getCapacity() const;
	void resize(int capacity_);//重新定义大小为capacity的动态数组
	void push_back(const T& item);
};

template<class T>
Array<T>::Array(int capacity_ )
{
	assert(capacity_ >= 0);//异常检测
	capacity = capacity_;
	list = new T[capacity];//指向T类型的指针，存放动态数组的首地址
}

template<class T>
Array<T>::Array(const Array<T>& a)//当复制构造函数需要深层复制时，那么也需要将赋值运算符=重载为深层复制形式
{
	size = a.size;
	capacity = a.capacity;
	list = new T[capacity];//深层复制，先创建新的一样大小的动态数组，
	for (int i = 0; i < size; i++)//然后把a对象中动态数组里的每个元素复制过来
		list[i] = a.list[i];
}

template<class T>
Array<T>::~Array()
{
	delete[]list;//释放list所指向的数组的内存空间
}

template<class T>
Array<T>& Array<T>::operator = (const Array<T>& rhs)//参数是常引用，引用可以提高传参效率，不需要复制一份实参的副本，常引用可以防止原实参被修改
{
	if (&rhs != this)//赋值运算时发生的事：先把左操作数对象原有的数据清空
	{
		if (capacity != rhs.capacity)//再把右操作数的数据复制过来
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
Array<T>::operator T* ()//类型转换运算符，在需要的时候，隐式地将对象名转换为指向T类型的指针
{
	return list;
}

template<class T>
Array<T>::operator const T* () const//常引用版本
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
	T* newList = new T[capacity_];//申请一块新的内存空间
	size = (capacity_ < size) ? capacity_:size;//size取较小的，即动态数组尺寸小于原本的元素个数时，就只取前面这部分元素，后面的舍弃
	for (int i = 0; i < size; i++)
		newList[i] = list[i];//把原内存空间的元素复制过来
	delete[]list;//把原内存空间释放
	list = newList;
}

template<class T>
void Array<T>::push_back(const T& item)
{
	if (size == capacity)//当动态数组已经满了时，先扩容
		resize(2 * capacity);
	list[size] = item;
	size++;
}