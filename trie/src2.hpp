/*
* Acknowledgement : Yuxuan Wang for Modifying the prototype of TrieStore class 
*/

#ifndef SJTU_TRIE_HPP
#define SJTU_TRIE_HPP

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string_view>
#include <iostream>
#include <shared_mutex>
#include <string_view>
#include <mutex>  
#include <future> 

namespace sjtu {

// A TrieNode is a node in a Trie.
class TrieNode {
 public:
  // Create a TrieNode with no children.
  TrieNode() = default;

  // Create a TrieNode with some children.
  explicit TrieNode(std::map<char, std::shared_ptr<const TrieNode>> children) : children_(std::move(children)) {}

  virtual ~TrieNode() = default;

  // Clone returns a copy of this TrieNode. If the TrieNode has a value, the value is copied. The return
  // type of this function is a unique_ptr to a TrieNode.
  //
  // You cannot use the copy constructor to clone the node because it doesn't know whether a `TrieNode`
  // contains a value or not.
  //
  // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use `std::shared_ptr<T>(std::move(ptr))`.
  // virtual auto Clone() const -> std::unique_ptr<TrieNode> { return std::make_unique<TrieNode>(children_); }
  virtual auto Clone() const -> std::shared_ptr<TrieNode> { return std::make_shared<TrieNode>(children_); }
  // A map of children, where the key is the next character in the key, and the value is the next TrieNode.
  std::map<char, std::shared_ptr<const TrieNode>> children_;

  // Indicates if the node is the terminal node.
  bool is_value_node_{false};

  // You can add additional fields and methods here. But in general, you don't need to add extra fields to
  // complete this project.
};

// A TrieNodeWithValue is a TrieNode that also has a value of type T associated with it.
template <class T>
class TrieNodeWithValue : public TrieNode {
 public:
  // Create a trie node with no children and a value.
  explicit TrieNodeWithValue(std::shared_ptr<T> value) : value_(std::move(value)) { this->is_value_node_ = true; }

  // Create a trie node with children and a value.
  TrieNodeWithValue(std::map<char, std::shared_ptr<const TrieNode>> children, std::shared_ptr<T> value)
      : TrieNode(std::move(children)), value_(std::move(value)) {
    this->is_value_node_ = true;
  }

  // Override the Clone method to also clone the value.
  //
  // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use `std::shared_ptr<T>(std::move(ptr))`.
  
  // auto Clone() const -> std::unique_ptr<TrieNode> override {
  //   return std::make_unique<TrieNodeWithValue<T>>(children_, value_);
  // }
  auto Clone() const -> std::shared_ptr<TrieNode> override {
    return std::make_shared<TrieNodeWithValue<T>>(children_, value_);
  }

  // The value associated with this trie node.
  std::shared_ptr<T> value_;
};

// A Trie is a data structure that maps strings to values of type T. All operations on a Trie should not
// modify the trie itself. It should reuse the existing nodes as much as possible, and create new nodes to
// represent the new trie.
class Trie {
 private:
  // The root of the trie.
  std::shared_ptr<const TrieNode> root_{nullptr};

  // Create a new trie with the given root.
  explicit Trie(std::shared_ptr<const TrieNode> root) : root_(std::move(root)) {}

 public:
  // Create an empty trie.
  Trie() = default;

  // Get the value associated with the given key.
  // 1. If the key is not in the trie, return nullptr.
  // 2. If the key is in the trie but the type is mismatched, return nullptr.
  // 3. Otherwise, return the value.
  template <class T>
  auto Get(std::string_view key) const -> const T * {
    std::shared_ptr<const TrieNode> cur = root_;
    for(char ch : key) {
        if(!cur || cur->children_.find(ch)==cur->children_.end())
          return nullptr;
        cur = cur->children_.at(ch);
    }
    if(cur && cur->is_value_node_) {
      auto node = std::dynamic_pointer_cast<const TrieNodeWithValue<T>>(cur);
      if(node) return node->value_.get(); 
    }
    return nullptr;
  };

