---
name: gpac-patching
description: Review or write GPAC bug and security-fix patches and their reports. Use for PR merge-readiness reviews, root-cause and patch-scope analysis, and regression or reproducer validation.
---

# GPAC patching

Use this skill to write or review a bug or security patch and its report. Review whether the patch fixes the demonstrated root cause and preserves normal processing, cleanup, and ownership. Review the diff, tests, PR body, and packaged reproducer together; they must consistently describe and demonstrate the final fix.

## Follow the requested task

- **Review a patch:** record the exact PR head SHA, examine its evidence and implementation, and return the assessment described below. Leave the contribution unchanged; use an isolated checkout or worktree for applying and testing the candidate patch.
- **Write a fix:** reproduce the defect on unpatched code before editing, implement the smallest complete correction with a regression test, verify valid behavior, and describe the final change and actual results.
- **Prepare a report:** use the demonstrated effect, reproduction, cause, and available fix evidence. Identify missing information and validation; preparing a report does not require inventing or implementing a fix.

Follow the applicable guidance below for the requested task. Local review, patching, and report preparation do not by themselves authorize a push, publication, PR comment, or merge.

## Define the defect and the patch's claims

Distinguish the original failing access or observable effect, additional defects reached by the same input, and the broader behavior or lifetime guarantees claimed by the patch. Assess root-cause coverage against that explicit scope. Identify whether each concern is introduced by the patch, an incomplete fix, or a separate residual defect. A smaller patch for one failing access must not be presented as completing broader lifetime repair; a related failure alone does not prove that the exact reported defect remains unfixed.

## Establish what the patch fixes

- Before patching or reviewing an existing PR, reproduce the failure on unmodified upstream. Record the full SHA, platform/compiler/build, exact command, exit status, observed effect, and expected result. Confirm it still reproduces on current `master` before reporting; GPAC only patches reports reproducible there. If the requested review excludes runtime testing or reproduction is unavailable, proceed with static analysis and document the evidence limits; do not claim reproduction, fix effectiveness, or full validation.
- Start with a real command-line reproduction using concrete media, a manifest, or a client request. Prefer the shipped CLI; if a path needs a downstream application, demonstrate it in that actual application rather than a stubbed harness. Use the built `bin/gcc/MP4Box` or `bin/gcc/gpac` so system-installed GPAC cannot mask the result. An API-level check may supplement the CLI reproduction when persistent state is otherwise invisible.
- For memory defects, use ASan to establish an actual memory-safety violation through the CLI and assess its security consequence. Code execution or a controlled primitive is not a prerequisite to a useful fix. A static suspicion, sanitizer label, or TSan warning alone does not establish security impact; retain TSan-only observations as correctness evidence until consequences are shown.
- For logic flaws, demonstrate the forbidden file access, credential disclosure, authorization failure, or other effect. For resource exhaustion, measure input size, CPU or memory cost, valid controls, and job or service impact. These need no ASan output. Deprioritize arithmetic warnings without consequences; overflow causing unsafe allocation or memory access remains relevant. GPAC's policy excludes corner cases without security concerns, including isolated overflow reports without a crash.
- Search open and closed issues, PRs, and existing fixes for the same defect. Search distinctive functions, input shapes, errors, and root causes; a shared component alone is not a duplicate. Read plausible matches and distinguish the root cause. Check alternative patches for the same defect before starting another implementation.

## Check root-cause coverage and patch scope

