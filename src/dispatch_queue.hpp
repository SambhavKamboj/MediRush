#ifndef MEDIRUSH_DISPATCH_QUEUE_HPP
#define MEDIRUSH_DISPATCH_QUEUE_HPP

#include "models.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace medirush {

struct HeapEntry {
    Id orderId{};
    double score{};
    Timestamp queuedAt{};
};

/** Custom indexed max-heap: push/pop/update/remove are O(log n), top is O(1), rebuild is O(n). */
class DispatchQueue {
public:
    /** Add an order to the queue. Duplicate order IDs are rejected. */
    void enqueue(const HeapEntry& entry);
    /** Remove and return the highest-priority entry. Throws when empty. */
    [[nodiscard]] HeapEntry dequeueNext();
    /** Return the highest-priority entry. Throws when empty. */
    [[nodiscard]] const HeapEntry& peek() const;
    /** Update a queued order's score in O(log n). Returns false if not found. */
    bool update(Id orderId, double newScore);
    /** Remove an order by ID in O(log n). Returns false if not found. */
    bool remove(Id orderId);
    /** Replace the heap entries and rebuild the index in O(n). */
    void rebuild(const std::vector<HeapEntry>& entries);
    /** Return all entries in dispatch order without changing this queue. */
    [[nodiscard]] std::vector<HeapEntry> snapshotSorted() const;
    [[nodiscard]] bool contains(Id orderId) const;
    [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return heap_.size(); }

private:
    std::vector<HeapEntry> heap_;
    std::unordered_map<Id, std::size_t> indexByOrderId_;

    /** True when lhs must appear before rhs in the max-heap. */
    [[nodiscard]] static bool higherPriority(const HeapEntry& lhs, const HeapEntry& rhs);
    void swapEntries(std::size_t a, std::size_t b);
    void siftUp(std::size_t index);
    void siftDown(std::size_t index);
    void repair(std::size_t index);
};

} // namespace medirush

#endif
