#ifndef MEDIRUSH_SERVICE_HPP
#define MEDIRUSH_SERVICE_HPP

#include "dispatch_queue.hpp"
#include "inventory.hpp"
#include "priority_calculator.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace medirush {

/**
 * Small in-memory service joining the MediRush classes.
 * This is a learning/demo version: no REST server, database, JWT, or file upload is included.
 */
class MediRushService {
public:
    /** Create the first admin; refuses to create a second bootstrap admin. */
    Id bootstrapAdmin(const std::string& name, const std::string& email);
    /** Public registration always creates a PATIENT account. */
    Id registerPatient(const std::string& name, const std::string& email);
    /** Admin-only creation of a staff account with one or more non-exclusive staff roles. */
    Id createStaffAccount(Id actorId, const std::string& name, const std::string& email,
                          const std::set<Role>& roles);

    /** Setup helper for initial medicine catalog/stock in this in-memory version. */
    Id addMedicine(const std::string& name, const std::string& category,
                   int criticality, int initialStock, bool requiresColdChain = false,
                   bool controlled = false);

    /** Create a pending prescription and its one-to-one pending order. */
    Id submitPrescription(Id patientId, const std::string& fileReference,
                          const std::string& patientNote, const std::string& deliveryAddress,
                          Timestamp now);
    /** Pharmacist/admin verifies a prescription and supplies authoritative structured items. */
    void verifyPrescription(Id actorId, Id prescriptionId,
                            const std::vector<PrescriptionItem>& items, Timestamp now);
    /** Pharmacist/admin rejects a pending prescription with a reason. */
    void rejectPrescription(Id actorId, Id prescriptionId, const std::string& reason,
                            bool mismatch, Timestamp now);

    /** Pharmacist/admin overrides priority; a reason of at least 10 characters is required. */
    void overridePriority(Id actorId, Id orderId, const std::string& justification, Timestamp now);
    /** Pharmacist/admin marks a verified queued order as emergency. */
    void markEmergency(Id actorId, Id orderId, const std::string& justification, Timestamp now);

    /** Patient may cancel their own order before it is dispatched. */
    void cancelOrder(Id patientId, Id orderId, Timestamp now);
    /** Manager/admin dispatches the best stocked order to a specified available agent. */
    [[nodiscard]] std::optional<Id> dispatchNext(Id actorId, Id agentId, Timestamp now);
    /** Assigned agent accepts an order and starts delivery. */
    void acceptDelivery(Id agentId, Id orderId, Timestamp now);
    /** Assigned agent completes a delivery. */
    void completeDelivery(Id agentId, Id orderId, Timestamp now);
    /** Manager/admin changes an agent's availability. */
    void setAgentStatus(Id actorId, Id agentId, AgentStatus status);
    /** Manager/admin restocks medicine and releases any now-stockable held orders. */
    void restock(Id actorId, Id medicineId, int quantity);

    /** Return a sorted queue snapshot for a manager/admin. */
    [[nodiscard]] std::vector<HeapEntry> dispatchQueue(Id actorId, Timestamp now);
    [[nodiscard]] const User& user(Id id) const;
    [[nodiscard]] const Medicine& medicine(Id id) const;
    [[nodiscard]] const Prescription& prescription(Id id) const;
    [[nodiscard]] const Order& order(Id id) const;
    [[nodiscard]] int stockFor(Id medicineId) const;

private:
    std::unordered_map<Id, User> users_;
    std::unordered_map<Id, Medicine> medicines_;
    std::unordered_map<Id, Prescription> prescriptions_;
    std::unordered_map<Id, Order> orders_;
    std::unordered_map<Id, Id> orderByPrescription_;
    std::unordered_map<Id, AgentStatus> agentStatus_;
    std::unordered_set<Id> heldForStock_;
    Inventory inventory_;
    DispatchQueue queue_;
    PriorityCalculator priorityCalculator_;
    Id nextUserId_{1};
    Id nextMedicineId_{1};
    Id nextPrescriptionId_{1};
    Id nextOrderId_{1};

    const User& requireUser(Id id) const;
    const User& requireRole(Id id, Role role) const;
    const User& requireAnyRole(Id id, const std::set<Role>& roles) const;
    void requireActiveUser(const User& user) const;
    void releaseStockHeldOrders();
    [[nodiscard]] double calculateOrderScore(const Order& order, Timestamp now) const;
    [[nodiscard]] int maxCriticality(const Prescription& prescription) const;
};

} // namespace medirush

#endif
