////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
template <typename T>
struct Link
{
    Link<T>* next;
    Link<T>* prev;

    Link()
        : next(nullptr)
        , prev(nullptr)
    {}
};

template <typename T>
class List
{
    using link_type = Link<T>;
    link_type m_root;

public:
    void Insert(link_type* element);
    void Remove(link_type* element);

    size_t Size() const;
    bool   Empty() const;

    link_type* Head();
    link_type* Tail();
};

template <typename T>
void List<T>::Insert(link_type* element)
{
    Assert(element != nullptr);
    Assert(!((this->m_root.next == nullptr) ^ (this->m_root.prev == nullptr)));

    if (this->m_root.next != nullptr)
    {
        // The list is non-empty
        element->next = &(this->m_root);
        element->prev = this->m_root.prev;
        this->m_root.prev->next = element;
        this->m_root.prev = element;
    }
    else
    {
        // The list is empty
        element->prev = &(this->m_root);
        element->next = &(this->m_root);
        this->m_root.next = element;
        this->m_root.prev = element;
    }
}

template <typename T>
void List<T>::Remove(link_type* element)
{
    Assert(element != nullptr);
    Assert(element != &(this->m_root));
    Assert(this->m_root.next != nullptr);
    Assert(this->m_root.prev != nullptr);
    Assert(element->next != nullptr);
    Assert(element->prev != nullptr);

    // That means the list has exactly one element
    if (this->m_root.next == this->m_root.prev)
    {
        this->m_root.next = nullptr;
        this->m_root.prev = nullptr;
    }
    else
    {
        element->next->prev = element->prev;
        element->prev->next = element->next;
    }

    // Optionnal
    element->next = nullptr;
    element->prev = nullptr;
}

template <typename T>
size_t List<T>::Size() const
{
    if (this->m_root.next == nullptr)
        return 0;

    Assert(this->m_root.next != nullptr);

    const link_type* startLink = &(this->m_root);
    const link_type* currentLink = this->m_root.next;

    size_t size = 0;

    while (currentLink != startLink)
    {
        Assert(currentLink->next != nullptr);
        currentLink = currentLink->next;
        size++;
    }

    return size;
}

template <typename T>
bool List<T>::Empty() const
{
    // next and prev cannot be null if the other is not.
    Assert(!((this->m_root.next == nullptr) ^ (this->m_root.prev == nullptr)));
    return this->m_root.next == nullptr;
}

template <typename T>
Link<T>* List<T>::Head()
{
    return this->m_root.next;
}

template <typename T>
Link<T>* List<T>::Tail()
{
    return this->m_root.prev;
}
} // namespace Reaper
