/*----------------------------------------------------------------
// Copyright 2021
// All rights reserved.
//
// author: 404558965@qq.com (xiaoquanjie)
// github: https://github.com/xiaoquanjie/cgo
// Created by xiaoqj on 2023/11/03
//----------------------------------------------------------------*/

#pragma once

#include <cstdlib>
#include <cassert>
#include <stdexcept>

template<typename T>
class squeue {
protected:
	size_t _cap;
	size_t _size;
	T*  _head;
public:
    squeue() {
		_cap = 100;
		_size = 0;
		_head = (T*)malloc(sizeof(T)*_cap);
	}

    squeue(size_t cap) {
		_cap = cap;
		_size = 0;
		_head = (T*)malloc(sizeof(T)*_cap);
	}

	~squeue() {
		clear();
		free(_head);
	}

    [[nodiscard]]
	bool empty()const {
		return (_size == 0);
	}

    [[nodiscard]]
	size_t size()const {
		return _size;
	}

	void swap(squeue<T>& other) {
		size_t tmp = this->_cap;
		this->_cap = other._cap;
		other._cap = tmp;
		tmp = this->_size;
		this->_size = other._size;
		other._size = tmp;
		T* tmp2 = this->_head;
		this->_head = other._head;
		other._head = tmp2;
	}

    [[nodiscard]]
	T& back() {
		return _head[_size-1];
	}

	void pop() {
		if (_size > 0) {
			_size--;
			T* p = &(this->_head[_size]);
			p->~T();
		}
	}

	void push(const T& t) {
		if (_size >= _cap) {
			this->_cap = this->_cap * 2;
            auto head = (T*)realloc(this->_head, sizeof(T)*this->_cap);
            if (!head) {
                assert(false);
                throw std::bad_alloc();
            }
			this->_head = head;
			return;
		}
		T* p = &(_head[_size++]);
		new(p)T(t);
	}

	void clear() {
		for (size_t i = 0; i < _size; ++i) {
			T* p = &(this->_head[i]);
			p->~T();
		}
		_size = 0;
	}

    [[nodiscard]]
	T& operator[](int i) {
		return _head[i];
	}

protected:
    squeue(const squeue&) = delete;
    squeue& operator=(const squeue&) = delete;
};

