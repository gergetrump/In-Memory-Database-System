// ---------------------------------------------------------------------------
// IMLAB
// ---------------------------------------------------------------------------
#ifndef IMLAB_INFRA_HASH_TABLE_H_
#define IMLAB_INFRA_HASH_TABLE_H_
//---------------------------------------------------------------------------
#include "imlab/infra/Bits.h"
#include "imlab/infra/Hash.h"
#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>
#include <tbb/enumerable_thread_specific.h>
//---------------------------------------------------------------------------
template <typename... T>
struct IsKey : std::false_type {};
template <typename... T>
struct IsKey<Key<T...>> : std::true_type {};
//---------------------------------------------------------------------------
template <typename T>
struct Tester;
//---------------------------------------------------------------------------
template <typename KeyT, typename ValueT>
class LazyMultiMap {
    // Check key type
    static_assert(IsKey<KeyT>::value, "The key of LazyMultiMap must be a Key<T>");

    protected:
    // Entry in the hash table
    struct Entry {
        // Pointer to the next element in the collision list
        Entry* next_;
        // Key of the hash table entry
        KeyT key_;
        // Value of the hash table entry
        ValueT value_;

        // Constructor
        Entry(KeyT key, ValueT value)
            : next_(nullptr), key_(key), value_(value) {}
    };

    // EqualRangeIterator for the hash table
    class EqualRangeIterator {
        public:
        using iterator_category = std::forward_iterator_tag;

        // Default constructor
        EqualRangeIterator() : ptr_(nullptr), key_() {}
        // Constructor
        explicit EqualRangeIterator(Entry* ptr, KeyT key) : ptr_(ptr), key_(key) {}
        // Destructor
        ~EqualRangeIterator() = default;

        // Postfix increment
        EqualRangeIterator operator++(int) {
            EqualRangeIterator tmp = *this;
            ++(*this);
            return tmp;
        }
        // Prefix increment
        EqualRangeIterator& operator++() {
            if (!ptr_) {
                return *this;
            }
            ptr_ = ptr_->next_;
            if (ptr_ && !(ptr_->key_ == key_)) {
                ptr_ = nullptr;
            }
            return *this;
        }
        // Reference
        Entry& operator*() { return *ptr_; }
        // Pointer
        Entry* operator->() { return ptr_; }
        // Equality
        bool operator==(const EqualRangeIterator& other) const { return ptr_ == other.ptr_; }
        // Inequality
        bool operator!=(const EqualRangeIterator& other) const { return ptr_ != other.ptr_; }

        protected:
        // Entry pointer
        Entry* ptr_;
        // Key that is being searched for
        KeyT key_;
    };

    public:
    // End of the equal range
    EqualRangeIterator end() { return EqualRangeIterator(); }

    // Insert an element into the hash table
    //  * Gather all entries with insert and build the hash table with finalize.
    void insert(const std::pair<KeyT, ValueT>& val) {
        entries_.local().emplace_back(val.first, val.second);
    }

    // Finalize the hash table
    //  * Get the next power of two, which is larger than the number of entries (include/imlab/infra/bits.h).
    //  * Resize the hash table to that size.
    //  * For each entry in entries_, calculate the hash and prepend it to the collision list in the hash table.
    void finalize() {
        size_t totalSize = 0;
        for (auto& localEntries : entries_) {
            totalSize += localEntries.size();
        }

        int hashTableSize = std::max(imlab::NextPow2_32(totalSize), static_cast<uint32_t>(32));
        hash_table_.resize(hashTableSize, nullptr);
        hash_table_mask_ = hashTableSize - 1;

        for (auto &slot : hash_table_) {
            slot = nullptr;
        }

        std::vector<Entry*> combined;
        combined.reserve(totalSize);
        for (auto &localEntries : entries_) {
            for (auto &entry : localEntries) {
                combined.push_back(&entry);
            }
        }

        // Prepend from the back so equal_range walks entries in insert order.
        for (size_t i = combined.size(); i-- > 0;) {
            Entry *e = combined[i];
            uint32_t h = e->key_.Hash() & hash_table_mask_;
            e->next_ = hash_table_[h];
            hash_table_[h] = e;
        }
    }

    // To find an element, calculate the hash (Key::Hash), and search this list until you reach a nullptr;
    std::pair<EqualRangeIterator, EqualRangeIterator> equal_range(KeyT key) {
        if (hash_table_.empty()) {
            return std::make_pair(end(), end());
        }

        auto hash = static_cast<uint32_t>(key.Hash()) & hash_table_mask_;
        Entry* current = hash_table_[hash];
        while (current != nullptr && !(current->key_ == key)) {
            current = current->next_;
        }

        if (current == nullptr) {
            return std::make_pair(end(), end());
        }

        return std::make_pair(EqualRangeIterator(current, key), end());
    }

    protected:
    // Entries of the hash table.
    tbb::enumerable_thread_specific<std::vector<Entry>> entries_;
    // The hash table.
    // Use the next_ pointers in the entries to store the collision list of the hash table.
    //
    //      hash_table_     entries_
    //      +---+           +---+
    //      | * | --------> | x | --+
    //      | 0 |           |   |   |
    //      | 0 |           |   |   |
    //      | 0 |           | z | <-+
    //      +---+           +---+
    //
    std::vector<Entry*> hash_table_;
    // The hash table mask.
    uint32_t hash_table_mask_;
};
//---------------------------------------------------------------------------
#endif // IMLAB_INFRA_HASH_TABLE_H_
//---------------------------------------------------------------------------
