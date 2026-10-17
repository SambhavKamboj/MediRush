#ifndef MEDIRUSH_INVENTORY_HPP
#define MEDIRUSH_INVENTORY_HPP

#include "models.hpp"

#include <unordered_map>
#include <vector>

namespace medirush {

/** In-memory inventory map; stock reservation checks the full batch before decrementing. */
class Inventory {
public:
    /** Set the starting quantity for a medicine. */
    void setStock(Id medicineId, int quantity);
    /** Return true when each medicine has enough stock for the complete order. */
    [[nodiscard]] bool canReserveBatch(const std::vector<PrescriptionItem>& items) const;
    /** Atomically check and decrement quantities for all items; throws if stock is short. */
    void reserveBatch(const std::vector<PrescriptionItem>& items);
    /** Return previously reserved quantities to stock. */
    void releaseBatch(const std::vector<PrescriptionItem>& items);
    /** Add units to inventory. */
    void restock(Id medicineId, int quantity);
    /** Return current stock, or zero when medicine has no inventory row. */
    [[nodiscard]] int stockFor(Id medicineId) const;

private:
    std::unordered_map<Id, int> quantityByMedicine_;
    [[nodiscard]] std::unordered_map<Id, int> aggregate(const std::vector<PrescriptionItem>& items) const;
};

} // namespace medirush

#endif
