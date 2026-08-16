# Continuous Integration (CI) Workflow Guide

This guide details recommended Continuous Integration pipelines for NextViper projects.

---

## 1. Official GitHub Actions Workflow

Create `.github/workflows/ci.yml`:

```yaml
name: NextViper CI Pipeline

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Source
        uses: actions/checkout@v4

      - name: Install NextViper Toolchain
        run: |
          curl -fsSL https://nextviper.nuratix.com/install.sh | bash
          echo "$HOME/.nextviper/bin" >> $GITHUB_PATH

      - name: Inspect Toolchain Environment
        run: nextviper doctor

      - name: Static Type Validation
        run: nextviper check

      - name: Formatting Compliance
        run: nextviper fmt --check

      - name: Static Code Analysis (Linter)
        run: nextviper lint

      - name: Execute Automated Test Suite
        run: nextviper test

      - name: Compile Native Production Binary
        run: nextviper build --native --release
```

---

## 2. GitLab CI / Bitbucket Pipelines

```yaml
# gitlab-ci.yml
image: ubuntu:22.04

stages:
  - test
  - build

test_pipeline:
  stage: test
  script:
    - nextviper check
    - nextviper fmt --check
    - nextviper lint
    - nextviper test

build_binary:
  stage: build
  script:
    - nextviper build --native --release
  artifacts:
    paths:
      - build/bin/
```
