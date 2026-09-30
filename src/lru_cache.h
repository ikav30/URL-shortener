#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <string>
#include <list>
#include <unordered_map>

class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    // Look up a key. Returns true (and fills `value`) on a hit, false on a miss.
    // A hit also marks the item as most-recently-used.
    bool get(const std::string& key, std::string& value) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;                  // cache miss
        items_.splice(items_.begin(), items_, it->second);   // move node to front (MRU)
        value = it->second->second;
        return true;
    }

    // Insert or update a key.
    void put(const std::string& key, const std::string& value) {
        auto it = map_.find(key);
        if (it != map_.end()) {                              // already exists: update + promote
            it->second->second = value;
            items_.splice(items_.begin(), items_, it->second);
            return;
        }
        if (items_.size() >= capacity_) {                    // full: evict the LRU (back)
            map_.erase(items_.back().first);
            items_.pop_back();
        }
        items_.emplace_front(key, value);                    // insert new at front
        map_[key] = items_.begin();
    }

    size_t size() const { return items_.size(); }

private:
    size_t capacity_;
    // front = most recently used, back = least recently used
    std::list<std::pair<std::string, std::string>> items_;
    // key -> iterator pointing at that key's node in the list
    std::unordered_map<std::string,
        std::list<std::pair<std::string, std::string>>::iterator> map_;
};

#endif
