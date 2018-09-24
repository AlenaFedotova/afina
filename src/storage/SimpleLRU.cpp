#include "SimpleLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it != _lru_index.end()) {
		return _Update_in_list(it->second.get(), value);
	}
	return _Insert_to_list(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) { 
	if (_lru_index.find(std::reference_wrapper<const std::string>(key)) != _lru_index.end())
		return false;
	return _Insert_to_list(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) { 
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it == _lru_index.end())
		return false;
	return _Update_in_list(it->second.get(), value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it == _lru_index.end())
		return false;
	_lru_index.erase(it);
	return _Erase_from_list(it->second.get());
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) { 
	auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
	if (it == _lru_index.end())
		return false;
	value = it->second.get().value;
	return _Update_in_list(it->second.get(), value);
}

bool SimpleLRU::_DeleteOld() {
	if (_lru_head == nullptr)
		return false;
	_lru_index.erase(_lru_head->key);
	return _Erase_from_list(*_lru_head);
}

bool SimpleLRU::_Insert_to_list(const std::string &key, const std::string &value) {
	std::unique_ptr<lru_node> ptr(new lru_node(key, value, _lru_tail));
	lru_node * tmp = ptr.get();
	ptr.swap(ptr->next);
	if (!_insert_ptr(*tmp))
		return false;
	_lru_index.insert(std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>(_lru_tail->key, *_lru_tail));
	return true;
}
    
bool SimpleLRU::_Erase_from_list(lru_node &node) {
	if (!_erase_ptr(node))
		return false;
	node.next.reset();
	return true;
}

bool SimpleLRU::_Update_in_list(lru_node &node, const std::string &value) {
	if (!_erase_ptr(node))
		return false;
	if (!_insert_ptr(node))
		return false;
	_lru_tail->value = value;
	return true;
}

bool SimpleLRU::_insert_ptr(lru_node &node) {
std::size_t len = node.key.size() + node.value.size();
	if (len > _max_size) 
		return false;
	while (len + _cur_size > _max_size) {
		if (!_DeleteOld())
			return false;
	}
	_cur_size += len;
	if (_lru_tail != nullptr) {
		_lru_tail->next.swap(node.next);
		node.prev = _lru_tail;
		_lru_tail = _lru_tail->next.get();
	}
	else {
		_lru_head.swap(node.next);
		node.prev = _lru_tail;
		_lru_tail = _lru_head.get();
	}
	return true;
}
    
bool SimpleLRU::_erase_ptr(lru_node &node) {
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
	return true;
}

} // namespace Backend
} // namespace Afina
