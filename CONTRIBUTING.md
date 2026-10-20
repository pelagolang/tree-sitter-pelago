# Contributing

Part of the [Pelago](https://github.com/pelagolang/pelago) project. The full guide to commits and pull requests is
[`docs/contributing/commits.md`](https://github.com/pelagolang/pelago/blob/main/docs/contributing/commits.md) in the main repository; the short version follows.

## Commits and pull request titles

Every commit subject and every pull request title uses [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/):

```
type(scope): summary
```

- Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `chore`, `ci`, `build`, `revert`.
- `scope` is optional (for example `grammar`, `queries`, `highlights`, `tests`).
- The summary is imperative, starts with a lowercase letter, has no trailing period, and keeps the first line under 72 characters.
- A breaking change gets `!` after the type or scope (`feat(grammar)!: ...`) or a `BREAKING CHANGE:` footer.
- Pull requests are squash-merged, so the PR title becomes the commit on `main`.

Examples:

```
feat(grammar): parse the new comment forms
fix(queries): highlight soft keywords only where they are keywords
test: add corpus cases for for-range
ci: lint pull request titles
```

CI (`.github/workflows/conventional-commits.yml`) lints the PR title and every non-merge commit subject.

## Branches

`<author>/<type>/<short-name>`, for example `dennis/feat/for-range` or `claude/fix/string-literal`. The type is one of `feat`, `fix` (or `bug`), `perf`, `refactor`, `docs`, `test`, `chore`, `ci`, `build`.
