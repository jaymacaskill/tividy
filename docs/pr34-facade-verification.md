# PR #34: facade startup verification

Prepared 1 November 2026. PR head: `3e79e7c9b1b34562c3d0ca6fea488552012df32f`.
Integrated main: `1c89341a75e0bbf2e692394d375ad43fcb9a2731`.

## Scope and coverage ownership

Keval requested runnable installPack/startWorkflow coverage for successful
installation/startup, duplicate keys, unknown workflows and instance isolation.
No existing main test calls either method. PR #75 proposes an unknown-workflow
case but no successful installation/startup, duplicate-key or instance-isolation
case. Its fixture cases use TEST rather than TEST_F and currently do not compile.

`test_facade_startup.cpp` supplies six focused cases:

- Registration of workflow/profile and rejection of a duplicate pack before its
  factories run; startup additionally checks participant/connector registration.
- Null-pack rejection without registration.
- Failed director build leaves the key and collaborators available for retry.
- Startup uses the registered definition/version/data, records and starts the
  instance, and attaches the real NotificationObserver to the instance channel.
- Unknown workflow neither records an instance nor consumes its generated id.
- Two startup requests retain separate instances/roots/data in creation order;
  mutation of the first run leaves the second run and caller data unchanged.

The unknown-workflow test adapts and strengthens #75's proposed case. After #34
lands, #75 should remove `StartingUnknownWorkflowThrows` and retain its remaining
facade/time/action coverage. None of #75's unrelated tests or missing production
implementations has been copied here. No other PR or review was changed.

## Isolation and limitations

The default Makefile target uses explicit object linkage, as existing builder and
validation tests do. `support/FacadeStartupFixtures.h` provides valid constructed
link-time substitutes only for unpublished library/prototype/runtime/context and
command-factory dependencies. Production builder/director, validator, StageItem,
services, events, profiles and notification observer are linked directly.

These are facade orchestration tests. Their default pass does not prove the
substitute collaborators' real behaviour, workflow transitions, banking/telecom,
undo or simulated deadlines. No raw storage/private-access macro is used. Do not
link the substitutes into libtividy.a or another executable. The test has a
TIVIDY_FACADE_REAL_COLLABORATORS guard so the same cases can also be compiled with
real dependency implementations, without the substitute header.

A separate temporary integration harness tested all six cases against the fetched
real runtime/state/context/library/prototype implementations. Only the unused
CommandFactory constructor was substituted. No dependency implementation was
added to this PR. The harness also uses a deliberately inert test WorkItem rather
than claiming to exercise production TaskItem transitions.

## Integration and results

The branch trailed main by 25 commits (including merge commits). Changes concern
StageItem, industry defaults, adapter/chain tests, documentation file headers,
and devcontainer tooling. `git merge --no-commit --no-ff origin/main` completed
without conflicts. No manual conflict resolutions or facade behaviour changes
were necessary. Installation documentation now describes the existing build-first
order and the limited rollback guarantee accurately.

- Native C++11 application build: passed.
- Native make run: passed; the unchanged main entry point exits immediately.
- Full make test: 327 cases passed across 22 executables, including six new cases.
- Focused startup suite under AddressSanitizer + UndefinedBehaviorSanitizer: 6/6
  passed, no reported errors.
- Temporary real-collaborator integration harness: 6/6 passed. It reports an
  existing state-integration include-guard typo warning, outside this PR.
- Three mutation probes (duplicate guard/start call/observer attachment removed):
  each was rejected by the focused tests. Production source was not mutated.
- PR delta whitespace check against integrated main: passed. A check of the whole
  merge against the old PR head also reports whitespace already present in main;
  unrelated main files were deliberately left unchanged.
- Docker checks unavailable: daemon not running. Valgrind unavailable.
- GitHub checks were not triggered or rerun; their result on the prepared update
  remains to be established after publication.

GoogleTest 1.12.1 (commit 58d77fa8070e8cec2dc1ed015d66b454c8d78850) was built in
/tmp for C++11/arm64. No machine or repository dependency configuration was changed.

## Normal CI command

With the repository's Docker environment available, run `make docker-test` and
`make docker-make`. Native environments with GoogleTest available run `make test`
and `make`. The test is included automatically in the existing make test target.

## Publication gate

Ready for renewed review of the scoped facade installation/startup change, not a
claim that the whole application is ready. Publish the main integration and test
changes together once, then obtain renewed approvals/checks on that final head.
Do not request repeated approvals on intermediate updates. Four approvals and all
other active main-protection requirements remain binding.
