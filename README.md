# autogit

A ~400-line C CLI that eliminates the frustration of:

```text
fatal: No configured push destination.
```

And its equally annoying cousin:

```text
ERROR: Repository not found.
```

**One command. One workflow. Zero unnecessary friction.**

`autogit` automatically configures your Git remote, stages changes, commits them, and pushes to GitHub. If the repository doesn't exist yet, it offers to create it for you.

---

## Why?

Pushing a fresh repository manually means remembering to:

1. `git remote add origin <url>`
2. `git add -A`
3. `git commit -m "..."`
4. `git push -u origin main`

Then there's remembering to use `-u` on the first push and creating the GitHub repository beforehand.

Forget a step, and Git complains.

`autogit` handles the entire workflow in one command, including writing a short project description to `.git/description`.

## Features

- **Automatic remote configuration** — Generates `git@<host>:<user>/<project>.git` using your global `~/.autogitrc` configuration.
- **`.env` support** — Allows each project to override the default remote using `GIT_REMOTE_URL`.
- **Smart project-name appending** — Converts a base URL like `git@github.com:you` into `git@github.com:you/<project>.git`.
- **Automatic GitHub repository creation** — If the repository doesn't exist, offers to create it using the GitHub CLI (`gh`) or REST API (`GITHUB_TOKEN`).
- **Safe commit messages** — Uses `git commit -F <tempfile>` to prevent quotes, `$`, and backticks from breaking shell execution.
- **Automatic upstream tracking** — Always pushes with `-u`, permanently eliminating the "No configured push destination" problem.
- **Windows-native support** — Builds and runs directly from Command Prompt (`cmd.exe`) or PowerShell.

## Build

### Requirements

- GCC
- MinGW-w64 on Windows
- Default `cc` compiler on Linux/macOS

### Using Make

```bash
make        # Build the executable
make run    # Build and execute
make test   # Build and display help
make clean  # Remove build/
```

Output:

- Linux/macOS: `build/autogit`
- Windows: `build\autogit.exe`

### Installation (POSIX)

```bash
sudo make install
```

Installs to `/usr/local/bin/autogit`.

### Windows installation

Copy `build\autogit.exe` to any directory included in your system `PATH`.

### Building without Make

On Windows, compile directly:

```bat
if not exist build mkdir build
gcc -Wall -Wextra -O2 -std=c11 -o build\autogit.exe autogit.c
```

## Usage

```bash
autogit [options]
```

### Options

| Flag | Description |
|---|---|
| `-u <url>` | Remote URL. Skips the prompt and rewrites an existing remote. |
| `-r <name>` | Remote name (default: `origin`). |
| `-b <branch>` | Branch to push (default: current branch). |
| `-d <desc>` | Short description used as the commit message and written to `.git/description`. |
| `-n` | Non-interactive mode. No prompts; uses defaults. |
| `-h` | Display help. |

## First Push of a New Project

```bash
cd ~/projects/my-cool-app
autogit
```

On the first run, `autogit` prompts for configuration and saves it to `~/.autogitrc`.

```text
First-time setup (saved to /home/you/.autogitrc)

Username (github handle): octocat
Host [github.com]:
Remote URL [git@github.com:octocat/my-cool-app.git]:
Short description: initial import of the CLI
```

Then automatically executes:

```bash
git remote add origin "git@github.com:octocat/my-cool-app.git"
git add -A
git commit -F "/tmp/autogit_1234_0.tmp"
git push -u origin main
```

## Everyday Push

From any configured Git repository:

```bash
autogit
```

Enter a short description when prompted:

```text
Short description: fix off-by-one in parser
```

Then `autogit` handles staging, committing, and pushing.

## Fully Non-Interactive

Perfect for scripts, automation, and CI/CD workflows:

```bash
autogit -n -d "nightly build" \
         -u git@github.com:octocat/my-cool-app.git
```

## Configuration

### `~/.autogitrc` — Global Configuration

Created once and reused across repositories.

```ini
user=octocat
host=github.com
```

This file is read during the first interactive run and determines the default remote URL:

```text
git@<host>:<user>/<project>.git
```

Delete the file if you want to be prompted for configuration again.

### `.env` — Per-Project Configuration

```env
# Base URL — autogit appends /<project>.git
GIT_REMOTE_URL=git@github.com:octocat

# Or provide a complete URL (used verbatim)
# GIT_REMOTE_URL=git@github.com:octocat/my-cool-app.git

# Optional: GitHub repository auto-creation
# GITHUB_TOKEN=ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
# GITHUB_VISIBILITY=private
```

