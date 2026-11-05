/* ── Platform layer ───────────────────────────────────────────────── */
#if defined(__MSYS__)
#  include <unistd.h>
#  include <sys/stat.h>
#  define AG_GETPID  getpid
#  define R_NULL     " > /dev/null 2>&1"
#  define R_ERR      " 2>/dev/null"
#  define PATHSEP    "/"
#elif defined(_WIN32)
#  include <process.h>
#  define AG_GETPID  _getpid
#  define R_NULL     " >NUL 2>&1"
#  define R_ERR      " 2>NUL"
#  define PATHSEP    "\\"
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  define AG_GETPID  getpid
#  define R_NULL     " > /dev/null 2>&1"
#  define R_ERR      " 2>/dev/null"
#  define PATHSEP    "/"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AG_CMD   4096
#define AG_PATH  1024
#define AG_LINE  512

/* ── Command helpers ──────────────────────────────────────────────── */
static int run(const char *cmd) {
    printf("$ %s\n", cmd);
    fflush(stdout);
    return system(cmd) == 0 ? 0 : 1;
}

static int remote_exists(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "git remote get-url %s%s", name, R_NULL);
    return system(cmd) == 0;
}

static int has_changes(void) {
    FILE *fp = popen("git status --porcelain" R_ERR, "r");
    if (!fp) return 0;
    int ch = fgetc(fp);
    pclose(fp);
    return ch != EOF;
}

/* ── Portable temp file (moved up — needed by github_create_repo) ── */
static int make_temp(char *out, size_t outsz) {
    const char *dir = getenv("TMPDIR");
#if defined(_WIN32) && !defined(__MSYS__)
    if (!dir) dir = getenv("TEMP");
    if (!dir) dir = getenv("TMP");
#endif
    if (!dir || !dir[0]) {
#if defined(_WIN32) && !defined(__MSYS__)
        dir = ".";
#else
        dir = "/tmp";
#endif
    }
    for (int i = 0; i < 1000; i++) {
        snprintf(out, outsz, "%s%sautogit_%d_%d.tmp",
                 dir, PATHSEP, (int)AG_GETPID(), i);
        FILE *f = fopen(out, "r");
        if (f) { fclose(f); continue; }
        return 0;
    }
    return -1;
}

/* ── Prompt / input ───────────────────────────────────────────────── */
static int prompt(const char *label, char *buf, size_t bufsz) {
    printf("%s", label);
    fflush(stdout);
    if (!fgets(buf, (int)bufsz, stdin)) return 0;
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' '))
        buf[--n] = 0;
    return 1;
}

/* ── ~/.autogitrc ─────────────────────────────────────────────────── */
static const char *config_path(void) {
    static char path[AG_PATH];
    const char *home = getenv("HOME");
#if defined(_WIN32) && !defined(__MSYS__)
    if (!home) home = getenv("USERPROFILE");
#endif
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s%s.autogitrc", home, PATHSEP);
    return path;
}

static void load_config(char *user, size_t u, char *host, size_t h) {
    user[0] = host[0] = 0;
    FILE *fp = fopen(config_path(), "r");
    if (!fp) return;
    char line[AG_LINE];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if      (!strncmp(line, "user=", 5)) snprintf(user, u, "%s", line + 5);
        else if (!strncmp(line, "host=", 5)) snprintf(host, h, "%s", line + 5);
    }
    fclose(fp);
}

static void save_config(const char *user, const char *host) {
    FILE *fp = fopen(config_path(), "w");
    if (!fp) { perror("cannot write .autogitrc"); return; }
    fprintf(fp, "user=%s\nhost=%s\n", user, host);
    fclose(fp);
#ifndef _WIN32
    chmod(config_path(), 0600);
#endif
}

/* ── .env loader ──────────────────────────────────────────────────── */
static int load_env(const char *root, const char *key, char *out, size_t outsz) {
    char path[AG_PATH];
    snprintf(path, sizeof(path), "%s%s.env", root, PATHSEP);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[AG_LINE];
    size_t klen = strlen(key);
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 || *p == '#') continue;
        if (strncmp(p, key, klen) != 0 || p[klen] != '=') continue;

        char *v = p + klen + 1;
        while (*v == ' ' || *v == '\t') v++;
        size_t vn = strlen(v);
        while (vn && (v[vn-1] == ' ' || v[vn-1] == '\t')) v[--vn] = 0;
        if (vn >= 2 && (v[0] == '"' || v[0] == '\'') && v[vn-1] == v[0]) {
            v[vn-1] = 0;
            v++;
        }
        snprintf(out, outsz, "%s", v);
        found = 1;
        break;
    }
    fclose(fp);
    return found;
}

