#pragma once
#include<iostream>
#include<future>
#include<thread>
#include<list>


template<typename T>
class CMyBlockQueue
{

private:
	const size_t _Max_size = 128;//至少应为2 以保证暂停时的绘图缓冲
	mutable std::mutex _Mutex;
	std::condition_variable _Condvar_consumer;
	std::condition_variable _Condvar_productor;
	std::list<T> _Queue;
	
public:
	CMyBlockQueue() :_Mutex(), _Condvar_consumer(), _Condvar_productor(), _Queue(){}

	void PutItem(const T& Item)
	{
		std::unique_lock<std::mutex> lock(_Mutex);
		_Condvar_productor.wait(lock, [this] {return _Queue.size() < _Max_size;});
		_Queue.push_back(Item);
		_Condvar_consumer.notify_all();
	}

	T& TakeItem()
	{
		std::unique_lock<std::mutex> lock(_Mutex);
		_Condvar_consumer.wait(lock, [this] {return !_Queue.empty(); });
		assert(!_Queue.empty());
		T& front(_Queue.front());
		_Condvar_productor.notify_all();
		return front;
	}
	T TakeItemWithoutBlock()
	{
		std::lock_guard<std::mutex> lock(_Mutex);
		if (!_Queue.empty())
		{
			T front(_Queue.front());
			_Condvar_productor.notify_all();
			return front;
		}
		return NULL;
	}

	void RemoveFront()
	{
		std::lock_guard<std::mutex> lock(_Mutex);
		if (_Queue.empty())
			return;
		_Queue.pop_front();
	}

	void EmptyQueue()
	{
		std::unique_lock<std::mutex> lock(_Mutex);
		_Queue.clear();
		_Condvar_productor.wait(lock, [this] {return _Queue.empty(); });
		_Condvar_consumer.wait(lock, [this] {return _Queue.empty(); });
		_Condvar_productor.notify_all();
	}

	bool IsEmpty()
	{
		std::lock_guard<std::mutex> lock(_Mutex);
		return _Queue.empty();
	}

	bool IsFull()
	{
		std::lock_guard<std::mutex> lock(_Mutex);
		return _Queue.size() >= _Max_size;
	}

	size_t QueueSize()const
	{
		std::lock_guard<std::mutex> lock(_Mutex);
		return _Queue.size();
	}
};


