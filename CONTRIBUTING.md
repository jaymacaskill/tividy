# Contributing to TIVIDY

This is how our team works on this repo. `main` is protected, so **nobody pushes to it directly**. Everything goes through a branch and a pull request (PR).

**New here?** Read [the README](README.md) first, then [how we planned the build](docs/IMPLEMENTATION_PLAN.md) to see who owns what and which order the files are built in.

## The workflow

1. Get the latest `main` and make a branch from it.
2. Do your work and commit small and often.
3. Push your branch and open a PR into `main`.
4. Fill in the PR template.
5. Get **4 approvals** from teammates (not yourself).
6. Fix any failing checks and resolve all review comments.
7. Merge. The branch is deleted automatically.

```bash
git checkout main
git pull
git checkout -b feature/work-item-state

# ...work, then...
git add .
git commit -m "feat: add WorkItemState and CreatedState"
git push -u origin feature/work-item-state
```

Then open GitHub and click **Compare & pull request**.

## Branch names

Use `prefix/short-description`. Prefixes are **lowercase**, and the description may only contain letters, numbers, `.`, `_` and `-`.

| Prefix | Use for | Example |
|---|---|---|
| `feature/` | New functionality | `feature/work-item-state` |
| `fix/` | Bug fixes | `fix/clone-shallow-copy` |
| `docs/` | Documentation, diagrams | `docs/class-diagram-v2` |
| `refactor/` | Restructuring, no behaviour change | `refactor/strategy-interfaces` |
| `test/` | Tests | `test/decorator-stacking` |
| `chore/` | Makefile, config, tooling | `chore/update-makefile` |

The branch prefix also decides which label the PR gets automatically.

## Commit messages and PR titles

Both use the form `type: what changed`. The types match the branch prefixes, except `feature` is shortened to `feat`.

| Type | Matches branch |
|---|---|
| `feat` | `feature/` |
| `fix` | `fix/` |
| `docs` | `docs/` |
| `refactor` | `refactor/` |
| `test` | `test/` |
| `chore` | `chore/` |

Examples:

- `feat: add StageItem composite with unique_ptr children`
- `fix: deep copy children in StageItem::clone`
- `docs: update class diagram with Visitor`

Keep the first line short. If the *why* isn't obvious, add it in the body of the commit message.

**The PR title is checked automatically.** It must start with one of the types above, followed by a colon, a space and a description. The title is what ends up in the history, so make it say something useful.

## Merging: squash or merge commit?

Both are allowed. The person merging picks one on the merge button.

| Use | When | Why |
|---|---|---|
| **Squash** | The branch has many meaningless commits ("wip", "oops", "fix build") or is a small one-idea change | Everything collapses into one clean commit using the PR title |
| **Merge commit** | The branch has many meaningful commits that tell a story and are worth keeping in the history | Every commit is kept, and the branching shows in the graph |

Merge commits start with "Merge pull request #12 from...", which says nothing useful. **When you use a merge commit, edit the subject line** in the box GitHub shows before you confirm, for example `feat: add WorkItem state hierarchy (#12)`.

## Reviewing

- 4 approvals are needed. You can't approve your own PR, so up to 6 teammates can review each one.
- Review PRs promptly. Four approvals means a few slow reviewers can block everyone.
- **Actually read and run the code.** Pressing approve without looking defeats the point of four reviewers. Check out the branch, run `make test`, and read the diff.
- Check that the code builds, the tests cover what changed, the design matches our diagrams, and the checklist in the PR is honest.
- Leave comments on specific lines. The author replies and the conversation is **resolved** once it's dealt with. All conversations must be resolved before merging.
- If the author pushes new commits, **old approvals are dismissed** and the PR needs to be re-reviewed.

## Automatic checks

These run on every PR into `main`. All three must pass before merging.

| Check | What it does |
|---|---|
| `build` (workflow **CI**) | Builds the Docker image, then runs `make test` inside it: the project must compile and **every test must pass** |
| `check-branch-name` | Confirms the branch follows `prefix/description` |
| `check-title` | Confirms the PR title follows `type: description` |

