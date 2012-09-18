/* J Login >> pam.h */
/* John Lindgren */
/* September 17, 2012 */

#ifndef JLOGIN_PAM_H
#define JLOGIN_PAM_H

void * open_pam (const char * user, const char * pass, int vt, int display);
void close_pam (void * handle);

#endif
