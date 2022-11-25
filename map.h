#pragma once

#include <cassert>
#include <iostream>

namespace bimap_details {

struct map_base_node {
  map_base_node *left{nullptr}, *right{nullptr}, *parent{nullptr};
};

template <typename T, typename Tag>
struct map_node : map_base_node {
  explicit map_node(const T& value) : value(value) {}
  explicit map_node(T&& value) : value(std::move(value)) {}

  T value;
};

template <typename T, typename Tag>
struct map_head_node : map_base_node {
  using node_ptr = map_node<T, Tag>*;

  map_head_node(node_ptr ptr) {
    left = ptr;
  }

  node_ptr get_root() const {
    return static_cast<node_ptr>(left);
  }
};

struct Left_Tag;
struct Right_Tag;

template <typename T, typename U>
struct node : map_node<T, Left_Tag>, map_node<U, Right_Tag> {
  using left_base = map_node<T, struct Left_Tag>;
  using right_base = map_node<U, struct Right_Tag>;

  template <typename T_, typename U_>
  node(T_&& first, U_&& second)
      : left_base(std::forward<T_>(first)),
        right_base(std::forward<U_>(second)) {}
};

template <typename Key, typename Comparator, typename Tag>
struct map {
  using key_t = Key;
  using node_t = map_node<Key, Tag>;
  using head_node_t = map_head_node<Key, Tag>;

  map() : head(nullptr) {}

  map(Comparator cmp) : head(nullptr), cmp(std::move(cmp)) {}

  map(const map& other) : head(nullptr), cmp(other.cmp) {}

  map(map&& other) : head(other.head), cmp(std::move(other.cmp)) {
    other.head.left = nullptr;
  }

  map& operator=(const map& other) noexcept {
    cmp = other.cmp;
    return *this;
  }

  map& operator=(map&& other) noexcept {
    head.left = other.head.left;
    cmp = std::move(other.cmp);
    other.head.left = nullptr;
    return *this;
  }

  void swap(map& other) noexcept {
    std::swap(cmp, other.cmp);
    auto root = head.left;
    head.left = other.head.left;
    if (head.left)
      head.left->parent = &head;
    other.head.left = root;
    if (other.head.left)
      other.head.left->parent = &other.head;
  }

  struct iterator {
    friend struct map<Key, Comparator, Tag>;
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = Key;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type const*;
    using reference = value_type const&;

    explicit iterator(map_base_node* ptr) : m_ptr(ptr){};
    explicit iterator(const map_base_node* ptr) : m_ptr(const_cast<map_base_node*>(ptr)){};

    reference operator*() const {
      return static_cast<node_t*>(m_ptr)->value;
    }

    pointer operator->() const {
      return &operator*();
    }

    iterator& operator++() {
      if (m_ptr->right) {
        m_ptr = m_ptr->right;
        while (m_ptr->left) {
          m_ptr = m_ptr->left;
        }
      } else {
        auto prev = m_ptr;
        m_ptr = m_ptr->parent;
        while (m_ptr && m_ptr->right == prev) {
          prev = m_ptr;
          m_ptr = m_ptr->parent;
        }
        if (!m_ptr) {
          m_ptr = prev;
        }
      }
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    iterator& operator--() {
      if (m_ptr->left) {
        m_ptr = m_ptr->left;
        while (m_ptr->right) {
          m_ptr = m_ptr->right;
        }
      } else {
        auto prev = m_ptr;
        m_ptr = m_ptr->parent;
        while (m_ptr && m_ptr->left == prev) {
          prev = m_ptr;
          m_ptr = m_ptr->parent;
        }
        if (!m_ptr) {
          m_ptr = prev;
        }
      }
      return *this;
    }

    iterator operator--(int) {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }

    node_t* get_node() const {
      return static_cast<node_t*>(m_ptr);
    }

    friend bool operator==(const iterator& a, const iterator& b) {
      return a.m_ptr == b.m_ptr;
    };
    friend bool operator!=(const iterator& a, const iterator& b) {
      return a.m_ptr != b.m_ptr;
    };

    map_base_node* m_ptr;
  };

  iterator begin() const {
    if (!head.left) {
      return iterator(&head);
    }
    auto res = head.left;
    while (res->left) {
      res = res->left;
    }
    return iterator(res);
  }

  iterator end() const {
    return iterator(&head);
  }

  iterator insert(node_t* node) {
    if (!head.left) {
      head.left = node;
      node->parent = &head;
      return iterator(node);
    }
    node_t* prev = nullptr;
    node_t* now = static_cast<node_t*>(head.left);
    bool left = false;
    while (now) {
      prev = now;
      if (cmp(node->value, now->value)) {
        left = true;
        now = static_cast<node_t*>(now->left);
      } else {
        left = false;
        now = static_cast<node_t*>(now->right);
      }
    }
    if (left) {
      prev->left = node;
    } else {
      prev->right = node;
    }
    node->parent = prev;
    return iterator(node);
  }

  iterator find(const key_t& key) const {
    auto now = head.get_root();
    while (now) {
      cmp(now->value, now->value);
      if (cmp(key, now->value)) {
        now = static_cast<node_t*>(now->left);
      } else if (cmp(now->value, key)) {
        now = static_cast<node_t*>(now->right);
      } else {
        return iterator(now);
      }
    }
    return iterator(&head);
  }

  void link_parent(map_base_node* old, map_base_node* new_) {
    if (old->parent->left == old) {
      old->parent->left = new_;
    } else {
      old->parent->right = new_;
    }
    if (new_) {
      new_->parent = old->parent;
    }
  }

  node_t* erase(iterator it) {
    auto old = it.get_node();
    if (old->left && old->right) {
      auto new_ = erase(++it);
      new_->left = old->left;
      new_->right = old->right;
      if (old->parent) {
        old->left->parent = new_;
      }
      if (old->right) {
        old->right->parent = new_;
      }
      link_parent(old, new_);
    } else if (old->left) {
      link_parent(old, old->left);
    } else if (old->right) {
      link_parent(old, old->right);
    } else {
      link_parent(old, nullptr);
    }
    old->left = old->right = old->parent = nullptr;
    return old;
  }

  iterator lower_bound(const Key& key) const {
    auto now = head.get_root();
    node_t* prev_gt = nullptr;
    while (now) {
      if (cmp(key, now->value)) {
        prev_gt = now;
        now = static_cast<node_t*>(now->left);
      } else if (cmp(now->value, key)) {

        now = static_cast<node_t*>(now->right);
      } else {
        return iterator(now);
      }
    }

    return (prev_gt ? iterator(prev_gt) : end());
  }

  iterator upper_bound(const Key& key) const {
    iterator lower = lower_bound(key);
    if (lower == end()) {
      return end();
    }
    return cmp(key, *lower) ? lower : ++lower;
  }

  template <typename U, typename TagU>
  void set_flip_head(map_head_node<U, TagU>* node) {
    head.right = node;
  }

  head_node_t head;
  [[no_unique_address]] Comparator cmp;
};
} // namespace bimap_details