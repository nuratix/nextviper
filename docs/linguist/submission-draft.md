# Linguist submission draft — blocked on adoption evidence

Reviewed the [official contribution policy](https://github.com/github-linguist/linguist/blob/main/CONTRIBUTING.md)
on 2026-10-31. The policy still requires 2,000 recently indexed non-fork files for
extensions normally used multiple times in a repository, with independent
repository/user distribution. This audit has not established that evidence.
Repository-local examples are not independent adoption. Do not submit this draft
as an eligible request or claim official recognition.

Proposed upstream work, once eligible:

- Insert [NextViper.yml](NextViper.yml) into `lib/linguist/languages.yml` in alphabetical order.
- Register this repository's `editors/vscode/syntaxes/nextviper.tmLanguage.json`
  through upstream `script/add-grammar`; then run `script/update-ids`.
- Consider `examples/todo-api/app.nv` as a real application sample, subject to
  maintainer review of representativeness and Apache-2.0 licensing. The new
  practical examples are teaching material and do not satisfy upstream's
  real-world sample requirement by themselves. Record original URLs and commit
  IDs for every sample. Do not relicense existing samples without authorization.
- Recheck extension conflicts against the current language database. Run
  upstream grammar compilation, classifier and test suites. None was run here.

PR draft fields to complete later:

- Description: Add NextViper, an experimental C++20-implemented programming language.
- Usage evidence: **MISSING** — supply a reviewed public code search and independent projects.
- Grammar: scope `source.nextviper`; source and license in this repository.
- Samples: **PENDING** maintainer selection and license verification.
- Tests: local metadata/keyword/operator checks pass; upstream checks **NOT RUN**.

The local Python grammar checks validate shared regex syntax and metadata only.
They do not execute TextMate/Oniguruma or Linguist's grammar compiler. Nested
comments, raw strings, triple-quoted strings and numeric patterns need fuller
editor tokenization coverage. No upstream repository was modified.