/* ── URL finalization ─────────────────────────────────────────────── */
static void finalize_url(const char *base, const char *proj,
                         char *out, size_t outsz) {
    size_t n = strlen(base);
    if (n >= 4 && strcmp(base + n - 4, ".git") == 0) {
        snprintf(out, outsz, "%s", base);
        return;
    }
    while (n > 0 && (base[n-1] == '/' || base[n-1] == '\\')) n--;
    snprintf(out, outsz, "%.*s/%s.git", (int)n, base, proj);
}

/* ── Git URL parsing → host / owner / repo ───────────────────────── */
static int parse_git_url(const char *url, char *host, size_t hsz,
                         char *owner, size_t osz, char *repo, size_t rsz) {
    host[0] = owner[0] = repo[0] = 0;
    const char *p = url;

    if      (strncmp(p, "https://", 8) == 0) p += 8;
    else if (strncmp(p, "http://",  7) == 0) p += 7;
    else if (strncmp(p, "ssh://",   6) == 0) p += 6;

    const char *at    = strchr(p, '@');
    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (at && (!slash || at < slash) && (!colon || at < colon))
        p = at + 1;

    const char *sep = NULL;
    for (const char *q = p; *q; q++)
        if (*q == ':' || *q == '/') { sep = q; break; }
    if (!sep) return -1;

    size_t hlen = (size_t)(sep - p);
    if (hlen == 0 || hlen >= hsz) return -1;
    memcpy(host, p, hlen); host[hlen] = 0;

    p = sep + 1;
    while (*p == '/') p++;

    const char *owner_end = strchr(p, '/');
    if (!owner_end) return -1;
    size_t olen = (size_t)(owner_end - p);
    if (olen == 0 || olen >= osz) return -1;
    memcpy(owner, p, olen); owner[olen] = 0;

    p = owner_end + 1;
    const char *repo_end = p + strlen(p);
    if (repo_end - p > 4 && strcmp(repo_end - 4, ".git") == 0)
        repo_end -= 4;
    size_t rlen = (size_t)(repo_end - p);
    if (rlen == 0 || rlen >= rsz) return -1;
    memcpy(repo, p, rlen); repo[rlen] = 0;

    return 0;
}

/* ── GitHub repo existence + creation ─────────────────────────────── */
static int gh_available(void) {
    if (system("gh --version" R_NULL) != 0) return 0;
    if (system("gh auth status" R_NULL) != 0) return 0;
    return 1;
}

static int github_repo_exists(const char *owner, const char *repo) {
    if (gh_available()) {
        char q[512];
        snprintf(q, sizeof(q), "gh repo view %s/%s%s", owner, repo, R_NULL);
        return system(q) == 0 ? 1 : 0;
    }
    const char *tok = getenv("GITHUB_TOKEN");
    if (!tok || !tok[0]) return -1;

    char q[512];
    snprintf(q, sizeof(q),
        "curl -sf -H \"Authorization: token %s\" "
        "https://api.github.com/repos/%s/%s%s",
        tok, owner, repo, R_NULL);
    return system(q) == 0 ? 1 : 0;
}

static int github_is_org(const char *owner, const char *token) {
    char q[512];
    snprintf(q, sizeof(q),
        "curl -sf -H \"Authorization: token %s\" "
        "https://api.github.com/orgs/%s%s",
        token, owner, R_NULL);
    return system(q) == 0 ? 1 : 0;
}

