# Contributing Guide
This repository follows an **issue-driven development workflow** with staged environments to avoid conflicts and ensure code quality, traceability, and collaboration.

Please review this guide in its entirety before making a contribution.

---

## Table of Contents
- [Development Process](#development-process)
- [Branching Strategy](#branching-strategy)
- [Issue Workflow](#issue-workflow)
- [Commit Message Guidelines](#commit-message-guidelines)
- [Pull Request Process](#pull-request-process)
- [Code Standards](#code-standards)
- [Testing Requirements](#testing-requirements)
- [Review & Merge Policy](#review--merge-policy)
- [Releasing a New Board Profile](#board-profile)

---

## Development Process
### Planning & Communication (Before Coding)
| Step | Action |
|---|---|
| **1.1** | TODO
| **1.2** | 
| **1.3** | See [README.md](README.md) for setup instructions and environment configuration.

### Technical Workflow (During Coding)
| Step | Action |
|---|---|
| **2.1** | Select or create an Issue, following the [Issue Workflow](#issue-workflow)
| **2.2** | Assign yourself as the Issue owner
| **2.3** | Create a feature branch from `dev` (see [Branching Strategy](#branching-strategy)) 
| **2.4** | Make **frequent pull requests** (during each work cycle) to avoid merge conflicts
| **2.5** | Document your code with detailed comments so that other contributors can understand the logic behind your implementation
| **2.6** | Commit changes in small logical parts, following the [Commit Message Guidelines](#commit-message-guidelines)
| **2.7** | Finally, submit a pull request to the **`dev` branch only**, following the [Pull Request Process](#pull-request-process) (features should only be pushed to `main` when releasing a new board profile)
> ⚠️   Each Pull Request **must reference an Issue**

> ⚠️   Feature branches should not be active for more than 2-3 days to avoid merge conflicts

### Code Review and Merging (After Coding)
| Step | Action |
|---|---|
| **3.1** | Follow the [Testing Requirements](#testing-requirements) and [Merge Policies](#review-&-merge-policy) before attempting a merge
| **3.2** | Communicate with the other OPEnS teams once the PR has been approved so that further testing may be done
| **3.3** | Once the dev branch has been tested and approved by Dr. Chet, create a new PR to `main` and begin the process to release a new board profile [Releasing a New Board Profile](#board-profile)

### After Merging
| Step | Action |
|---|---|
| **4.1** | Once feature branches have been sucessfuly merged to main (after dev testing and approval), feature branches can be safely deleted as per indusry standards (merge and PR history, as well as project/file version history will remain accessible after feature branch deletions) 
> Note: Branches can only be restored if associated with a PR and can be accessed through the PR's history/overview
---


## Branching Strategy

This project uses a staged branching model:

- `main`
  - Main branch
  - **Protected** (no direct commits) - PRs to main must be approved by Dr. Udell and/or Team Leads
  - All merges or changes to `main` will not be reflected in the deployed Arduino IDE board profile until a new board profile is released

- `dev`  
  - Integration and testing branch
  - All feature and bugfix work is **merged here first**

- `feature/*`  
  - Used for all new features or sensor integrations
  - Must be created from `dev`
    
- `bugfix/*`  
  - Used for all bug related work
  - Must be created from `dev`

### Branch Naming Convention
Examples:
- `feature/lte-location-metadata`
- `feature/veml6075-sensor-integration`
- `bugfix/lte-timezone`

---

## Issue Workflow

All work must be tracked using GitHub Issues.

### Open an Issue for:
- New features
- Bug fixes
- Design improvements
- Performance improvements
- Infrastructure or CI changes
- Testing

### Issue Requirements
| Bug Fixes, Infrastructure, and Testing | New Features and Design/Performance Improvements |
|---|---|
| Each issue should include:| Each issue should include:|
| <ul><li> Clear problem or new feature description</li></ul> | <ul><li> Clear problem or new feature description</li></ul>  |
| <ul><li> Appropriate labels </li></ul> | <ul><li> Appropriate labels </li></ul> | 
| <ul><li> Appropriate type </li></ul> | <ul><li> Appropriate type </li></ul> |
| <ul><li> Assigned milestone </li></ul> | <ul><li> Assigned milestone </li></ul> |
| <ul><li> Assigned owner </li></ul> | <ul><li> Assigned owner </li></ul>| 
| <ul><li> Screenshots or logs if applicable </li></ul> | <ul><li> Screenshots or logs if applicable </li></ul> |
| <ul><li> Expected behavior </li></ul> | |
| <ul><li> Actual behavior </li></ul>| |
| <ul><li> Steps to reproduce bug </li></ul>| |
| <ul><li> Environment (where or how bug was found) </li></ul> | |

Issues serve as the **source of truth** for all work.


### Closing Issues
You can close an issue by linking it to a Pull Request. Use GitHub’s supported keywords in your PR description:
- Keywords: Closes #123, Fixes #123, or Resolves #123
- This **automatically closes the issue** once the PR is merged and creates a permanent link between the fix and the issue

### Manual Closures
If you are closing an issue manually (without a PR), please provide a final comment that includes:
- Summary: A brief explanation of the resolution
- Reference: Links to any relevant commits, external documentation, or related discussions
- Reasoning: If closing as "Won't Fix" or "Duplicate," clearly state the reason

### Stale Issues
If an issue has no activity for more than six months, it will be labeled as 'stale' or will be closed as "Unplanned".

### Duplicate Issues
If you would like to create a new issue but there is a an older, less descriptive version:
- Create an improved, more descriptive issue
- Navigate to the older issue and choose 'Close as duplicate'
- Choose your new issue from the dropdown as the replacement issue

---


## Commit Message Guidelines
Commit messages must be clear and descriptive.

### Preferred Format

Examples:
- `Change timezone from local to UTC`
- `Remove duplicate reattachInterrupt calls`
- `VEML6075 measure and initialization methods implementation`

Avoid vague messages such as:
- `fix`
- `changes`
- `wip`

---

## Pull Request Process
All changes must be submitted via Pull Request.

### Pull Request Requirements
- Reference the related Issue (e.g., `Closes #42`)
- Clearly describe:
  - What was changed
  - Why the change was needed
  - How the change was implemented
- Include testing steps or screenshots if applicable

### Ownership
Each PR should have:
- One primary contributor
- Optional co-contributors noted in the description
> Note: This is a key factor in your resume-aware workflow, as PR's give you a chance to show off your skills to future employers. Keep things concise but descriptive in your contribution reasoning and documentation.

### Merging Process
On approval, and after satisfying all testing and documentation requirements, the commits must be squashed and the commit message edited to include the following:
- The commit message must begin with the accompanying issue identifier
- A unique description specifying the feature or bugfix implemented with clear keywords included

Examples:
- `#258 - OPEnS_RTC to Adafruit RTClib replacement`
- `#193 - NetworkTimeUpdate incorrect timezone bugfix`
---

## Releasing a New Board Profile
- TODO

## Code Standards
- Refer to the [C Style Guide](https://github.com/OPEnSLab-OSU/Loom-V4/wiki/C-Style-Guide#style-guide-for-embedded-programming) found in the Wiki
- Follow existing project structure and conventions
- Keep changes focused and scoped to the Issue
- Avoid unrelated refactors in the same PR
- Use consistent naming and formatting

---

## Testing Requirements
- All existing tests must pass
- New features should include tests when applicable
- Manual testing steps should be documented in the PR

No PR will be merged if it breaks existing functionality.

---

## Review & Merge Policy
- All PRs require at least one reviewer approval
- CI checks must pass
- No direct commits to `main` or `dev`
- `dev` is merged into `main` only after validation

---

## Questions or Help

For questions related to workflow or contribution guidelines, refer to the team [Wiki](https://github.com/OPEnSLab-OSU/Loom-V4/wiki) or contact the project maintainers.
