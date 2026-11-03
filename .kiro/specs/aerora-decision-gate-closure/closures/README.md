# Closure_Records

This directory holds one Closure_Record per Gate part. It is empty, and that is its correct state.

A Closure_Record's content is an owner's signature over a decision. This spec built the shape a
signature goes into and the checks that read it back; it did not write any, because writing the
content would be the engineering forging the signature it exists to demand.

## One file per part, not per Gate

The unit of signature is the **part**. Requirement 3 criterion 6 gives parts separate owners,
separate Sign_Off_Conditions and separate acceptance, and six Gates divide their Decision_Content
into parts that can be accepted independently — `DG-PAY` has four. So the unit of file is the part
too:

```text
closures/<GATE>.<part>.json
```

Both components come from the register. `<GATE>` is one of the ten identifiers; `<part>` is the
`id` of a part the Gate_Dossier records, which the schema constrains to a filename-safe slug for
this reason. `closures/DG-TLM.adr-016-core-edit.json` is a valid name; `closures/DG-TLM.json` is
not, because `DG-TLM` has two parts with different signing owners.

Each record validates against [`closure-record.schema.json`](../closure-record.schema.json).

## `entryCommit` is derived, and absent until it can be

Requirement 3 criterion 1 requires the record to carry the commit at which it enters the
repository — which a file cannot contain about itself. Git already knows it:

```bash
git log --diff-filter=A --format=%H -- .kiro/specs/aerora-decision-gate-closure/closures/<GATE>.<part>.json
```

So `entryCommit` is `null` while the record's `state` is `submitted`, and a 40-character hex string
once it is `accepted`. `closure-entry-commit` resolves the add-commit and fails on disagreement. If
git cannot answer — no `.git`, or a shallow clone — the check fails naming that reason rather than
skipping, because an unresolved premise is a failure and never a pass. The `pre-commit.yml` leg
checks out full history, so CI can answer.

## Signatures are recorded identity, not cryptographic authorship

There is no PKI here, and `.github/CODEOWNERS` routes no review today — every rule in it is
commented out. A signature is therefore three things, and the checks say exactly which of them they
verify:

1. a row naming the owner **label the Gate_Dossier records** for that part, with a sign-off date.
   The labels are the ones `requirements.md` names — `Product`, `Firmware`, `Architecture`,
   `operations`, `engineering`, `security architecture`, `the external interface authority`,
   `the process owner`, `Tab 1`..`Tab 8` — and no other string is representable;
2. a `Gate-Signed-By:` trailer on the commit that adds the record, which
   `git log --format=%(trailers)` reads back. `closure-signatures` compares the trailer set against
   the recorded owner set;
3. optionally a GPG-signed commit, recorded as `signatureVerified` when `git verify-commit`
   succeeds.

`closure-signatures` asserts set equality in both directions: every owner the dossier records for
the part has signed, and no signature comes from a party the dossier does not record as an owner of
that part. What it cannot assert is that the named human agreed. `AeroraSecurityStatus` is the
honest precedent — the station reports *not enforced* rather than claiming a boundary it does not
evaluate — and this register reports **recorded identity**, which its own output says. Making that
stronger needs a repository setting (branch protection requiring code-owner review on this path),
which is outside the repository and is named as such rather than assumed.

## What happens when a record lands here

Adding a file to this directory is what closes a Gate part, so it moves several derived things at
once and every one of them is checked:

| Check | What it reads |
| --- | --- |
| `closure-completeness` | Every Decision_Content element the dossier records has a settled entry, by its `settlementShape` |
| `closure-signatures` | The owner sets match in both directions, `signOffDate` is present, and the commit trailer agrees |
| `closure-entry-commit` | `entryCommit` equals the git add-commit of this file |
| `signoff-verdicts` | Every Sign_Off_Condition the dossier records carries a verdict and a named reviewer |
| `gate-status-derivation` | The Gate reads `CLOSED` exactly when every part of it is accepted |
| `released-derivation` | Each screen's recorded `released` equals the derived value |
| `guard-registry` | Each guard the part could permit carries a verdict, so omission is a finding rather than a default |

The last one is the easiest to get wrong. Requirement 4 criterion 6 is read as totality: *for each*
guard the dossier maps to the part, the record states whether the acceptance permits it to change
value. A part accepted with no verdict for a guard mapped to it is rejected, and a part accepted
with a non-permitting verdict leaves the guard pinned — which is the state Requirement 6 criterion
17 requires after `DG-MIS` closes, where `kGcsPayloadStateReplayImplemented` stays at false and no
part permits it at all.

## An amendment settles one key

Requirement 8 criterion 5 lets a freshness class arrive after the rest of `DG-FRS` is signed, and
criterion 14 scopes what that does: the amendment's signatures and sign-off date attach to **that
key alone**, and every other key keeps its recorded acceptance. `closure-amendments` checks exactly
that, so an amendment cannot quietly re-open or re-sign what was already settled.
