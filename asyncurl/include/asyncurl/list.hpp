/**
 * Copyright (C) 2023 Osmozis SA - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

/**
 * @file list.hpp
 * @brief Wrapper around curl_slist
 * \see https://github.com/curl/curl/blob/master/lib/slist.c for more informations
 *
 * Basically, this is a linked list that is used to setup transfers options with list type value
 * One obvious example is to setup the headers to use (\see https://curl.se/libcurl/c/CURLOPT_HTTPHEADER.html)
 * @author lhm
 */

#ifndef INCLUDE_ASYNCURL_LIST_H
#define INCLUDE_ASYNCURL_LIST_H

#include <iterator>
#include <utility>

struct curl_slist;

namespace asyncurl
{
class handle;

class list
{
private:
    curl_slist* head__{ nullptr };
    curl_slist* tail__{ nullptr };
    friend class handle;

public:
    class iterator
    {
        friend class list;

    private:
        curl_slist* cur__{ nullptr };

    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = char*;
        using pointer           = value_type*;
        using reference         = value_type&;

        constexpr explicit iterator(curl_slist* elm = nullptr) noexcept;

        reference operator*() const noexcept;
        pointer   operator->() const noexcept;
        iterator& operator++() noexcept;
        iterator  operator++(int) noexcept;

        friend bool operator==(const iterator& lhs, const iterator& rhs);
        friend bool operator!=(const iterator& lhs, const iterator& rhs);
    };

public:
    inline static const struct owns_data
    {
    } owns;
    inline static const struct copy_data
    {
    } copy;

    // Constructors
    constexpr list() noexcept = default;
    list(curl_slist*, owns_data) noexcept;
    list(const curl_slist* raw, copy_data);
    list(const list& o);
    list& operator=(const list& o);
    list(list&& other) noexcept;
    list& operator=(list&& other) noexcept;
    ~list() noexcept;

    // Insertion / Deletion
    iterator push_back(const std::string& str);
    iterator push_front(const std::string& str);
    iterator insert(size_t idx, const std::string& str);
    void     remove(size_t idx);
    iterator insert_after(iterator it, const std::string& str);
    void     remove(iterator it);
    void     clear();

    constexpr iterator index(size_t idx) const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr iterator begin() const noexcept;
    constexpr iterator end() const noexcept;

    // Access to raw wrapped list
    [[nodiscard]] curl_slist* release() noexcept;
};

} // namespace asyncurl

#endif // INCLUDE_ASYNCURL_LIST_H
