/* AirScan (a.k.a. eSCL) backend for SANE
 *
 * Copyright (C) 2019 and up by Alexander Pevzner (pzz@apevzner.com)
 * See LICENSE for license terms and conditions
 *
 * OS Facilities
 */

#include "airscan.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __OpenBSD__
#   include <sys/types.h>
#   include <sys/sysctl.h>
#endif

/* Static variables */
static pthread_once_t os_homedir_once = PTHREAD_ONCE_INIT;
static char os_homedir_buf[PATH_MAX];

static pthread_once_t os_progname_once = PTHREAD_ONCE_INIT;
static char os_progname_buf[PATH_MAX];

/* Initialize os_homedir_buf. Called once, on demand
 */
static void
os_homedir_init (void)
{
    const char    *s = getenv("HOME");
    struct passwd pwd, *result;
    char          buf[16384];

    /* Try $HOME first, so user can override it's home directory */
    if (s != NULL && s[0] && strlen(s) < sizeof(os_homedir_buf)) {
        strcpy(os_homedir_buf, s);
        return;
    }

    /* Now try getpwuid_r */
    getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &result);
    if (result == NULL) {
        return;
    }

    if (result->pw_dir[0] && strlen(result->pw_dir) < sizeof(os_homedir_buf)) {
        strcpy(os_homedir_buf, result->pw_dir);
    }
}

/* Get user's home directory.
 * There is no need to free the returned string
 *
 * May return NULL in a case of error
 */
const char *
os_homedir (void)
{
    pthread_once(&os_homedir_once, os_homedir_init);
    return os_homedir_buf[0] ? os_homedir_buf : NULL;
}

/* Initialize os_progname_buf. Called once, on demand
 */
static void
os_progname_init (void)
{
#ifdef  OS_HAVE_LINUX_PROCFS
    ssize_t rc;

    rc = readlink("/proc/self/exe", os_progname_buf, sizeof(os_progname_buf));
    if (rc > 0) {
        char *s = strrchr(os_progname_buf, '/');
        if (s != NULL) {
            memmove(os_progname_buf, s+1, strlen(s+1) + 1);
        }
    }
#elif defined(__OpenBSD__)
    struct kinfo_proc kp;
    const int mib[6] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid(),
        sizeof(struct kinfo_proc), 1};
    size_t len = sizeof(kp);
    int rc = sysctl(mib, 6, &kp, &len, NULL, 0);
    if (rc == -1) {
        return;
    }
    memmove(os_progname_buf, kp.p_comm, KI_MAXCOMLEN);
#else
    /* This is nice to have but not critical. The caller already has
       to handle os_progname returning NULL. The error is left as a
       reminder for anyone porting this. */
#   error FIX ME
#endif
}

/* Get base name of the calling program. 
 * There is no need to free the returned string
 *
 * May return NULL in a case of error
 */
const char*
os_progname (void)
{
    pthread_once(&os_progname_once, os_progname_init);
    return os_progname_buf[0] ? os_progname_buf : NULL;
}

/* Make directory with parents
 */
int
os_mkdir (const char *path, mode_t mode)
{
    size_t len = strlen(path);
    char   *p = alloca(len + 1), *s;

    if (len == 0) {
        errno = EINVAL;
        return -1;
    }

    strcpy(p, path);

    for (s = strchr(p + 1, '/'); s != NULL; s = strchr(s + 1, '/')) {
        *s = '\0';
        mkdir(p, mode);
        *s = '/';
    }

    return mkdir(p, mode);
}

/* vim:ts=8:sw=4:et
 */
