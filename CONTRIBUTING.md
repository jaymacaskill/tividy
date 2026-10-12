# Contributing to TIVIDY

This is how our team works on this repo. `main` is protected, so **nobody pushes to it directly**. Everything goes through a branch and a pull request (PR).

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
- Check that the code builds, the design matches our diagrams, and the checklist in the PR is honest.
- Leave comments on specific lines. The author replies and the conversation is **resolved** once it's dealt with. All conversations must be resolved before merging.
- If the author pushes new commits, **old approvals are dismissed** and the PR needs to be re-reviewed.

## Automatic checks

These run on every PR into `main`. All three must pass before merging.

| Check | What it does |
|---|---|
| `build` | Runs `make` to confirm the project compiles |
| `check-branch-name` | Confirms the branch follows `prefix/description` |
| `check-title` | Confirms the PR title follows `type: description` |

A fourth workflow, **Label PR**, adds a label from your branch prefix. It's not a gate.

### When a check fails

A failed check doesn't remove your work. It just blocks the merge button.

- **`build` failed:** fix it locally, run `make clean && make`, then commit and push to the same branch. The checks re-run on their own.
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

## Project rules

- The language is **C++11**, and the code must compile with `make`.
- No large if/else chains, giant switch statements or manual type checks where polymorphism should be used.
- If your change affects the design, **update the diagrams** so they all agree with each other.
- Log important design decisions and changes in the design decisions and revisions section in `docs/`.
- Name the design pattern(s) and classes your PR touches in the PR template.
- Every member must understand the whole design, so ask if something in a PR is unclear.

## Before you open a PR

- [ ] Branch name follows `prefix/description`
- [ ] PR title follows `type: description`
- [ ] `make clean && make` works locally
- [ ] Branch is up to date with `main`
- [ ] I've read through my own diff
- [ ] Diagrams and `docs/` are updated if the design changed