#include "medirush_service.hpp"

#include <iomanip>
#include <iostream>
#include <set>
#include <vector>

int main() {
    using namespace medirush;

    try {
        MediRushService app;
        const Id adminId = app.bootstrapAdmin("Pharmacy Owner", "admin@pharmacy.local");
        const Id pharmacistId = app.createStaffAccount(
            adminId, "Pharmacist", "pharmacist@pharmacy.local", {Role::PHARMACIST_VERIFIER});
        const Id managerId = app.createStaffAccount(
            adminId, "Delivery Manager", "manager@pharmacy.local", {Role::DELIVERY_MANAGER});
        const Id agentId = app.createStaffAccount(
            adminId, "Delivery Agent", "agent@pharmacy.local", {Role::DELIVERY_AGENT});
        app.setAgentStatus(managerId, agentId, AgentStatus::AVAILABLE);

        const Id insulinId = app.addMedicine("Insulin (rapid-acting)", "Diabetes", 10, 40, true);
        const Id vitaminId = app.addMedicine("Vitamin C", "Supplement", 1, 500);
        const Id patientA = app.registerPatient("Patient A", "patient.a@example.com");
        const Id patientB = app.registerPatient("Patient B", "patient.b@example.com");

        // Patient-written notes are stored for the pharmacist but never passed to the scorer.
        const Id prescriptionA = app.submitPrescription(
            patientA, "prescription-a.pdf", "Please help, this is URGENT!", "12 Example Street", 1000);
        const Id prescriptionB = app.submitPrescription(
            patientB, "prescription-b.pdf", "Routine refill", "24 Example Street", 1000);

        app.verifyPrescription(pharmacistId, prescriptionA, {{insulinId, 8, 1, 14}}, 1010);
        app.verifyPrescription(pharmacistId, prescriptionB, {{vitaminId, 24, 1, 30}}, 1020);

        std::cout << "MediRush dispatch queue\n";
        for (const auto& entry : app.dispatchQueue(managerId, 1620)) {
            std::cout << "Order " << entry.orderId << " | score " << std::fixed << std::setprecision(2)
                      << entry.score << "\n";
        }

        const auto dispatched = app.dispatchNext(managerId, agentId, 1620);
        if (dispatched.has_value()) {
            std::cout << "\nDispatched order " << *dispatched << "\n";
            app.acceptDelivery(agentId, *dispatched, 1700);
            app.completeDelivery(agentId, *dispatched, 2200);
            std::cout << "Final status: " << toString(app.order(*dispatched).status()) << "\n";
            std::cout << "Insulin stock left: " << app.stockFor(insulinId) << "\n";
        } else {
            std::cout << "No order with sufficient stock is ready to dispatch.\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "MediRush error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
