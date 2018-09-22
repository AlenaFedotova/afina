#include "SimpleLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
	auto it = _lru_index.find(key);
	if (it != _lru_index.end()) {
		if (!_Erase_from_list(it->second.get()))
			return false;
		if (!_Insert_to_list(key, value))
			return false;
		it->second = std::reference_wrapper<lru_node>(*_lru_tail);
		return true;
	}
	if (!_Insert_to_list(key, value))
		return false;
	_lru_index.insert(std::pair<std::string, std::reference_wrapper<lru_node>>(key, *_lru_tail));
	return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) { 
	if (_lru_index.find(key) != _lru_index.end())
		return false;
	if (!_Insert_to_list(key, value))
		return false;
	_lru_index.insert(std::pair<std::string, std::reference_wrapper<lru_node>>(key, *_lru_tail));
	return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) { 
	auto it = _lru_index.find(key);
	if (it == _lru_index.end())
		return false;
	if (!_Erase_from_list(it->second.get()))
		return false;
	if (!_Insert_to_list(key, value))
		return false;
	it->second = std::reference_wrapper<lru_node>(*_lru_tail);
	return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
	auto it = _lru_index.find(key);
	if (it == _lru_index.end())
		return false;
	_lru_index.erase(it);
	return _Erase_from_list(it->second.get());
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) { 
	auto it = _lru_index.find(key);
	if (it == _lru_index.end())
		return false;
	value = it->second.get().value;
	if (!_Erase_from_list(it->second.get()))
		return false;
	return _Insert_to_list(key, value);
}

bool SimpleLRU::_DeleteOld() {
	if (_lru_head == nullptr)
		return false;
	_lru_index.erase(_lru_head->key);
	return _Erase_from_list(*_lru_head);
}

bool SimpleLRU::_Insert_to_list(const std::string &key, const std::string &value) {
	std::size_t len = key.size() + value.size();
	if (len > _max_size) 
		return false;
	while (len + _cur_size > _max_size) {
		if (!_DeleteOld())
			return false;
	}
	_cur_size += len;
	if (_lru_tail != nullptr) {
		_lru_tail->next.reset(new lru_node(key, value, _lru_tail));
		_lru_tail = _lru_tail->next.get();
	}
	else {
		_lru_head.reset(new lru_node(key, value, _lru_tail));
		_lru_tail = _lru_head.get();
	}
	return true;
}
    
bool SimpleLRU::_Erase_from_list(lru_node &node) {
	std::size_t len = node.key.size() + node.value.size();
	_cur_size -= len;
	if (node.next != nullptr)
		node.next->prev = node.prev;
	else
		_lru_tail = node.prev;
	if (node.prev != nullptr)
		node.prev->next.swap(node.next);
	else
		_lru_head.swap(node.next);
	node.next.reset();
	return true;
}

} // namespace Backend
} // namespace Afina
