#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) { 
	if (_lru_index.find(key) != _lru_index.end())
		if (!Delete(key))
			return false;
	return PutIfAbsent(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) { 
	if (_lru_index.find(key) != _lru_index.end())
		return false;
	std::size_t len = key.size() + value.size();
	if (len > _max_size)
		return false;
	while (len + _cur_size > _max_size) {
		if (!_DeleteOld())
			return false;
	}
	_cur_size += len;
	_lru_tail = std::make_shared<lru_node>(key, value, _lru_tail, nullptr);
	if (_lru_tail->prev != nullptr)
		_lru_tail->prev->next = _lru_tail;
	if (_lru_head == nullptr)
		_lru_head = _lru_tail;
	_lru_index.insert(std::pair<std::string, std::reference_wrapper<lru_node>>(key, *_lru_tail));
	return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) { 
	if (_lru_index.find(key) == _lru_index.end())
		return false;
	if (!Delete(key))
		return false;
	return PutIfAbsent(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) { 
	if (_lru_head == nullptr)
		return false;
	auto ind = _lru_index.find(key);
	if (ind == _lru_index.end())
		return false;
	auto value = ind->second.get().value;
	std::size_t len = key.size() + value.size();
	_cur_size -= len;
	auto next = ind->second.get().next;
	auto prev = ind->second.get().prev;
	_lru_index.erase(key);
	if (next != nullptr)
		next->prev = prev;
	else
		_lru_tail = prev;
	if (prev != nullptr)
		prev->next = next;
	else
		_lru_head = next;
	return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) const { 
	auto ind = _lru_index.find(key);
	if (ind == _lru_index.end())
		return false;
	value = ind->second.get().value;
	return true;
}

bool SimpleLRU::_DeleteOld() {
	if (_lru_head == nullptr)
		return false;
	return Delete(_lru_head->key);
}

} // namespace Backend
} // namespace Afina
