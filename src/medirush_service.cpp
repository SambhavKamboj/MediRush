#include "medirush_service.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace medirush {
namespace {

std::string normalizeEmail(std::string email) {
    std::transform(email.begin(), email.end(), email.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return email;
}

bool isStaffRole(Role role) {
    return role == Role::PHARMACIST_VERIFIER || role == Role::DELIVERY_MANAGER ||
           role == Role::DELIVERY_AGENT;
}

} // namespace

const User& MediRushService::requireUser(Id id) const {
    const auto found = users_.find(id);
    if (found == users_.end()) throw std::out_of_range("User not found");
    requireActiveUser(found->second);
    return found->second;
}

void MediRushService::requireActiveUser(const User& userValue) const {
    if (!userValue.active) throw std::runtime_error("User account is inactive");
}

const User& MediRushService::requireRole(Id id, Role role) const {
    const User& actor = requireUser(id);
    if (!actor.hasRole(Role::ADMIN) && !actor.hasRole(role)) {
        throw std::runtime_error("User does not have the required role: " + toString(role));
    }
    return actor;
}

const User& MediRushService::requireAnyRole(Id id, const std::set<Role>& roles) const {
    const User& actor = requireUser(id);
    if (actor.hasRole(Role::ADMIN)) return actor;
    for (Role role : roles) if (actor.hasRole(role)) return actor;
    throw std::runtime_error("User does not have permission for this action");
}

Id MediRushService::bootstrapAdmin(const std::string& name, const std::string& email) {
    for (const auto& [id, existing] : users_) {
        (void)id;
        if (existing.hasRole(Role::ADMIN)) throw std::logic_error("An admin already exists");
        if (normalizeEmail(existing.email) == normalizeEmail(email)) throw std::invalid_argument("Email already exists");
    }
    if (name.empty() || email.empty()) throw std::invalid_argument("Name and email are required");
    const Id id = nextUserId_++;
    users_.emplace(id, User{id, name, email, {Role::ADMIN}, true});
    return id;
}

Id MediRushService::registerPatient(const std::string& name, const std::string& email) {
    if (name.empty() || email.empty()) throw std::invalid_argument("Name and email are required");
    for (const auto& [id, existing] : users_) {
        (void)id;
        if (normalizeEmail(existing.email) == normalizeEmail(email)) throw std::invalid_argument("Email already exists");
    }
    const Id id = nextUserId_++;
    users_.emplace(id, User{id, name, email, {Role::PATIENT}, true});
    return id;
}

Id MediRushService::createStaffAccount(Id actorId, const std::string& name,
                                      const std::string& email, const std::set<Role>& roles) {
    (void)requireRole(actorId, Role::ADMIN);
    if (name.empty() || email.empty() || roles.empty()) throw std::invalid_argument("Name, email and at least one role are required");
    for (Role role : roles) {
        if (!isStaffRole(role)) throw std::invalid_argument("Staff accounts may only have staff roles");
    }
    for (const auto& [id, existing] : users_) {
        (void)id;
        if (normalizeEmail(existing.email) == normalizeEmail(email)) throw std::invalid_argument("Email already exists");
    }
    const Id id = nextUserId_++;
    users_.emplace(id, User{id, name, email, roles, true});
    if (roles.count(Role::DELIVERY_AGENT) != 0U) agentStatus_[id] = AgentStatus::OFFLINE;
    return id;
}

Id MediRushService::addMedicine(const std::string& name, const std::string& category,
                                int criticality, int initialStock, bool requiresColdChain,
                                bool controlled) {
    // ASSUMPTION: This is a seed/setup helper in the console demo, not a public production endpoint.
    if (name.empty() || criticality < 1 || criticality > 10 || initialStock < 0) {
        throw std::invalid_argument("Invalid medicine name, criticality, or initial stock");
    }
    for (const auto& [id, existing] : medicines_) {
        (void)id;
        if (normalizeEmail(existing.name) == normalizeEmail(name)) throw std::invalid_argument("Medicine already exists");
    }
    const Id id = nextMedicineId_++;
    medicines_.emplace(id, Medicine{id, name, category, criticality, requiresColdChain, controlled, true});
    inventory_.setStock(id, initialStock);
    return id;
}

Id MediRushService::submitPrescription(Id patientId, const std::string& fileReference,
                                      const std::string& patientNote,
                                      const std::string& deliveryAddress, Timestamp now) {
    (void)requireRole(patientId, Role::PATIENT);
    if (fileReference.empty() || deliveryAddress.empty()) {
        throw std::invalid_argument("Prescription file reference and delivery address are required");
    }
    if (patientNote.size() > 1000) throw std::invalid_argument("Patient note exceeds 1000 characters");

    const Id prescriptionId = nextPrescriptionId_++;
    const Id orderId = nextOrderId_++;
    prescriptions_.emplace(prescriptionId,
        Prescription{prescriptionId, patientId, fileReference, patientNote, now});
    orders_.emplace(orderId, Order{orderId, patientId, prescriptionId, deliveryAddress, now});
    orderByPrescription_[prescriptionId] = orderId;
    return prescriptionId;
}

void MediRushService::verifyPrescription(Id actorId, Id prescriptionId,
                                         const std::vector<PrescriptionItem>& items,
                                         Timestamp now) {
    (void)requireAnyRole(actorId, {Role::PHARMACIST_VERIFIER});
    auto pIt = prescriptions_.find(prescriptionId);
    if (pIt == prescriptions_.end()) throw std::out_of_range("Prescription not found");
    Prescription& p = pIt->second;
    if (p.status() != PrescriptionStatus::PENDING) throw std::logic_error("Prescription has already been processed");
    if (items.empty()) throw std::invalid_argument("At least one medicine item is required");
    for (const auto& item : items) {
        const Medicine& med = medicine(item.medicineId);
        if (!med.active) throw std::invalid_argument("Inactive medicine cannot be used");
    }
    const auto orderRef = orderByPrescription_.find(prescriptionId);
    if (orderRef == orderByPrescription_.end()) throw std::logic_error("Prescription has no linked order");
    Order& o = orders_.at(orderRef->second);

    p.markVerified(actorId, items, now);
    o.setQueuedAt(now);
    o.transition(OrderStatus::VERIFIED_QUEUED, actorId, now, "Prescription verified");
    const double score = calculateOrderScore(o, now);
    o.setPriorityScore(score);
    queue_.enqueue({o.id(), score, o.queuedAt()});
}

void MediRushService::rejectPrescription(Id actorId, Id prescriptionId,
                                         const std::string& reason, bool mismatch,
                                         Timestamp now) {
    (void)requireAnyRole(actorId, {Role::PHARMACIST_VERIFIER});
    auto pIt = prescriptions_.find(prescriptionId);
    if (pIt == prescriptions_.end()) throw std::out_of_range("Prescription not found");
    Prescription& p = pIt->second;
    p.markRejected(actorId, reason, mismatch, now);
    const auto orderRef = orderByPrescription_.find(prescriptionId);
    if (orderRef == orderByPrescription_.end()) throw std::logic_error("Prescription has no linked order");
    orders_.at(orderRef->second).transition(OrderStatus::REJECTED, actorId, now, reason);
}

int MediRushService::maxCriticality(const Prescription& prescriptionValue) const {
    int result = 1;
    for (const auto& item : prescriptionValue.items()) result = std::max(result, medicine(item.medicineId).criticality);
    return result;
}

double MediRushService::calculateOrderScore(const Order& orderValue, Timestamp now) const {
    const Prescription& p = prescription(orderValue.prescriptionId());
    const double waitingMinutes = orderValue.queuedAt() > 0
        ? static_cast<double>(std::max<Timestamp>(0, now - orderValue.queuedAt())) / 60.0
        : 0.0;
    // ASSUMPTION: This small demo uses the spec's DEFAULT_HOURS_REMAINING (24h).
    // Patient-specific last-delivery history is not implemented in this basic in-memory version.
    PriorityContext context{maxCriticality(p), p.status() == PrescriptionStatus::VERIFIED,
                            24.0, waitingMinutes, orderValue.overrideActive(), orderValue.emergency()};
    return priorityCalculator_.calculate(context);
}

void MediRushService::overridePriority(Id actorId, Id orderId, const std::string& justification,
                                       Timestamp now) {
    (void)requireAnyRole(actorId, {Role::PHARMACIST_VERIFIER});
    if (justification.size() < 10) throw std::invalid_argument("Justification must contain at least 10 characters");
    auto found = orders_.find(orderId);
    if (found == orders_.end()) throw std::out_of_range("Order not found");
    Order& o = found->second;
    if (o.status() != OrderStatus::VERIFIED_QUEUED) throw std::logic_error("Only queued orders can be overridden");
    o.setOverrideActive(true);
    const double score = calculateOrderScore(o, now);
    o.setPriorityScore(score);
    if (!queue_.update(orderId, score)) throw std::logic_error("Queued order was missing from heap");
}

void MediRushService::markEmergency(Id actorId, Id orderId, const std::string& justification,
                                    Timestamp now) {
    (void)requireAnyRole(actorId, {Role::PHARMACIST_VERIFIER});
    if (justification.size() < 10) throw std::invalid_argument("Justification must contain at least 10 characters");
    auto found = orders_.find(orderId);
    if (found == orders_.end()) throw std::out_of_range("Order not found");
    Order& o = found->second;
    if (o.status() != OrderStatus::VERIFIED_QUEUED) throw std::logic_error("Only queued orders can be marked emergency");
    o.setEmergency(true);
    const double score = calculateOrderScore(o, now);
    o.setPriorityScore(score);
    if (!queue_.update(orderId, score)) throw std::logic_error("Queued order was missing from heap");
}

void MediRushService::cancelOrder(Id patientId, Id orderId, Timestamp now) {
    (void)requireRole(patientId, Role::PATIENT);
    auto found = orders_.find(orderId);
    if (found == orders_.end() || found->second.patientId() != patientId) {
        throw std::out_of_range("Order not found"); // Do not reveal other patients' orders.
    }
    Order& o = found->second;
    if (o.status() != OrderStatus::PENDING_VERIFICATION && o.status() != OrderStatus::VERIFIED_QUEUED) {
        throw std::logic_error("Order can no longer be cancelled");
    }
    (void)queue_.remove(orderId);
    heldForStock_.erase(orderId);
    o.transition(OrderStatus::CANCELLED, patientId, now, "Cancelled by patient");
}

void MediRushService::releaseStockHeldOrders() {
    for (auto it = heldForStock_.begin(); it != heldForStock_.end();) {
        const auto orderIt = orders_.find(*it);
        if (orderIt == orders_.end() || orderIt->second.status() != OrderStatus::VERIFIED_QUEUED) {
            it = heldForStock_.erase(it);
            continue;
        }
        const Prescription& p = prescription(orderIt->second.prescriptionId());
        if (inventory_.canReserveBatch(p.items())) {
            const Order& o = orderIt->second;
            queue_.enqueue({o.id(), o.priorityScore(), o.queuedAt()});
            it = heldForStock_.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<Id> MediRushService::dispatchNext(Id actorId, Id agentId, Timestamp now) {
    (void)requireRole(actorId, Role::DELIVERY_MANAGER);
    const User& agent = requireRole(agentId, Role::DELIVERY_AGENT);
    (void)agent;
    const auto agentStatus = agentStatus_.find(agentId);
    if (agentStatus == agentStatus_.end() || agentStatus->second != AgentStatus::AVAILABLE) {
        throw std::logic_error("Delivery agent is not available");
    }

    // Refresh aging scores before dispatch. Rebuild is O(n).
    (void)dispatchQueue(actorId, now);
    for (int attempt = 0; attempt < 20 && !queue_.empty(); ++attempt) {
        const HeapEntry entry = queue_.dequeueNext();
        auto orderIt = orders_.find(entry.orderId);
        if (orderIt == orders_.end() || orderIt->second.status() != OrderStatus::VERIFIED_QUEUED) continue;
        Order& o = orderIt->second;
        const Prescription& p = prescription(o.prescriptionId());
        if (!inventory_.canReserveBatch(p.items())) {
            heldForStock_.insert(o.id());
            continue;
        }
        inventory_.reserveBatch(p.items());
        o.assignAgent(agentId);
        o.transition(OrderStatus::AGENT_ASSIGNED, actorId, now, "Dispatched by manager");
        agentStatus_[agentId] = AgentStatus::BUSY;
        return o.id();
    }
    return std::nullopt;
}

void MediRushService::acceptDelivery(Id agentId, Id orderId, Timestamp now) {
    (void)requireRole(agentId, Role::DELIVERY_AGENT);
    auto found = orders_.find(orderId);
    if (found == orders_.end()) throw std::out_of_range("Order not found");
    Order& o = found->second;
    if (o.assignedAgentId() != agentId) throw std::runtime_error("Order is assigned to another agent");
    o.transition(OrderStatus::OUT_FOR_DELIVERY, agentId, now, "Agent accepted delivery");
}

void MediRushService::completeDelivery(Id agentId, Id orderId, Timestamp now) {
    (void)requireRole(agentId, Role::DELIVERY_AGENT);
    auto found = orders_.find(orderId);
    if (found == orders_.end()) throw std::out_of_range("Order not found");
    Order& o = found->second;
    if (o.assignedAgentId() != agentId) throw std::runtime_error("Order is assigned to another agent");
    o.transition(OrderStatus::DELIVERED, agentId, now, "Delivery completed");
    o.setDeliveredAt(now);
    agentStatus_[agentId] = AgentStatus::AVAILABLE;
}

void MediRushService::setAgentStatus(Id actorId, Id agentId, AgentStatus status) {
    (void)requireRole(actorId, Role::DELIVERY_MANAGER);
    (void)requireRole(agentId, Role::DELIVERY_AGENT);
    agentStatus_[agentId] = status;
}

void MediRushService::restock(Id actorId, Id medicineId, int quantity) {
    (void)requireRole(actorId, Role::DELIVERY_MANAGER);
    (void)medicine(medicineId);
    inventory_.restock(medicineId, quantity);
    releaseStockHeldOrders();
}

std::vector<HeapEntry> MediRushService::dispatchQueue(Id actorId, Timestamp now) {
    (void)requireRole(actorId, Role::DELIVERY_MANAGER);
    std::vector<HeapEntry> updated;
    const auto oldEntries = queue_.snapshotSorted();
    updated.reserve(oldEntries.size());
    for (const auto& entry : oldEntries) {
        const auto found = orders_.find(entry.orderId);
        if (found == orders_.end() || found->second.status() != OrderStatus::VERIFIED_QUEUED) continue;
        Order& o = found->second;
        const double score = calculateOrderScore(o, now);
        o.setPriorityScore(score);
        updated.push_back({o.id(), score, o.queuedAt()});
    }
    queue_.rebuild(updated);
    return queue_.snapshotSorted();
}

const User& MediRushService::user(Id id) const {
    const auto found = users_.find(id);
    if (found == users_.end()) throw std::out_of_range("User not found");
    return found->second;
}

const Medicine& MediRushService::medicine(Id id) const {
    const auto found = medicines_.find(id);
    if (found == medicines_.end()) throw std::out_of_range("Medicine not found");
    return found->second;
}

const Prescription& MediRushService::prescription(Id id) const {
    const auto found = prescriptions_.find(id);
    if (found == prescriptions_.end()) throw std::out_of_range("Prescription not found");
    return found->second;
}

const Order& MediRushService::order(Id id) const {
    const auto found = orders_.find(id);
    if (found == orders_.end()) throw std::out_of_range("Order not found");
    return found->second;
}

int MediRushService::stockFor(Id medicineId) const {
    return inventory_.stockFor(medicineId);
}

} // namespace medirush
