#ifndef MEDIRUSH_MODELS_HPP
#define MEDIRUSH_MODELS_HPP

#include <cstdint>
#include <list>
#include <set>
#include <string>
#include <vector>

namespace medirush {

using Id = std::int64_t;
using Timestamp = std::int64_t; // Unix epoch seconds.

enum class Role {
    PATIENT,
    PHARMACIST_VERIFIER,
    DELIVERY_MANAGER,
    DELIVERY_AGENT,
    ADMIN
};

enum class PrescriptionStatus { PENDING, VERIFIED, REJECTED };
enum class OrderStatus {
    PENDING_VERIFICATION,
    VERIFIED_QUEUED,
    AGENT_ASSIGNED,
    OUT_FOR_DELIVERY,
    DELIVERED,
    REJECTED,
    CANCELLED
};

enum class AgentStatus { AVAILABLE, BUSY, OFFLINE };

/** Convert a role to its stable display name. */
std::string toString(Role value);
/** Convert a prescription status to its stable display name. */
std::string toString(PrescriptionStatus value);
/** Convert an order status to its stable display name. */
std::string toString(OrderStatus value);

/** Basic account model. Roles are stored as a set so staff roles can be combined. */
struct User {
    Id id{};
    std::string name;
    std::string email;
    std::set<Role> roles;
    bool active{true};

    /** Return true when the user holds the requested role. */
    [[nodiscard]] bool hasRole(Role role) const;
};

/** Medicine reference data used by the priority calculator. */
struct Medicine {
    Id id{};
    std::string name;
    std::string category;
    int criticality{1}; // 1..10
    bool requiresColdChain{false};
    bool controlled{false};
    bool active{true};
};

/** Structured prescription data entered by a pharmacist, not parsed from the file. */
struct PrescriptionItem {
    Id medicineId{};
    int dosageFrequencyHours{};
    int unitsPerDose{1};
    int quantity{};
};

/** One order-status event in chronological order. */
struct OrderStatusEvent {
    OrderStatus status{OrderStatus::PENDING_VERIFICATION};
    Id actorId{};
    Timestamp at{};
    std::string note;
};

/** Stores the legal order-state transitions in one place. */
class OrderStateMachine {
public:
    /** Return whether a transition is legal. */
    [[nodiscard]] static bool canTransition(OrderStatus from, OrderStatus to);
    /** Throw std::logic_error when the transition is illegal. */
    static void require(OrderStatus from, OrderStatus to);
};

/** Prescription status can only be changed through markVerified/markRejected. */
class Prescription {
public:
    Prescription(Id id, Id patientId, std::string fileReference,
                 std::string patientNote, Timestamp createdAt);

    [[nodiscard]] Id id() const noexcept { return id_; }
    [[nodiscard]] Id patientId() const noexcept { return patientId_; }
    [[nodiscard]] const std::string& fileReference() const noexcept { return fileReference_; }
    [[nodiscard]] const std::string& patientNote() const noexcept { return patientNote_; }
    [[nodiscard]] PrescriptionStatus status() const noexcept { return status_; }
    [[nodiscard]] const std::vector<PrescriptionItem>& items() const noexcept { return items_; }
    [[nodiscard]] const std::string& rejectionReason() const noexcept { return rejectionReason_; }
    [[nodiscard]] bool mismatchFlag() const noexcept { return mismatchFlag_; }
    [[nodiscard]] Timestamp createdAt() const noexcept { return createdAt_; }
    [[nodiscard]] Id verifiedBy() const noexcept { return verifiedBy_; }
    [[nodiscard]] Timestamp verifiedAt() const noexcept { return verifiedAt_; }

    /** Verify a pending prescription and attach pharmacist-entered structured items. */
    void markVerified(Id pharmacistId, std::vector<PrescriptionItem> items, Timestamp at);
    /** Reject a pending prescription with a human-readable reason. */
    void markRejected(Id pharmacistId, const std::string& reason, bool mismatch, Timestamp at);

private:
    Id id_;
    Id patientId_;
    std::string fileReference_;
    std::string patientNote_; // Display-only context; never passed to the priority engine.
    PrescriptionStatus status_{PrescriptionStatus::PENDING};
    std::vector<PrescriptionItem> items_;
    std::string rejectionReason_;
    bool mismatchFlag_{false};
    Id verifiedBy_{};
    Timestamp verifiedAt_{};
    Timestamp createdAt_{};
};

/** Order model with a linked-list status history. */
class Order {
public:
    Order(Id id, Id patientId, Id prescriptionId, std::string deliveryAddress,
          Timestamp createdAt);

    [[nodiscard]] Id id() const noexcept { return id_; }
    [[nodiscard]] Id patientId() const noexcept { return patientId_; }
    [[nodiscard]] Id prescriptionId() const noexcept { return prescriptionId_; }
    [[nodiscard]] OrderStatus status() const noexcept { return status_; }
    [[nodiscard]] const std::string& deliveryAddress() const noexcept { return deliveryAddress_; }
    [[nodiscard]] Timestamp createdAt() const noexcept { return createdAt_; }
    [[nodiscard]] Timestamp queuedAt() const noexcept { return queuedAt_; }
    [[nodiscard]] Timestamp deliveredAt() const noexcept { return deliveredAt_; }
    [[nodiscard]] double priorityScore() const noexcept { return priorityScore_; }
    [[nodiscard]] bool overrideActive() const noexcept { return overrideActive_; }
    [[nodiscard]] bool emergency() const noexcept { return emergency_; }
    [[nodiscard]] Id assignedAgentId() const noexcept { return assignedAgentId_; }
    [[nodiscard]] const std::list<OrderStatusEvent>& history() const noexcept { return history_; }

    /** Apply a legal transition and append it to the status trail. */
    void transition(OrderStatus next, Id actorId, Timestamp at, const std::string& note);
    /** Set the time the order entered the verified dispatch queue. */
    void setQueuedAt(Timestamp at) noexcept { queuedAt_ = at; }
    /** Set the calculated priority score. */
    void setPriorityScore(double score) noexcept { priorityScore_ = score; }
    /** Record the assigned delivery agent. */
    void assignAgent(Id agentId) noexcept { assignedAgentId_ = agentId; }
    /** Record that a pharmacist/admin override is active. */
    void setOverrideActive(bool active) noexcept { overrideActive_ = active; }
    /** Record that a pharmacist/admin marked the order as emergency. */
    void setEmergency(bool active) noexcept { emergency_ = active; }
    /** Record delivery completion time. */
    void setDeliveredAt(Timestamp at) noexcept { deliveredAt_ = at; }

private:
    Id id_;
    Id patientId_;
    Id prescriptionId_;
    OrderStatus status_{OrderStatus::PENDING_VERIFICATION};
    std::string deliveryAddress_;
    Timestamp createdAt_{};
    Timestamp queuedAt_{};
    Timestamp deliveredAt_{};
    double priorityScore_{0.0};
    bool overrideActive_{false};
    bool emergency_{false};
    Id assignedAgentId_{};
    std::list<OrderStatusEvent> history_;
};

} // namespace medirush

#endif
