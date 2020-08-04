#pragma once
#include <cassert>

template<class T>
class Array
{
private:
	T* list;//ָ��T���͵�ָ��
	int size;//Ԫ�ظ���
	int capacity;//��̬�����С
public:
	Array(int capacity=50);//Ĭ�Ϲ��캯������̬����Ĭ�ϴ�С50��Ԫ��
	Array(const Array<T>& a);//���ƹ��캯��
	~Array();//��������
	Array<T>& operator = (const Array<T>& rhs);//���ظ�ֵ�����= ������ֵ�����ã���ֵ
	T& operator [] (int i) ;//�����±������[]������ֵ�����ã���ֵ�����ܽ��к������޸�
	const T& operator [] (int i) const;//�����±������[]������ֵ�ǳ��������ͣ�
	operator T* ();
	operator const T* () const;
	int getSize() const;
	int getCapacity() const;
	void resize(int capacity_);//���¶����СΪcapacity�Ķ�̬����
	void push_back(const T& item);
};

template<class T>
Array<T>::Array(int capacity_ )
{
	assert(capacity_ >= 0);//�쳣���
	capacity = capacity_;
	list = new T[capacity];//ָ��T���͵�ָ�룬��Ŷ�̬������׵�ַ
}

template<class T>
Array<T>::Array(const Array<T>& a)//�����ƹ��캯����Ҫ��㸴��ʱ����ôҲ��Ҫ����ֵ�����=����Ϊ��㸴����ʽ
{
	size = a.size;
	capacity = a.capacity;
	list = new T[capacity];//��㸴�ƣ��ȴ����µ�һ����С�Ķ�̬���飬
	for (int i = 0; i < size; i++)//Ȼ���a�����ж�̬�������ÿ��Ԫ�ظ��ƹ���
		list[i] = a.list[i];
}

template<class T>
Array<T>::~Array()
{
	delete[]list;//�ͷ�list��ָ���������ڴ�ռ�
}

template<class T>
Array<T>& Array<T>::operator = (const Array<T>& rhs)//�����ǳ����ã����ÿ�����ߴ���Ч�ʣ�����Ҫ����һ��ʵ�εĸ����������ÿ��Է�ֹԭʵ�α��޸�
{
	if (&rhs != this)//��ֵ����ʱ�������£��Ȱ������������ԭ�е��������
	{
		if (capacity != rhs.capacity)//�ٰ��Ҳ����������ݸ��ƹ���
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
Array<T>::operator T* ()//����ת�������������Ҫ��ʱ����ʽ�ؽ�������ת��Ϊָ��T���͵�ָ��
{
	return list;
}

template<class T>
Array<T>::operator const T* () const//�����ð汾
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
	T* newList = new T[capacity_];//����һ���µ��ڴ�ռ�
	size = (capacity_ < size) ? capacity_:size;//sizeȡ��С�ģ�����̬����ߴ�С��ԭ����Ԫ�ظ���ʱ����ֻȡǰ���ⲿ��Ԫ�أ����������
	for (int i = 0; i < size; i++)
		newList[i] = list[i];//��ԭ�ڴ�ռ��Ԫ�ظ��ƹ���
	delete[]list;//��ԭ�ڴ�ռ��ͷ�
	list = newList;
}

template<class T>
void Array<T>::push_back(const T& item)
{
	if (size == capacity)//����̬�����Ѿ�����ʱ��������
		resize(2 * capacity);
	list[size] = item;
	size++;
}