static int github_create_repo(const char *owner, const char *repo,
                              const char *visibility, int noninteractive) {
    int priv = !visibility || visibility[0] == 0 || strcmp(visibility, "public") != 0;

    /* Preferred: gh CLI (no token needed) */
    if (gh_available()) {
        char q[512];
        if (noninteractive) {
            snprintf(q, sizeof(q),
                "echo y | gh repo create %s/%s --%s%s",
                owner, repo, priv ? "private" : "public", R_NULL);
        } else {
            snprintf(q, sizeof(q),
                "gh repo create %s/%s --%s%s",
                owner, repo, priv ? "private" : "public", R_NULL);
        }
        if (run(q) == 0) return 0;
        /* fall through to API */
    }

    /* Fallback: REST API with GITHUB_TOKEN */
    const char *tok = getenv("GITHUB_TOKEN");
    if (!tok || !tok[0]) {
        fprintf(stderr,
            "Cannot create repository: install `gh` (https://cli.github.com)\n"
            "and run `gh auth login`, or set GITHUB_TOKEN in .env (needs `repo` scope).\n");
        return 1;
    }

    char body[AG_PATH];
    if (make_temp(body, sizeof(body)) != 0) {
        fprintf(stderr, "cannot create temp file\n");
        return 1;
    }
    FILE *bf = fopen(body, "w");
    if (!bf) { perror(body); return 1; }
    fprintf(bf, "{\"name\":\"%s\",\"private\":%s}",
            repo, priv ? "true" : "false");
    fclose(bf);

    char endpoint[512];
    if (github_is_org(owner, tok) == 1)
        snprintf(endpoint, sizeof(endpoint), "orgs/%s/repos", owner);
    else
        snprintf(endpoint, sizeof(endpoint), "user/repos");

    char q[AG_CMD];
    snprintf(q, sizeof(q),
        "curl -sf -X POST -H \"Authorization: token %s\" "
        "-H \"Accept: application/vnd.github+json\" "
        "-H \"Content-Type: application/json\" "
        "--data-binary @\"%s\" "
        "https://api.github.com/%s%s",
        tok, body, endpoint, R_NULL);
    int rc = system(q);
    remove(body);
    return rc == 0 ? 0 : 1;
}

/* ── Repo discovery ───────────────────────────────────────────────── */
static void get_root(char *out, size_t outsz) {
    out[0] = 0;
    FILE *fp = popen("git rev-parse --show-toplevel" R_ERR, "r");
    if (fp) {
        if (fgets(out, (int)outsz, fp)) out[strcspn(out, "\r\n")] = 0;
        pclose(fp);
    }
}

static void project_name(const char *root, char *out, size_t outsz) {
    const char *slash  = strrchr(root, '/');
    const char *bslash = strrchr(root, '\\');
    const char *sep    = slash > bslash ? slash : bslash;
    const char *base   = sep ? sep + 1 : root;
    snprintf(out, outsz, "%s", base);
    size_t n = strlen(out);
    if (n > 4 && !strcmp(out + n - 4, ".git")) out[n - 4] = 0;
}

/* ── Commit / description ─────────────────────────────────────────── */
static int commit_with_message(const char *msg) {
    char tmp[AG_PATH];
    if (make_temp(tmp, sizeof(tmp)) != 0) {
        fprintf(stderr, "cannot create temp file\n");
        return 1;
    }
    FILE *f = fopen(tmp, "w");
    if (!f) { perror(tmp); return 1; }
    fputs(msg, f);
    fputc('\n', f);
    fclose(f);

    char cmd[AG_CMD];
    snprintf(cmd, sizeof(cmd), "git commit -F \"%s\"", tmp);
    int rc = system(cmd);
    remove(tmp);
    return rc == 0 ? 0 : 1;
}

static void set_repo_description(const char *root, const char *msg) {
    char path[AG_PATH];
    snprintf(path, sizeof(path), "%s/.git/description", root);
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp, "%s\n", msg);
    fclose(fp);
}

static void usage(const char *p) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -u <url>       Remote URL (skips prompt; also updates existing remote)\n"
        "  -r <name>      Remote name (default: origin)\n"
        "  -b <branch>    Branch to push (default: current)\n"
        "  -d <desc>      Short description (commit msg + .git/description)\n"
        "  -n             Non-interactive (use defaults, never prompt)\n"
        "  -h             Show help\n"
        "\n"
        "URL precedence: -u flag  >  .env GIT_REMOTE_URL  >  prompt\n"
        "A base URL without .git gets /<project>.git appended.\n"
        "\n"
        "If the remote is a GitHub URL and the repo doesn't exist, autogit\n"
        "offers to create it via `gh` (if installed) or the REST API with\n"
        "GITHUB_TOKEN from .env. Set GITHUB_VISIBILITY=public|private\n"
        "(default: private) to choose repo visibility.\n", p);
}