A fourth workflow, **Label PR**, adds a label from your branch prefix. It's not a gate.

### When a check fails

A failed check doesn't remove your work. It just blocks the merge button.

- **`build` failed:** reproduce it locally with `make clean && make test` (or `make docker-test` for the exact CI environment), fix it, then commit and push to the same branch. The checks re-run on their own. A failing test anywhere blocks every PR, so fix red tests quickly.
- **`check-title` failed:** edit the PR title on GitHub. No new commit needed.
- **`check-branch-name` failed:** a branch can't be renamed under an open PR. Rename it (`git branch -m old-name new-name`), push the new name, delete the old one, and open a new PR.

## Keeping your branch up to date

If `main` moves ahead while you work:

```bash
git checkout main
git pull
git checkout feature/work-item-state
git merge main
git push
```

If only you are working on the branch, `git rebase main` followed by `git push --force-with-lease` gives a tidier history. Don't rebase a branch someone else is also using, and remember that force pushing dismisses existing approvals.

## Tests

We use **GoogleTest**, and we write tests as we go: a feature or class should arrive with tests in the same PR.

- Tests live in `tests/`. `make test` builds every `tests/**/*.cpp` as its own binary linked against `libtividy.a`, runs them all, and fails if any fail.
- **Folder tests** (`tests/test_engine_<folder>.cpp`) check edge cases and illegal use: one happy path per class, and one illegal operation that must throw.
- **Pattern tests** (`tests/test_pattern_<pattern>.cpp`) check that a pattern's required behaviour is met. The pattern's owner writes them and is responsible for them passing.
- **Industry tests** (`tests/test_industry_<pack>.cpp`) run a pack end to end through `Tividy`.
- Only push a test once every file it needs is already on `main`; otherwise CI is red. Write it locally earlier if you like.
- Tests should check workflow behaviour: transitions, routing, instance isolation and invalid operations.

## Working in phases

Work was planned in dependency-ordered phases (see [the implementation plan](docs/IMPLEMENTATION_PLAN.md)):

- Each `.cpp` has one author, and a header is shared by at most two people. The pair agree up front who writes which functions.
- Files that call each other in a circle go on a shared **integration branch** (`feature/p<phase>-integration-<name>`). Helpers open PRs into it; the lead opens the single PR into `main`. Those inner PRs run no CI, so build locally first.
- Every `.cpp` must compile on its own, because `make` builds all of `src/` into the library.

## Code style

Follow what the existing files already do:

- Start every file with the team header comment (names, student numbers, course, last modified).
- Tabs for indentation, braces on their own line, include guards named `TIVIDY_<PATH>_<FILE>_H`.
- `PascalCase` for classes, `camelCase` for functions, `snake_case` for variables and struct fields.
- C++11 only: no `std::make_unique`, `std::optional` or other later features.
- Document every class and non-obvious function with Doxygen (`/** ... */`). Run `make doxygen` and fix new warnings.
- Engine and UI code must stay industry-neutral: this must always print nothing:
  `grep -riE "banking|dispute|chargeback|telecom|msisdn" include/engine src/engine include/ui src/ui`

## Project rules

- The language is **C++11**, and the code must compile with `make`.
- No large if/else chains, giant switch statements or manual type checks where polymorphism should be used.
- If your change affects the design, **update the diagrams** so they all agree with each other.
- Log important design decisions and changes in the Design Decisions and Revisions section of the report.
- Name the design pattern(s) and classes your PR touches in the PR template.
- Every member must understand the whole design, so ask if something in a PR is unclear.

## Before you open a PR

- [ ] Branch name follows `prefix/description`
- [ ] PR title follows `type: description`
- [ ] `make clean && make test` passes locally
- [ ] New or changed code has tests
- [ ] New or changed code has Doxygen comments
- [ ] Engine neutrality `grep` prints nothing
- [ ] Branch is up to date with `main`
- [ ] I've read through my own diff
- [ ] Diagrams and `docs/` are updated if the design changed