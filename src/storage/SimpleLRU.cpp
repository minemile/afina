#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        std::size_t size = key.size() + value.size();
        CheckLRUCache(size);
        return InsertNode(key, value);
    } else {
        lru_node *node = &(it->second.get());
        node->value = value;
        return UpdateNode(node);
    }
}

bool SimpleLRU::InsertNode(const std::string &key, const std::string &value) {

    std::size_t size = key.size() + value.size();
    _cur_size += size;

    std::unique_ptr<lru_node> node(new lru_node(key, value, _lru_tail));
    if (!_lru_head) {
        _lru_head = std::move(node);
        _lru_tail = _lru_head.get();
    } else {
        _lru_tail->next = std::move(node);
        _lru_tail = _lru_tail->next.get();
    }

    _lru_index.insert(std::pair<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>>(
        _lru_tail->key, *_lru_tail));
    return true;
}

bool SimpleLRU::CheckLRUCache(const std::size_t size) {
    if (_cur_size + size > _max_size) {
        while (_lru_head && (_cur_size + size > _max_size)) {
            std::size_t node_size = _lru_head->value.size() + _lru_head->key.size();
            _cur_size -= node_size;
            _lru_index.erase(_lru_head->key);
            _lru_head = std::move(_lru_head->next);
        }
    }
    return true;
}

bool SimpleLRU::UpdateNode(lru_node *node) const {
    if (node == _lru_head.get() && _lru_head->next) {
        auto future_head = std::move(_lru_head->next);
        future_head->prev = nullptr;

        _lru_head->next = nullptr;
        _lru_head->prev = _lru_tail;

        _lru_tail->next = std::move(_lru_head);
        _lru_tail = _lru_tail->next.get();

        _lru_head = std::move(future_head);
    } else if (node->next && node->prev) {
        node->next->prev = node->prev;
        node->prev->next.swap(node->next);

        node->prev = _lru_tail;
        node->next.release();

        _lru_tail->next.reset(node);
        _lru_tail = _lru_tail->next.get();
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    } else {
        return InsertNode(key, value);
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }
    lru_node *node = &(it->second.get());
    node->value = value;
    return UpdateNode(node);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }
    _lru_index.erase(key);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) const {
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    } else {
        lru_node *node = &(it->second.get());
        value = node->value;
        return UpdateNode(node);
    }
}
} // namespace Backend
} // namespace Afina
