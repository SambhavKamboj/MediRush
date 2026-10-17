#include "inventory.hpp"

#include <stdexcept>

namespace medirush {

void Inventory::setStock(Id medicineId, int quantity) {
    if (medicineId <= 0 || quantity < 0) throw std::invalid_argument("Invalid medicine ID or stock quantity");
    quantityByMedicine_[medicineId] = quantity;
}

std::unordered_map<Id, int> Inventory::aggregate(const std::vector<PrescriptionItem>& items) const {
    std::unordered_map<Id, int> required;
    for (const auto& item : items) {
        if (item.medicineId <= 0 || item.quantity <= 0) throw std::invalid_argument("Invalid item quantity");
        required[item.medicineId] += item.quantity;
    }
    return required;
}

bool Inventory::canReserveBatch(const std::vector<PrescriptionItem>& items) const {
    const auto required = aggregate(items);
    for (const auto& [medicineId, quantity] : required) {
        const auto found = quantityByMedicine_.find(medicineId);
        if (found == quantityByMedicine_.end() || found->second < quantity) return false;
    }
    return true;
}

void Inventory::reserveBatch(const std::vector<PrescriptionItem>& items) {
    const auto required = aggregate(items);
    for (const auto& [medicineId, quantity] : required) {
        const auto found = quantityByMedicine_.find(medicineId);
        if (found == quantityByMedicine_.end() || found->second < quantity) {
            throw std::runtime_error("Insufficient stock for medicine ID " + std::to_string(medicineId));
        }
    }
    for (const auto& [medicineId, quantity] : required) quantityByMedicine_[medicineId] -= quantity;
}

void Inventory::releaseBatch(const std::vector<PrescriptionItem>& items) {
    const auto required = aggregate(items);
    for (const auto& [medicineId, quantity] : required) quantityByMedicine_[medicineId] += quantity;
}

void Inventory::restock(Id medicineId, int quantity) {
    if (medicineId <= 0 || quantity <= 0) throw std::invalid_argument("Restock quantity must be positive");
    quantityByMedicine_[medicineId] += quantity;
}

int Inventory::stockFor(Id medicineId) const {
    const auto found = quantityByMedicine_.find(medicineId);
    return found == quantityByMedicine_.end() ? 0 : found->second;
}

} // namespace medirush