- **Keep the patch focused.** Fix the demonstrated root cause and preserve valid behavior. Compare checks in existing shared helpers or at input validation boundaries before adding repeated local guards. Prefer the simplest complete fix; defer a shared fix only for concrete refactoring, compatibility, or review concerns, not merely because it affects multiple callers.
- **Establish the safety invariant.** Before consolidating checks or removing guards, trace validation through every affected consumer and state transition, including reconfiguration. Distinguish allocated capacity, declared count, and validated records; use focused tests to demonstrate that the invariant holds.
- **Check ownership and failure paths.** Evaluate root cause, ownership, error cleanup, state restoration, and behavior on valid input. Distinguish residual related bugs from failure to fix the exact reported defect.
- **Justify every production change.** Review the diff file by file: every production change should be needed to close the reported defect or preserve valid behavior. Before adding a helper move, public input policy, or change in another component, establish why the reported fix needs it. Avoid unrelated refactors, formatting churn, new dependencies, or changes to public interfaces without user approval. Follow local C conventions for `GF_Err`, `GF_LOG`, and `gf_*` allocation APIs.
- **Document changed behavior.** Reconcile the report with the final production diff: explain every substantive behavior change and its rationale, including changed bounds and what the compared fields represent. If the fix defines an input policy, test its distinct rejection cases and document the rule at the affected public API or CLI surface. Feature disabling, legacy runtime replacement, and queue architecture changes require a separate decision.
- **Cite standards-derived constants.** Explain their meaning, identify the standard and relevant edition or clause, and link its authoritative source in the PR description. Distinguish format requirements from implementation limits.
- **Justify follow-up work.** For each optional item, state what remains, why it does not replace or complete this fix, and the distinct behavior or validation it needs. Use "follow-up work" for separate focused fixes; reserve "long-term" for demonstrated substantial refactoring, compatibility changes, or redesign. Do not defer a simpler complete fix or required root-cause coverage.

## Prove the regression test and valid behavior

