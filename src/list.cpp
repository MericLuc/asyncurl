/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <asyncurl/list.hpp>
#include <curl/curl.h>

#include <new>

namespace asyncurl
{
//---------------------------------------------------------------------------------------------------------------------
// LIST::ITERATOR FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------

constexpr list::iterator::iterator(curl_slist* elm) noexcept
  : cur__{ elm }
{}

list::iterator::reference
list::iterator::operator*() const noexcept
{
    return cur__->data;
}

list::iterator::pointer
list::iterator::operator->() const noexcept
{
    return &cur__->data;
}

list::iterator&
list::iterator::operator++() noexcept
{
    if (nullptr != cur__) cur__ = cur__->next;
    return *this;
}

list::iterator
list::iterator::operator++(int) noexcept
{
    list::iterator tmp{ *this };
    ++(*this);
    return tmp;
}

bool
operator==(const list::iterator& lhs, const list::iterator& rhs)
{
    return (lhs.cur__ == rhs.cur__);
}

bool
operator!=(const list::iterator& lhs, const list::iterator& rhs)
{
    return (lhs.cur__ != rhs.cur__);
}

//---------------------------------------------------------------------------------------------------------------------
// LIST FUNCTIONS
//---------------------------------------------------------------------------------------------------------------------

static std::pair<curl_slist*, curl_slist*>
_cpy(const curl_slist* n)
{
    curl_slist* head{ nullptr };
    curl_slist* tail{ nullptr };
    while (n)
    {
        if (tail = curl_slist_append(tail, n->data); !tail)
        {
            curl_slist_free_all(head);
            throw std::bad_alloc();
        }
        if (!head) head = tail;
        n = n->next;
    }
    return { head, tail };
}

/**
 * @brief list Construct the list from an existing curl_slist
 * @param n the existing list
 *
 * @warning The created object OWNS the data in curl_slist
 * In particular, you MUST *NOT* call curl_slist_free_all on it
 */
list::list(curl_slist* n, owns_data) noexcept
  : head__{ n }
{
    while ((nullptr != n) && (nullptr != n->next))
        n = n->next;
    tail__ = n;
}

/**
 * @brief list Construct the list from an existing curl_slist
 * @param n the existing list
 *
 * @warning The created object get its own copy of the data in the list.
 * Therefore, you MUST release it using curl_slist_free_all
 */
list::list(const curl_slist* n, copy_data)
{
    auto l{ _cpy(n) };
    head__ = l.first;
    tail__ = l.second;
}

list::list(const list& o)
{
    auto l = _cpy(o.head__);
    head__ = l.first;
    tail__ = l.second;
}

list&
list::operator=(const list& o)
{
    auto l{ _cpy(o.head__) };
    curl_slist_free_all(head__);
    head__ = l.first;
    tail__ = l.second;
    return *this;
}

list::list(list&& o) noexcept
  : head__{ o.head__ }
  , tail__{ o.tail__ }
{
    o.head__ = nullptr;
    o.tail__ = nullptr;
}

list&
list::operator=(list&& o) noexcept
{
    head__ = std::exchange(o.head__, head__);
    tail__ = std::exchange(o.tail__, tail__);
    return *this;
}

list::~list() noexcept
{
    clear();
}

list::iterator
list::push_back(const std::string& str)
{
    return insert_after(end(), str.c_str());
}

list::iterator
list::push_front(const std::string& str)
{
    auto n{ curl_slist_append(nullptr, str.c_str()) };
    if (nullptr == n) throw std::bad_alloc{};
    n->next = head__;
    head__  = n;
    if (tail__ == nullptr) tail__ = n;
    return iterator{ n };
}

constexpr list::iterator
list::index(size_t idx) const noexcept
{
    auto node{ head__ };
    for (size_t i{ 0 }; ((i != idx) && (nullptr != node)); ++i)
        node = node->next;
    return iterator{ node };
}

list::iterator
list::insert(size_t idx, const std::string& str)
{
    if (0 == idx) return push_front(str);
    return insert_after(index(idx - 1), str.c_str());
}

list::iterator
list::insert_after(list::iterator pos, const std::string& str)
{
    auto n{ curl_slist_append(nullptr, str.c_str()) };

    if (nullptr == n) throw std::bad_alloc{};
    if (nullptr == pos.cur__) pos.cur__ = tail__;
    if (pos.cur__)
    {
        n->next         = pos.cur__->next;
        pos.cur__->next = n;
    }
    else
    {
        n->next = nullptr;
        head__  = n;
    }
    if (nullptr == n->next) tail__ = n;
    return iterator{ n };
}

constexpr bool
list::empty() const noexcept
{
    return head__ == nullptr;
}

constexpr list::iterator
list::begin() const noexcept
{
    return list::iterator{ head__ };
}

constexpr list::iterator
list::end() const noexcept
{
    return list::iterator{ nullptr };
}

curl_slist*
list::release() noexcept
{
    tail__ = nullptr;
    return std::exchange(head__, nullptr);
}

void
list::remove(size_t idx)
{
    if (idx == 0)
    {
        auto ptr = head__;
        if (tail__ == ptr) tail__ = nullptr;
        if (ptr != nullptr)
        {
            head__    = ptr->next;
            ptr->next = nullptr;
            curl_slist_free_all(ptr);
        }
    }
    else
    {
        auto prev = head__;
        if (prev == nullptr) return;
        auto ptr = prev->next;
        for (size_t i = 1; i < idx && ptr != nullptr; i++)
        {
            prev = ptr;
            ptr  = ptr->next;
        }
        if (ptr != nullptr)
        {
            if (ptr == tail__) tail__ = prev;
            prev->next = ptr->next;
            ptr->next  = nullptr;
            curl_slist_free_all(ptr);
        }
    }
}

void
list::remove(iterator pos)
{
    if (pos.cur__ == nullptr) pos.cur__ = tail__;
    if (pos.cur__ == nullptr) return;
    if (pos.cur__ == head__)
    {
        head__ = pos.cur__->next;
        if (head__ == nullptr) tail__ = nullptr;
    }
    else
    {
        auto prev = head__;
        while (prev != nullptr && prev->next != pos.cur__)
        {
            prev = prev->next;
        }
        if (prev == nullptr) return;
        prev->next = pos.cur__->next;
        if (pos.cur__->next == nullptr) tail__ = prev;
    }
    pos.cur__->next = nullptr;
    curl_slist_free_all(pos.cur__);
}

void
list::clear()
{
    curl_slist_free_all(head__);
    head__ = nullptr;
    tail__ = nullptr;
}

} // namespace asyncurl
