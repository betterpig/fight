#pragma once

namespace 
{	//namespaceʹ����Ķ���ֻ�ڵ�ǰ�ļ�����Ч
	//�洢ƽ����ĳ����1��֮ǰ�ж����죬Ϊ����getMaxDay������ʵ�֣���������һ��
	const int DAYS_BEFORE_MONTH[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
}

class Date
{
private:
	int year;
	int month;
	int day;
	int totalDays;

public:
	Date(int year, int month, int day);
	int getYear() const { return year; }
	int getMonth() const { return month; }
	int getDay() const { return day; }

	int getMaxDay() const;
	bool isLeapYear() const //�жϵ����Ƿ�Ϊ����
	{	
		return year % 4 == 0 && year % 100 != 0 || year % 400 == 0;
	}
	int operator - (const Date &lastDate) const
	{
		return totalDays - lastDate.totalDays;
	}
	void show() const;
};