- Run the same test against unpatched and patched code. Confirm the original fails for the intended reason and the patch passes. Include meaningful valid controls. The test should exercise the report's scenario and observable effect, with safe-input and caller-controlled controls where relevant.
- Prove the claimed behavior in tests and reproduction scripts, not merely a zero exit status: check that controls emit useful output and that a trigger fails when neither the reported fault nor the expected rejection occurs. In [#3892](https://github.com/gpac/gpac/pull/3892#issuecomment-5559183251), the maintainer found that the proposed unit test still passed with most of the patch reverted.
- Make setup, trigger, and expected behavior obvious. Prefer named fields and simple structured fixtures; explain unusual values and malformed inputs briefly.
- Add a regression test in the relevant `src/*/unittests/` directory or `testsuite/` script. Keep the full end-to-end reproduction package outside the GPAC patch; the committed test must run without that package. Do not add binary trigger media to the GPAC patch merely to supply the report's sample.
- Run the original trigger, valid and boundary controls, relevant unit tests, and the affected functional tests. Record missing regression coverage. Use a deployment or integration scenario when it adds concrete coverage.
- When a PR revision changes the patch or tests, rerun the affected regression and valid controls, and update the PR body with actual results. Check that the packaged reproducer and reported results match this revision. Do not describe tests or deployment coverage that were not run.

## Give concurrency fixes additional scrutiny

Each logical patch needs its synchronization or lifetime invariant, dependencies on other patches, before/after CLI evidence, and remaining limitations. Exercise valid input and failing teardown repeatedly with both default and mutex schedulers; use separate ASan and TSan builds, check completion and output as well as diagnostics, and measure throughput for affected workloads. Obtain relevant non-x86 execution evidence before claiming portability. If that hardware or review is unavailable, retain the limitation explicitly rather than calling the series fully validated.

Use repeated runs for races and distinguish observed successes from proof of synchronization correctness. A disappearing crash in bounded race tests does not prove the race fixed. Give race fixes extra runtime checks and a second opinion, since maintainers deferred them for more testing and review in [#3870](https://github.com/gpac/gpac/pull/3870#issuecomment-5427891933).

When concrete merge blockers are established, report them promptly and identify the remaining validation. Checks that were unavailable or not run remain coverage limits; complete synchronization, performance, or portability claims require the corresponding evidence. A useful assessment of specific blockers does not require completing every concurrency check first.

## Write a report that supports the fix

Use a title that names the consequence and entry point. Keep the PR body short and self-contained. Use these Markdown section headings in this order so a maintainer can find the impact, reproduction, and fix quickly:

- **`## Summary and Impact`:** attacker-controlled field, entry point, required user or service action, writable target or other relevant permissions, and observed security consequence. Briefly identify the root cause and affected function. Rate severity from that evidence and mention an existing safe CLI option when it changes exposure. Do not imply code execution or broader control without proof.
- **`## Reproduction`:** the shortest runnable end-to-end commands, reproducer ZIP link, current upstream SHA, expected result, actual result, and a valid control. State platform/compiler details when they affect reproduction or limit the result. Include sanitizer and feature settings when they affect reproduction or the security assessment. Put the collapsible sanitizer excerpt here. Keep incidental host-specific build workarounds in local evidence rather than the PR body.
- **`## Fix and Verification`:** function and source location, why the trust boundary fails, and the precise behavior change. State important residual behavior and compatibility effects. For example, rejecting path syntax in implicit names is not a guarantee that accepted names cannot overwrite an existing basename or follow a symlink. Report tests and counts actually run on the current commit, distinguishing runtime observations from static review and stating platform limits.

End with this disclosure when applicable; adjust it to reflect the assistance and validation actually performed. Do not claim human review before it happens. Preserve contributor attribution when extending a PR.

> AI assistance was used to investigate the issue, prepare the patch and tests, and draft this report. The commands and results above were run locally.

Keep the main report focused on executable evidence a maintainer can act on. Mention relevant prior issues or maintainer scope decisions briefly and identify the specific difference between the cases. The PR title already identifies the finding; start the body with the report, without repeating the title.

When a sanitizer establishes the defect, add a brief HTML `<details>` block with a real, trimmed output excerpt: the error class, read/write size or signal, and the first relevant GPAC frames. Keep complete sanitizer logs with the local evidence. Do not make maintainers open a ZIP just to learn what diagnostic to expect.

Use the reporting requirements in GPAC's [issue template](https://github.com/gpac/gpac/blob/master/.github/ISSUE_TEMPLATE/bug_report.md) and [security policy](https://github.com/gpac/gpac/blob/master/SECURITY.md). Put the report in the patch PR when a fix is ready; a separate issue is unnecessary.

## Make the reproducer usable by the reviewer

Put the trigger, valid control, reproduction scripts, a brief `README.md` with exact commands, and a deterministic generator when useful in one ZIP per report. Separate run scripts from input media in the ZIP. Give its local SHA-256 when bytes matter. GPAC's `SECURITY.md` asks for a sample file but does not require the sample to be committed to GPAC. Make the reproducer ZIP accessible to reviewers.

Give both a one-command automated reproduction and the equivalent target CLI command in the PR body and ZIP README. If a script starts a local server, provide a server-only mode that prints its URL and leaves it running while the maintainer invokes GPAC manually or under a debugger. Show valid-control commands or the exact input substitution as well.

## Return a concrete review assessment

Keep one concise assessment with the evidence: ready for merging, needs a specific correction, incomplete for the reported root cause, or unable to reproduce. Include:

- The reviewed PR head SHA and upstream SHA, commands and tests actually run, observed results, and material coverage limitations.
- Each merge-blocking concern, with the relevant source location, failed invariant or missing evidence, and the specific correction needed.
- Any separate, justified follow-up work, distinguishing residual related bugs from failure to fix the exact reported defect.

Check edge cases, regressions, test quality, readability, whether every production change is necessary, and whether the artifacts consistently describe and demonstrate the final fix. When a reviewer finds confusing code or missing evidence, the contributor must resolve the concern in the patch or test and rerun affected checks before the patch is considered ready for merging. A review may finish with a correction verdict and specific requests without editing the contribution. Static review or unavailable runtime evidence must be identified explicitly in the assessment.
