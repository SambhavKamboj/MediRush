#include "models.hpp"

#include <stdexcept>
#include <utility>

namespace medirush {

std::string toString(Role value) {
    switch (value) {
        case Role::PATIENT: return "PATIENT";
        case Role::PHARMACIST_VERIFIER: return "PHARMACIST_VERIFIER";
        case Role::DELIVERY_MANAGER: return "DELIVERY_MANAGER";
        case Role::DELIVERY_AGENT: return "DELIVERY_AGENT";
        case Role::ADMIN: return "ADMIN";
    }
    return "UNKNOWN";
}

std::string toString(PrescriptionStatus value) {
    switch (value) {
        case PrescriptionStatus::PENDING: return "PENDING";
        case PrescriptionStatus::VERIFIED: return "VERIFIED";
        case PrescriptionStatus::REJECTED: return "REJECTED";
    }
    return "UNKNOWN";
}

std::string toString(OrderStatus value) {
    switch (value) {
        case OrderStatus::PENDING_VERIFICATION: return "PENDING_VERIFICATION";
        case OrderStatus::VERIFIED_QUEUED: return "VERIFIED_QUEUED";
        case OrderStatus::AGENT_ASSIGNED: return "AGENT_ASSIGNED";
        case OrderStatus::OUT_FOR_DELIVERY: return "OUT_FOR_DELIVERY";
        case OrderStatus::DELIVERED: return "DELIVERED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

bool User::hasRole(Role role) const {
    return roles.find(role) != roles.end();
}

bool OrderStateMachine::canTransition(OrderStatus from, OrderStatus to) {
    switch (from) {
        case OrderStatus::PENDING_VERIFICATION:
            return to == OrderStatus::VERIFIED_QUEUED || to == OrderStatus::REJECTED ||
                   to == OrderStatus::CANCELLED;
        case OrderStatus::VERIFIED_QUEUED:
            return to == OrderStatus::AGENT_ASSIGNED || to == OrderStatus::CANCELLED;
        case OrderStatus::AGENT_ASSIGNED:
            return to == OrderStatus::OUT_FOR_DELIVERY;
        case OrderStatus::OUT_FOR_DELIVERY:
            return to == OrderStatus::DELIVERED;
        case OrderStatus::DELIVERED:
        case OrderStatus::REJECTED:
        case OrderStatus::CANCELLED:
            return false;
    }
    return false;
}

void OrderStateMachine::require(OrderStatus from, OrderStatus to) {
    if (!canTransition(from, to)) {
        throw std::logic_error("Illegal order transition: " + toString(from) + " -> " + toString(to));
    }
}

Prescription::Prescription(Id id, Id patientId, std::string fileReference,
                           std::string patientNote, Timestamp createdAt)
    : id_(id), patientId_(patientId), fileReference_(std::move(fileReference)),
      patientNote_(std::move(patientNote)), createdAt_(createdAt) {}

void Prescription::markVerified(Id pharmacistId, std::vector<PrescriptionItem> items, Timestamp at) {
    if (status_ != PrescriptionStatus::PENDING) {
        throw std::logic_error("Only pending prescriptions can be verified");
    }
    if (items.empty()) {
        throw std::invalid_argument("At least one verified prescription item is required");
    }
    for (const auto& item : items) {
        if (item.medicineId <= 0 || item.dosageFrequencyHours <= 0 ||
            item.dosageFrequencyHours > 168 || item.unitsPerDose <= 0 || item.quantity <= 0) {
            throw std::invalid_argument("Prescription item contains invalid values");
        }
    }
    items_ = std::move(items);
    status_ = PrescriptionStatus::VERIFIED;
    verifiedBy_ = pharmacistId;
    verifiedAt_ = at;
}

void Prescription::markRejected(Id pharmacistId, const std::string& reason, bool mismatch, Timestamp at) {
    (void)pharmacistId; // Kept in the method signature to show who performed the decision.
    (void)at;
    if (status_ != PrescriptionStatus::PENDING) {
        throw std::logic_error("Only pending prescriptions can be rejected");
    }
    if (reason.size() < 5) {
        throw std::invalid_argument("Rejection reason must contain at least 5 characters");
    }
    rejectionReason_ = reason;
    mismatchFlag_ = mismatch;
    status_ = PrescriptionStatus::REJECTED;
}

Order::Order(Id id, Id patientId, Id prescriptionId, std::string deliveryAddress,
             Timestamp createdAt)
    : id_(id), patientId_(patientId), prescriptionId_(prescriptionId),
      deliveryAddress_(std::move(deliveryAddress)), createdAt_(createdAt) {
    history_.push_back({OrderStatus::PENDING_VERIFICATION, 0, createdAt_, "Order created"});
}

void Order::transition(OrderStatus next, Id actorId, Timestamp at, const std::string& note) {
    OrderStateMachine::require(status_, next);
    status_ = next;
    history_.push_back({status_, actorId, at, note});
}

} // namespace medirush
