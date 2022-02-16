
#include <unistd.h>
#include <sys/stat.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <security/pam_ext.h>

#define PAM_TTY_SAVED_OWNER "pam_tty_saved_owner"

/*
 * we use pam_set/get_data() but we do not need to use malloc()
 * for our simple use case here"
 */
static uid_t pam_tty_saved_owner = -1;

#define PAM_TTY_DEBUG	0x01
#define PAM_TTY_SILENT	0x02 /* currently not needed */

/*ARGSUSED*/
static void pam_tty_getopt (const pam_handle_t *pamh, int flags,
	                    int argc, const char **argv, int *opt)
{
    *opt = 0;

    if ((flags & PAM_SILENT) == PAM_SILENT)
	*opt |= PAM_TTY_SILENT;

    for (; argc-- > 0; ++argv)
    {
	if (!strcmp(*argv, "debug")) {
	    *opt |= PAM_TTY_DEBUG;
	} else {
	    pam_syslog(pamh, LOG_ERR, "Unknown option: %s", *argv);
	}
    }
}

int pam_sm_open_session (pam_handle_t *pamh, int flags, int argc,
                         const char **argv)
{
    int opt;
    int rv; /* reused */
    const char *user;
    const char *tty;
    const char *service;
    const struct passwd *pwdent;
    struct stat sbuf;

    pam_tty_getopt(pamh, flags, argc, argv, &opt);

    /*
     * get PAM_USER, PAM_TTY and PAM_SERVICE
     */
    rv = pam_get_item(pamh, PAM_USER, (const void **)&user);
    if (rv != PAM_SUCCESS || user == NULL || *user == '\0')
    {
	pam_syslog(pamh, LOG_NOTICE, "Could not query the user name.");
	return PAM_USER_UNKNOWN;
    }

    rv = pam_get_item(pamh, PAM_TTY, (const void **)&tty);
    if (rv != PAM_SUCCESS || tty == NULL || *tty == '\0')
    {
	pam_syslog(pamh, LOG_NOTICE, "Could not query the tty name.");
	return PAM_SESSION_ERR;
    }

    rv = pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
    if (rv != PAM_SUCCESS || service == NULL || *service == '\0')
    {
	pam_syslog(pamh, LOG_NOTICE, "Could not query the tty name.");
	return PAM_SESSION_ERR;
    }

    if (opt & PAM_TTY_DEBUG)
	pam_syslog(pamh, LOG_NOTICE,
		   "open_session: user=%s, tty=%s, service=%s",
		   user, tty, service);

    /*
     * get uid
     */
    pwdent = pam_modutil_getpwnam(pamh, user);
    if (pwdent == NULL)
    {
	pam_syslog(pamh, LOG_NOTICE, "User not in passwd.");
	return PAM_USER_UNKNOWN;
    }

    /*
     * save old owner
     */
    rv = stat(tty, &sbuf);
    if (rv < 0)
    {
	pam_syslog(pamh, LOG_ERR, "Could not stat '%s': %s.",
		   tty, strerror(errno));
	return PAM_SESSION_ERR;
    }
    pam_tty_saved_owner = sbuf.st_uid;

    rv = pam_set_data(pamh, PAM_TTY_SAVED_OWNER,
	              (void *) &pam_tty_saved_owner, NULL);
    if (rv != PAM_SUCCESS)
    {
	pam_syslog(pamh, LOG_NOTICE, "Could not save the tty owner.");
	return PAM_SESSION_ERR;
    }

    if (opt & PAM_TTY_DEBUG)
	pam_syslog(pamh, LOG_NOTICE, "open_session: saved old owner uid=%d",
		   sbuf.st_uid);

    /*
     * do the actual chown()
     */
    rv = chown(tty, pwdent->pw_uid, -1);
    if (rv < 0)
    {
	pam_syslog(pamh, LOG_ERR, "Could not chown '%s': %s.",
		   tty, strerror(errno));
	return PAM_SESSION_ERR;
    }

    if (opt & PAM_TTY_DEBUG)
	pam_syslog(pamh, LOG_NOTICE, "open_session: chowned %s to uid=%d.",
		   tty, pwdent->pw_uid);

    return PAM_SUCCESS;
}

int pam_sm_close_session (pam_handle_t *pamh, int flags, int argc,
                          const char **argv)
{
    int opt;
    int rv; /* reused */
    const char *tty;
    const char *service;
    uid_t *saved_owner;

    pam_tty_getopt(pamh, flags, argc, argv, &opt);

    /*
     * get PAM_TTY and PAM_SERVICE
     */
    rv = pam_get_item(pamh, PAM_TTY, (const void **)&tty);
    if (rv != PAM_SUCCESS || tty == NULL || *tty == '\0')
    {
	pam_syslog(pamh, LOG_NOTICE, "Could not query the tty name.");
	return PAM_SESSION_ERR;
    }

    rv = pam_get_item(pamh, PAM_SERVICE, (const void **)&service);
    if (rv != PAM_SUCCESS || service == NULL || *service == '\0')
    {
	pam_syslog(pamh, LOG_NOTICE, "Could not query the tty name.");
	return PAM_SESSION_ERR;
    }

    /*
     * get saved owner
     */
    rv = pam_get_data(pamh, PAM_TTY_SAVED_OWNER, (const void **) &saved_owner);
    if (rv != PAM_SUCCESS || *saved_owner < 0)
    {
	pam_syslog(pamh, LOG_NOTICE, "Could not retrieve saved tty owner.");
	return PAM_SESSION_ERR;
    }

    if (opt & PAM_TTY_DEBUG)
	pam_syslog(pamh, LOG_NOTICE,
		   "close_session: retrieved saved owner uid=%d.",
		   *saved_owner);

    /*
     * restore previous ownership with chown()
     *
     */
    rv = chown(tty, *saved_owner, -1);
    if (rv < 0)
    {
	pam_syslog(pamh, LOG_ERR, "Could not restore uid of '%s': %s.",
		   tty, strerror(errno));
	return PAM_SESSION_ERR;
    }

    if (opt & PAM_TTY_DEBUG)
	pam_syslog(pamh, LOG_NOTICE,
		   "close_session: restored owner of %s to uid=%d.",
		   tty, *saved_owner);

    return PAM_SUCCESS;
}

