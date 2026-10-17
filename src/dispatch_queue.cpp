#include "dispatch_queue.hpp"

#include <stdexcept>
#include <utility>
#include <unordered_set>

namespace medirush {

bool DispatchQueue::higherPriority(const HeapEntry& lhs, const HeapEntry& rhs) {
    if (lhs.score != rhs.score) return lhs.score > rhs.score;
    if (lhs.queuedAt != rhs.queuedAt) return lhs.queuedAt < rhs.queuedAt;
    return lhs.orderId < rhs.orderId;
}

void DispatchQueue::swapEntries(std::size_t a, std::size_t b) {
    std::swap(heap_[a], heap_[b]);
    indexByOrderId_[heap_[a].orderId] = a;
    indexByOrderId_[heap_[b].orderId] = b;
}

void DispatchQueue::siftUp(std::size_t index) {
    while (index > 0) {
        const std::size_t parent = (index - 1) / 2;
        if (!higherPriority(heap_[index], heap_[parent])) break;
        swapEntries(index, parent);
        index = parent;
    }
}

void DispatchQueue::siftDown(std::size_t index) {
    for (;;) {
        const std::size_t left = index * 2 + 1;
        const std::size_t right = left + 1;
        std::size_t best = index;
        if (left < heap_.size() && higherPriority(heap_[left], heap_[best])) best = left;
        if (right < heap_.size() && higherPriority(heap_[right], heap_[best])) best = right;
        if (best == index) break;
        swapEntries(index, best);
        index = best;
    }
}

void DispatchQueue::repair(std::size_t index) {
    if (index >= heap_.size()) return;
    if (index > 0 && higherPriority(heap_[index], heap_[(index - 1) / 2])) {
        siftUp(index);
    } else {
        siftDown(index);
    }
}

void DispatchQueue::enqueue(const HeapEntry& entry) {
    if (contains(entry.orderId)) throw std::invalid_argument("Order is already in dispatch queue");
    indexByOrderId_[entry.orderId] = heap_.size();
    heap_.push_back(entry);
    siftUp(heap_.size() - 1);
}

HeapEntry DispatchQueue::dequeueNext() {
    if (heap_.empty()) throw std::out_of_range("Dispatch queue is empty");
    const HeapEntry result = heap_.front();
    (void)remove(result.orderId);
    return result;
}

const HeapEntry& DispatchQueue::peek() const {
    if (heap_.empty()) throw std::out_of_range("Dispatch queue is empty");
    return heap_.front();
}

bool DispatchQueue::update(Id orderId, double newScore) {
    const auto found = indexByOrderId_.find(orderId);
    if (found == indexByOrderId_.end()) return false;
    const std::size_t index = found->second;
    heap_[index].score = newScore;
    repair(index);
    return true;
}

bool DispatchQueue::remove(Id orderId) {
    const auto found = indexByOrderId_.find(orderId);
    if (found == indexByOrderId_.end()) return false;

    const std::size_t index = found->second;
    const std::size_t last = heap_.size() - 1;
    indexByOrderId_.erase(found);
    if (index == last) {
        heap_.pop_back();
        return true;
    }

    heap_[index] = heap_.back();
    heap_.pop_back();
    indexByOrderId_[heap_[index].orderId] = index;
    repair(index);
    return true;
}

void DispatchQueue::rebuild(const std::vector<HeapEntry>& entries) {
    std::unordered_set<Id> seen;
    seen.reserve(entries.size());
    for (const auto& entry : entries) {
        if (!seen.insert(entry.orderId).second) {
            throw std::invalid_argument("Duplicate order ID when rebuilding dispatch queue");
        }
    }

    heap_ = entries;
    indexByOrderId_.clear();
    indexByOrderId_.reserve(heap_.size());
    for (std::size_t i = 0; i < heap_.size(); ++i) indexByOrderId_[heap_[i].orderId] = i;
    for (std::size_t i = heap_.size() / 2; i > 0; --i) siftDown(i - 1);
}

std::vector<HeapEntry> DispatchQueue::snapshotSorted() const {
    DispatchQueue copy = *this;
    std::vector<HeapEntry> result;
    result.reserve(copy.size());
    while (!copy.empty()) result.push_back(copy.dequeueNext());
    return result;
}

bool DispatchQueue::contains(Id orderId) const {
    return indexByOrderId_.find(orderId) != indexByOrderId_.end();
}

} // namespace medirush