The `.env` file only needs a public Git remote URL, so it can safely be committed and shared with collaborators.

If you ever store actual secrets in `.env`, ensure it is included in `.gitignore`.

### Remote URL Precedence

`autogit` resolves remote URLs in the following order:

1. `-u <url>` command-line flag (highest priority)
2. `.env` → `GIT_REMOTE_URL`
3. Interactive prompt (defaults to URL generated from `~/.autogitrc`)

### URL Resolution

URLs that don't end in `.git` automatically get `/<project>.git` appended.

| Input | Resolved URL |
|---|---|
| `git@github.com:octocat` | `git@github.com:octocat/my-cool-app.git` |
| `git@github.com:octocat/` | `git@github.com:octocat/my-cool-app.git` |
| `git@github.com:octocat/my-cool-app.git` | Unchanged |

## Automatic GitHub Repository Creation

If the remote points to GitHub and the repository doesn't exist yet, `autogit` offers to create it before pushing.

### Supported Backends

#### 1. GitHub CLI (`gh`) — Recommended

Install the GitHub CLI:

```bash
# Windows
winget install GitHub.cli

# macOS
brew install gh

# Debian/Ubuntu
sudo apt install gh
```

Authenticate once:

```bash
gh auth login
```

No additional token configuration is required.

#### 2. GitHub Personal Access Token

Alternatively, configure a classic Personal Access Token with the `repo` scope.

Generate one through:

GitHub → Settings → Developer settings → Personal access tokens → Tokens (classic) → Generate new token.

Add it to `.env`:

```env
GITHUB_TOKEN=ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

### Repository Visibility

Repositories default to **private**.

To create a public repository instead:

```env
GITHUB_VISIBILITY=public
```

Non-GitHub hosts such as GitLab, Bitbucket, and self-hosted Git servers are skipped automatically. `autogit` continues with the regular push workflow.

In non-interactive (`-n`) mode, repository creation proceeds without prompting.

## SSH Setup (One-Time)

For SSH remotes such as `git@github.com:...`, you need a working SSH key.

Generate one:

```bash
ssh-keygen -t ed25519 -C "you@example.com"
```

Add it to GitHub using the GitHub CLI:

```bash
gh ssh-key add ~/.ssh/id_ed25519.pub --title "my machine"
```

Test the connection:

```bash
ssh -T git@github.com
```

A successful connection should return something similar to:

```text
Hi <username>!
```

Alternatively, add your public key manually:

https://github.com/settings/keys

### HTTPS Authentication

For HTTPS remotes (`https://github.com/...`), use a Personal Access Token when Git prompts for a password, or configure Git Credential Manager:

```bash
git config --global credential.helper manager
```

## How It Works

Every execution follows this workflow:

1. Verifies that you're inside a Git working tree.
2. Gets the repository root and project name using `git rev-parse`.
3. Reads `.env`, `~/.autogitrc`, and the current branch.
4. If the remote is missing, resolves the URL using `-u`, `.env`, or the interactive prompt.
5. Adds the Git remote using `git remote add`.
6. If the remote is GitHub and the repository doesn't exist, offers to create it.
7. Stages all changes using `git add -A`.
8. Creates a commit using `git commit -F <tempfile>` (only when changes exist).
9. Pushes using `git push -u <remote> <branch>`.

The `-u` flag establishes upstream tracking, so subsequent pushes no longer require manual configuration.

## Files It Touches

| Path | Purpose |
|---|---|
| `~/.autogitrc` | Stores `user=...` and `host=...`; created once. |
| `.git/description` | Updated with the short description when a remote is added or `-d` is passed. |
| `<tmpdir>/autogit_*.tmp` | Temporary commit message file; deleted immediately after committing. |
| `.env` (repository root) | Read-only. Never modified. |

## Windows Notes

- Builds natively with `mingw32-make` and MinGW-w64 GCC. No WSL or Git Bash required.
- If a file named `NUL` appears in your project root, a POSIX shell may be interpreting `>NUL` incorrectly.
- The Makefile forces `SHELL := cmd.exe`, and the binary detects MSYS GCC via `__MSYS__` and switches to `/dev/null` automatically. Both fixes are included in the current version.
- Remove a stray `NUL` file using:



MIT License. See [LICENSE](LICENSE).
