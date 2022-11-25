#pragma once

#include "map.h"
#include <cstddef>
#include <iostream>
#include <stdexcept>

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap {
  using left_t = Left;
  using right_t = Right;
  using node_t = bimap_details::node<left_t, right_t>;
  using left_node = bimap_details::map_node<left_t, bimap_details::Left_Tag>;
  using right_node = bimap_details::map_node<right_t, bimap_details::Right_Tag>;

  template <typename T, typename ComparatorT, typename TagT, typename U,
            typename ComparatorU, typename TagU>
  struct iterator {
    friend struct bimap<Left, Right, CompareLeft, CompareRight>;

    using tree_it = typename bimap_details::map<T, ComparatorT, TagT>::iterator;

    using map_it = typename bimap_details::map<T, ComparatorT, TagT>::iterator;
    using flip_it = iterator<U, ComparatorU, TagU, T, ComparatorT, TagT>;

    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type const*;
    using reference = value_type const&;

    explicit iterator(tree_it it) : it(it) {}

    reference operator*() const {
      return *it;
    }
    pointer operator->() const {
      return &operator*();
    }

    iterator& operator++() {
      ++it;
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    iterator& operator--() {
      --it;
      return *this;
    }

    iterator operator--(int) {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }

    map_it to_map_it() const {
      if (!it.m_ptr->parent) {
        return typename bimap_details::map<T, ComparatorT, TagT>::iterator(
            static_cast<bimap_details::map_head_node<T, TagT>*>(
                it.m_ptr->right));
      }
      return typename bimap_details::map<T, ComparatorT, TagT>::iterator(
          static_cast<bimap_details::map_node<T, TagT>*>(
              static_cast<node_t*>(it.get_node())));
    }

    flip_it flip() const {
      if (!it.m_ptr->parent) {
        return flip_it(
            typename bimap_details::map<U, ComparatorU, TagU>::iterator(
                static_cast<bimap_details::map_head_node<U, TagU>*>(
                    it.m_ptr->right)));
      }
      return flip_it(
          typename bimap_details::map<U, ComparatorU, TagU>::iterator(
              static_cast<bimap_details::map_node<U, TagU>*>(
                  static_cast<node_t*>(it.get_node()))));
    }

    friend bool operator==(const iterator& a, const iterator& b) {
      return a.it == b.it;
    };
    friend bool operator!=(const iterator& a, const iterator& b) {
      return a.it != b.it;
    };

  private:
    tree_it it;
  };

  using left_iterator =
      iterator<left_t, CompareLeft, bimap_details::Left_Tag, right_t,
               CompareRight, bimap_details::Right_Tag>;
  using right_iterator =
      iterator<right_t, CompareRight, bimap_details::Right_Tag, left_t,
               CompareLeft, bimap_details::Left_Tag>;

  // Создает bimap не содержащий ни одной пары.
  bimap(CompareLeft compare_left = CompareLeft(),
        CompareRight compare_right = CompareRight())
      : left_map(std::move(compare_left)), right_map(std::move(compare_right)) {
    link_heads();
  }

  // Конструкторы от других и присваивания

  bimap(bimap const& other)
      : left_map(other.left_map), right_map(other.right_map) {
    for (auto it = other.begin_left(); it != other.end_left(); ++it) {
      try {
        insert(*it, *it.flip());
      } catch (...) {
        clear();
        break;
      }
    }
    link_heads();
  }

  bimap(bimap&& other) noexcept {
    other.swap(*this);
  }

  bimap& operator=(bimap const& other) {
    if (this == &other || other.empty()) {
      return *this;
    }
    bimap tmp(other);
    try {
      this->swap(tmp);
    } catch (...) {
      //do nothing
    }
    return *this;
  }

  bimap& operator=(bimap&& other) noexcept {
    if (this == &other || other.empty()) {
      return *this;
    }
    clear();
    _size = other.size();
    left_map = std::move(other.left_map);
    right_map = std::move(other.right_map);
    return *this;
  }

  // Деструктор. Вызывается при удалении объектов bimap.
  // Инвалидирует все итераторы ссылающиеся на элементы этого bimap
  // (включая итераторы ссылающиеся на элементы следующие за последними).
  ~bimap() {
    clear();
  }

  void swap(bimap& other) noexcept {
    left_map.swap(other.left_map);
    right_map.swap(other.right_map);
    std::swap(_size, other._size);
  }

  // Вставка пары (left, right), возвращает итератор на left.
  // Если такой left или такой right уже присутствуют в bimap, вставка не
  // производится и возвращается end_left().
  //  left_iterator insert(left_t const &left, right_t const &right);
  //  left_iterator insert(left_t const &left, right_t &&right);
  //  left_iterator insert(left_t &&left, right_t const &right);

  template <typename T = left_t, typename U = right_t>
  left_iterator insert(T&& left, U&& right) {
    if (!empty() && (find_left(std::forward<T>(left)) != end_left() ||
                     find_right(std::forward<U>(right)) != end_right())) {
      return end_left();
    }
    node_t* ptr(new node_t(std::forward<T>(left), std::forward<U>(right)));
    auto ans = left_map.insert(static_cast<left_node*>(ptr));
    right_map.insert(static_cast<right_node*>(ptr));
    _size++;
    return left_iterator(ans);
  }

  // Удаляет элемент и соответствующий ему парный.
  // erase невалидного итератора неопределен.
  // erase(end_left()) и erase(end_right()) неопределены.
  // Пусть it ссылается на некоторый элемент e.
  // erase инвалидирует все итераторы ссылающиеся на e и на элемент парный к e.
  left_iterator erase_left(left_iterator it) {
    auto current = it;
    auto current_flip = it.flip();
    it++;
    left_map.erase(current.to_map_it());
    current_flip.to_map_it();
    auto to_delete =
        static_cast<node_t*>(right_map.erase(current_flip.to_map_it()));
    delete to_delete;
    _size--;
    return it;
  }
  // Аналогично erase, но по ключу, удаляет элемент если он присутствует, иначе
  // не делает ничего Возвращает была ли пара удалена
  bool erase_left(left_t const& left) {
    auto it = find_left(left);
    if (it == end_left()) {
      return false;
    }
    erase_left(it);
    return true;
  }

  right_iterator erase_right(right_iterator it) {
    auto current = it;
    auto copy_current = it;
    it++;
    left_map.erase(current.flip().to_map_it());
    delete static_cast<node_t*>(right_map.erase(copy_current.to_map_it()));
    _size--;
    return it;
  }

  bool erase_right(right_t const& right) {
    auto it = find_right(right);
    if (it == end_right()) {
      return false;
    }
    erase_right(it);
    return true;
  }

  // erase от ренжа, удаляет [first, last), возвращает итератор на последний
  // элемент за удаленной последовательностью
  left_iterator erase_left(left_iterator first, left_iterator last) {
    while (first != last) {
      first = erase_left(first);
    }
    return last;
  }
  right_iterator erase_right(right_iterator first, right_iterator last) {
    while (first != last) {
      first = erase_right(first);
    }
    return last;
  }

  // Возвращает итератор по элементу. Если не найден - соответствующий end()
  left_iterator find_left(left_t const& left) const {
    return left_iterator(left_map.find(left));
  }

  right_iterator find_right(right_t const& right) const {
    return right_iterator(right_map.find(right));
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует -- бросает std::out_of_range
  right_t const& at_left(left_t const& key) const {
    left_iterator ans = find_left(key);
    if (ans == end_left()) {
      throw std::out_of_range("No such element in bimap");
    }
    return *ans.flip();
  }

  left_t const& at_right(right_t const& key) const {
    right_iterator ans = find_right(key);
    if (ans == end_right()) {
      throw std::out_of_range("No such element in bimap");
    }
    return *ans.flip();
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует, добавляет его в bimap и на противоположную
  // сторону кладет дефолтный элемент, ссылку на который и возвращает
  // Если дефолтный элемент уже лежит в противоположной паре - должен поменять
  // соответствующий ему элемент на запрашиваемый (смотри тесты)
  template <typename T = right_t,
            typename = std::enable_if_t<std::is_default_constructible_v<T>>>
  right_t const& at_left_or_default(left_t const& key) {
    if (auto it = find_left(key); it != end_left()) {
      return *it.flip();
    }
    right_t dflt{};
    erase_right(dflt);
    return *(insert(key, std::move(dflt))).flip();
  }

  template <typename T = right_t,
            typename = std::enable_if_t<std::is_default_constructible_v<T>>>
  left_t const& at_right_or_default(right_t const& key) {
    if (auto it = find_right(key); it != end_right()) {
      return *it.flip();
    }
    left_t dflt{};
    erase_left(dflt);
    return *(insert(std::move(dflt), key));
  }

  // lower и upper bound'ы по каждой стороне
  // Возвращают итераторы на соответствующие элементы
  // Смотри std::lower_bound, std::upper_bound.
  left_iterator lower_bound_left(const left_t& left) const {
    return left_iterator(left_map.lower_bound(left));
  }

  left_iterator upper_bound_left(const left_t& left) const {
    return left_iterator(left_map.upper_bound(left));
  }

  right_iterator lower_bound_right(const right_t& left) const {
    return right_iterator(right_map.lower_bound(left));
  }

  right_iterator upper_bound_right(const right_t& left) const {
    return right_iterator(right_map.upper_bound(left));
  }

  // Возващает итератор на минимальный по порядку left.
  left_iterator begin_left() const {
    return left_iterator(left_map.begin());
  }

  // Возващает итератор на следующий за последним по порядку left.
  left_iterator end_left() const {
    return left_iterator(left_map.end());
  }

  // Возващает итератор на минимальный по порядку right.
  right_iterator begin_right() const {
    return right_iterator(right_map.begin());
  }

  // Возващает итератор на следующий за последним по порядку right.
  right_iterator end_right() const {
    return right_iterator(right_map.end());
  }

  // Проверка на пустоту
  bool empty() const {
    return _size == 0;
  }

  // Возвращает размер бимапы (кол-во пар)
  std::size_t size() const {
    return _size;
  }

  // операторы сравнения
  friend bool operator==(bimap const& a, bimap const& b) {
    if (a.size() != b.size()) {
      return false;
    }
    auto first = a.begin_left();
    auto second = b.begin_left();
    while (first != a.end_left() && second != b.end_left()) {
      if (a.equal_left(*first, *second) &&
          a.equal_right(*(first.flip()), *(second.flip()))) {
        first++;
        second++;
      } else {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(bimap const& a, bimap const& b) {
    return !operator==(a, b);
  }

private:
  void clear() {
    erase_left(begin_left(), end_left());
  }

  void link_heads() {
    left_map.set_flip_head(&right_map.head);
    right_map.set_flip_head(&left_map.head);
  }

  bool equal_left(const left_t& first, const left_t& second) const {
    return !left_map.cmp(first, second) && !left_map.cmp(second, first);
  }

  bool equal_right(const right_t& first, const right_t& second) const {
    return !right_map.cmp(first, second) && !right_map.cmp(second, first);
  }

private:
  std::size_t _size = 0;
  bimap_details::map<left_t, CompareLeft, bimap_details::Left_Tag> left_map;
  bimap_details::map<right_t, CompareRight, bimap_details::Right_Tag> right_map;
};