/* ── Main ─────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    const char *remote = "origin";
    const char *branch = NULL;
    const char *url    = NULL;
    const char *desc   = NULL;
    int noninteractive = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-u") && i+1 < argc) url    = argv[++i];
        else if (!strcmp(argv[i], "-r") && i+1 < argc) remote = argv[++i];
        else if (!strcmp(argv[i], "-b") && i+1 < argc) branch = argv[++i];
        else if (!strcmp(argv[i], "-d") && i+1 < argc) desc   = argv[++i];
        else if (!strcmp(argv[i], "-n")) noninteractive = 1;
        else if (!strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "Unknown arg: %s\n", argv[i]); usage(argv[0]); return 1; }
    }

    if (system("git rev-parse --is-inside-work-tree" R_NULL) != 0) {
        fprintf(stderr, "Not inside a git repository.\n");
        return 1;
    }

    char root[AG_PATH];
    get_root(root, sizeof(root));
    if (!root[0]) { fprintf(stderr, "Could not locate repo root.\n"); return 1; }

    char proj[256];
    project_name(root, proj, sizeof(proj));

    char cur_branch[256] = {0};
    if (!branch) {
        FILE *fp = popen("git symbolic-ref --short HEAD" R_ERR, "r");
        if (fp) {
            if (fgets(cur_branch, sizeof(cur_branch), fp))
                cur_branch[strcspn(cur_branch, "\r\n")] = 0;
            pclose(fp);
        }
        if (!cur_branch[0]) strcpy(cur_branch, "main");
        branch = cur_branch;
    }

    /* .env: GIT_REMOTE_URL, GITHUB_VISIBILITY, GITHUB_TOKEN */
    char env_url_raw[AG_PATH] = {0};
    char env_url[AG_PATH]     = {0};
    int have_env_url = load_env(root, "GIT_REMOTE_URL", env_url_raw, sizeof(env_url_raw))
                       && env_url_raw[0];
    if (have_env_url)
        finalize_url(env_url_raw, proj, env_url, sizeof(env_url));

    char visibility[32] = "private";
    load_env(root, "GITHUB_VISIBILITY", visibility, sizeof(visibility));

    char gh_token[AG_LINE] = {0};
    if (load_env(root, "GITHUB_TOKEN", gh_token, sizeof(gh_token)) && gh_token[0]) {
#ifdef _WIN32
        _putenv_s("GITHUB_TOKEN", gh_token);
#else
        setenv("GITHUB_TOKEN", gh_token, 1);
#endif
    }

    char flag_url[AG_PATH] = {0};
    if (url) finalize_url(url, proj, flag_url, sizeof(flag_url));

    int added_remote = 0;

    /* ── Remote setup ─────────────────────────────────────────────── */
    if (!remote_exists(remote)) {
        char user[AG_LINE], host[AG_LINE];
        load_config(user, sizeof(user), host, sizeof(host));

        int need_default = !flag_url[0] && !have_env_url;
        if (need_default && (!user[0] || !host[0])) {
            if (noninteractive) {
                fprintf(stderr,
                    "No .autogitrc and -n given. Pass -u <url>, set GIT_REMOTE_URL "
                    "in .env, or run interactively first.\n");
                return 1;
            }
            printf("First-time setup (saved to %s)\n", config_path());
            char tmp[128];
            if (!user[0]) {
                if (!prompt("Username (github handle): ", tmp, sizeof(tmp)) || !tmp[0]) {
                    fprintf(stderr, "Username required.\n"); return 1;
                }
                snprintf(user, sizeof(user), "%s", tmp);
            }
            if (!host[0]) {
                if (!prompt("Host [github.com]: ", tmp, sizeof(tmp))) return 1;
                snprintf(host, sizeof(host), "%s", tmp[0] ? tmp : "github.com");
            }
            save_config(user, host);
        }

        char default_url[AG_PATH] = {0};
        if (user[0] && host[0])
            snprintf(default_url, sizeof(default_url),
                     "git@%s:%s/%s.git", host, user, proj);

        char chosen[AG_PATH];
        if (flag_url[0]) {
            snprintf(chosen, sizeof(chosen), "%s", flag_url);
        } else if (have_env_url) {
            snprintf(chosen, sizeof(chosen), "%s", env_url);
            printf("Using GIT_REMOTE_URL from .env -> %s\n", chosen);
        } else if (noninteractive) {
            snprintf(chosen, sizeof(chosen), "%s", default_url);
        } else {
            char line[1024], label[AG_PATH + 32];
            snprintf(label, sizeof(label), "Remote URL [%s]: ", default_url);
            if (!prompt(label, line, sizeof(line))) return 1;
            if (line[0])
                finalize_url(line, proj, chosen, sizeof(chosen));
            else
                snprintf(chosen, sizeof(chosen), "%s", default_url);
        }

        char cmd[AG_CMD];
        snprintf(cmd, sizeof(cmd), "git remote add %s \"%s\"", remote, chosen);
        if (run(cmd) != 0) { fprintf(stderr, "git remote add failed.\n"); return 1; }
        added_remote = 1;
    } else if (flag_url[0]) {
        char cmd[AG_CMD];
        snprintf(cmd, sizeof(cmd), "git remote set-url %s \"%s\"", remote, flag_url);
        run(cmd);
    }

    /* Read back configured remote URL */
    char cur_url[AG_PATH] = {0};
    {
        char q[512];
        snprintf(q, sizeof(q), "git remote get-url %s", remote);
        FILE *fp = popen(q, "r");
        if (fp) {
            if (fgets(cur_url, sizeof(cur_url), fp))
                cur_url[strcspn(cur_url, "\r\n")] = 0;
            pclose(fp);
        }
    }

    /* ── Ensure GitHub repo exists, offer to create if missing ───── */
    {
        char host[128], owner[128], repo_name[128];
        if (cur_url[0]
            && parse_git_url(cur_url, host, sizeof(host),
                             owner, sizeof(owner), repo_name, sizeof(repo_name)) == 0
            && strcmp(host, "github.com") == 0) {

            int exists = github_repo_exists(owner, repo_name);
            if (exists == 0) {
                printf("\nRepository %s/%s does not exist on GitHub.\n", owner, repo_name);
                int go = 1;
                if (!noninteractive) {
                    char ans[16];
                    if (!prompt("Create it now? [Y/n]: ", ans, sizeof(ans))) return 1;
                    if (ans[0] == 'n' || ans[0] == 'N') go = 0;
                }
                if (!go) {
                    fprintf(stderr, "Aborted (repo missing).\n");
                    return 1;
                }
                if (github_create_repo(owner, repo_name, visibility, noninteractive) != 0) {
                    fprintf(stderr, "Failed to create %s/%s on GitHub.\n", owner, repo_name);
                    return 1;
                }
                printf("Created https://github.com/%s/%s\n", owner, repo_name);
            } else if (exists == -1) {
                fprintf(stderr,
                    "note: cannot verify %s/%s exists (no gh CLI, no GITHUB_TOKEN).\n"
                    "      continuing — push may fail if repo is missing.\n",
                    owner, repo_name);
            }
        }
    }

    /* .env-vs-current warning */
    if (have_env_url && cur_url[0] && strcmp(cur_url, env_url) != 0) {
        fprintf(stderr, "note: .env GIT_REMOTE_URL differs from current remote\n");
        fprintf(stderr, "      current: %s\n", cur_url);
        fprintf(stderr, "      .env:    %s\n", env_url);
        fprintf(stderr, "      pass -u \"%s\" to change it\n", env_url);
    }

    /* ── Description / commit message ────────────────────────────── */
    char message[AG_LINE];
    if (desc) {
        snprintf(message, sizeof(message), "%s", desc);
    } else if (noninteractive) {
        snprintf(message, sizeof(message), "auto-commit");
    } else {
        if (!prompt("Short description: ", message, sizeof(message))) return 1;
        if (!message[0]) snprintf(message, sizeof(message), "auto-commit");
    }

    if (has_changes()) {
        if (run("git add -A") != 0) return 1;
        if (commit_with_message(message) != 0) {
            fprintf(stderr, "Commit failed.\n");
            return 1;
        }
    } else {
        printf("No changes to commit.\n");
    }

    if (added_remote || desc)
        set_repo_description(root, message);

    /* ── Push ────────────────────────────────────────────────────── */
    char cmd[AG_CMD];
    snprintf(cmd, sizeof(cmd), "git push -u %s %s", remote, branch);
    return run(cmd);
}