# Aerora Universal GCS Agent Instructions

The `docs/` directory is the project's engineering specification.

Before implementing a non-trivial change:
1. identify the relevant requirement IDs;
2. read relevant architecture/safety/security documents;
3. use a Kiro Spec when the change is more than a trivial edit;
4. inspect the current implementation before proposing changes;
5. preserve QGC upstream architecture whenever possible;
6. run tests;
7. inspect the Git diff;
8. report changed files, tests and known limitations.

Do not autonomously approve production release, safety decisions, legal decisions or licensing decisions.

Before modifying code related to the current vehicle, inspect `docs/46_PRODUCT_BASELINE_ANALYSIS.md` and the relevant ICD/authority/power/safety documents.
