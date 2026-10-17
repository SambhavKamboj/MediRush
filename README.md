# MediRush

**A priority-based medication delivery system built in C++17.**

Most delivery systems treat every order the same. MediRush ranks orders by medical urgency, so a request for insulin is delivered before a request for vitamins, and nobody can fake urgency to jump the queue.

> Built as a DSA + OOP project. **Status: work in progress.** The core logic runs as an in-memory console prototype. The database, REST API and web apps are still being built.

## The core idea: claimed vs. verified data

If patients could type their own diagnosis or urgency, anyone could game the queue. So priority is calculated **only from data the system can verify**:

| Data | Source | Used in priority? |
|---|---|---|
| Prescription file | Patient uploads | No (reference only) |
| Patient note / diagnosis text | Patient types | **Never** |
| Medicine, dosage, quantity | **Pharmacist** enters after reading the prescription | Yes |
| Medicine criticality (1-10) | Pharmacy-managed table | Yes |
| Verification status | Pharmacist action | Yes (gate) |
| Time waited | System clock | Yes (aging) |

Doctor handwriting can't be reliably read by patients or software, so a pharmacist reads the prescription and enters the structured data. The system records that decision.

## Priority formula

```
priorityScore = (criticality x verificationMultiplier)
              + timeUrgencyBonus + agingBonus + overrideBonus
```

- Unverified orders never enter the dispatch queue and are clamped to a low