  // Put a new key-value pair into the trie. If the key already exists, overwrite the value.
  // Returns the new trie.
  template <class T>
  auto Put(std::string_view key, T value) const -> Trie {
    auto newroot = root_ ? root_->Clone() : std::make_shared<TrieNode>();
    std::shared_ptr<TrieNode> cur = newroot, pre = nullptr;
    char pch = 0;
    for(char ch : key) {
      pre = cur;
      pch = ch;
      std::shared_ptr<TrieNode> newNode;
      if(cur->children_.find(ch) == cur->children_.end())
        newNode = std::make_shared<TrieNode>();
      else
        newNode = cur->children_.at(ch)->Clone();
      cur->children_[ch] = newNode;
      cur = newNode;
    }
    if(!cur->is_value_node_) {
      auto valuePtr = std::make_shared<T>(std::move(value));
      auto newNode = std::make_shared<TrieNodeWithValue<T>>(cur->children_, std::move(valuePtr));
      pre->children_[pch] = std::move(newNode);
    }
    else {
      auto node = std::dynamic_pointer_cast<TrieNodeWithValue<T>>(cur);
      node->value_ = std::make_shared<T>(std::move(value));
    }
    return Trie(newroot);
  };

  // Remove the key from the trie. If the key does not exist, return the original trie.
  // Otherwise, returns the new trie.
  auto Remove(std::string_view key) const -> Trie {
    if(!root_) return *this;

    std::vector<std::pair<char, std::shared_ptr<TrieNode>>> path;
    auto newRoot = root_->Clone();
    std::shared_ptr<TrieNode> cur = newRoot;
    bool exists = true;

    for(char ch : key) {
      if (cur->children_.find(ch) == cur->children_.end()) {
          exists = false;
          break;
      }
      path.emplace_back(ch, cur);
      cur = cur->children_.at(ch)->Clone();
    }

    if(!exists || !cur->is_value_node_) return *this;

    if (!cur->children_.empty()) {
      auto newNode = std::make_shared<TrieNode>(std::move(cur->children_));
      cur = newNode;
    }
    else {
      for (auto it = path.rbegin(); it != path.rend(); ++it) {
          it->second->children_.erase(it->first); 
          if (!it->second->children_.empty() || it->second->is_value_node_) break;
      }
    }

    path.emplace_back('\0', cur);
    for(auto it=path.begin(); (it+1)!=path.end();it++){
      if(it->second->children_.find(it->first)==it->second->children_.end()) break;
      it->second->children_[it->first] = (it+1)->second; 
    }

    return Trie(newRoot);
  };
};


// This class is used to guard the value returned by the trie. It holds a reference to the root so
// that the reference to the value will not be invalidated.
template <class T>
class ValueGuard {
 public:
  ValueGuard(Trie root, const T &value) : root_(std::move(root)), value_(value) {}
  auto operator*() const -> const T & { return value_; }

 private:
  Trie root_;
  const T &value_;
};

// This class is a thread-safe wrapper around the Trie class. It provides a simple interface for
// accessing the trie. It should allow concurrent reads and a single write operation at the same
// time.
class TrieStore {
 public:
  // This function returns a ValueGuard object that holds a reference to the value in the trie of the given version (default: newest version). If
  // the key does not exist in the trie, it will return std::nullopt.
  template <class T>
  auto Get(std::string_view key, size_t version = -1) -> std::optional<ValueGuard<T>> {
    if(version >= snapshots_.size() || version == -1) return std::nullopt;
    const auto& trie = snapshots_[version];
    const T* res = trie.Get<T>(key);
    if(res) return ValueGuard<T>(trie, *res);
    else return std::nullopt;
  };

  // This function will insert the key-value pair into the trie. If the key already exists in the
  // trie, it will overwrite the value
  // return the version number after operation
  // Hint: new version should only be visible after the operation is committed(completed)
  template <class T>
  size_t Put(std::string_view key, T value) {
    std::lock_guard<std::mutex> lock(write_lock_);
    Trie newtrie = snapshots_.back().Put<T>(key, std::move(value));
    snapshots_.push_back(std::move(newtrie));
    return snapshots_.size()-1;
  };

  // This function will remove the key-value pair from the trie.
  // return the version number after operation
  // if the key does not exist, version number should not be increased
  size_t Remove(std::string_view key) {
    std::lock_guard<std::mutex> wlock(write_lock_);
    Trie newtrie = snapshots_.back().Remove(key);
    snapshots_.push_back(std::move(newtrie));
    return snapshots_.size()-1;
  };

  // This function return the newest version number
  size_t get_version() {
    return snapshots_.size()-1;
  };

 private:

  // This mutex sequences all writes operations and allows only one write operation at a time.
  // Concurrent modifications should have the effect of applying them in some sequential order
  std::mutex write_lock_;

  // Stores all historical versions of trie
  // version number ranges from [0, snapshots_.size())
  std::vector<Trie> snapshots_{1};
};

}  // namespace sjtu

#endif  // SJTU_TRIE_HPP