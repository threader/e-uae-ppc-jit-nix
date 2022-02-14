 /*
  * UAE - The Un*x Amiga Emulator
  *
  * bsdsocket.library emulation - Unix
  *
  * Copyright 2000-2001 Carl Drougge <carl.drougge@home.se> <bearded@longhaired.org>
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  *
  */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "autoconf.h"

#include "threaddep/thread.h"
#include "native2amiga.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <arpa/inet.h>

#ifdef DEBUG_BSDSOCKET
#define DEBUG_LOG(x...) write_log(x)
#else
#define DEBUG_LOG(x...)
#endif

/* BSD-systems don't seem to have MSG_NOSIGNAL..
   @@@ We need to catch SIGPIPE on those systems! (?) */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* Preparing for GetSocketEvents().. */
struct bsd_socket {
    int         s;
    uae_u32     events;
    uae_u32     eventmask;
};

/* Maybe this should be reworked, somehow.. */
struct bsd_gl_o {
    pthread_t   thread;
    sem_t       sem;
    int         action;
    struct sockaddr_in addr;
    int         addrlen;
    uae_u32     s;
    uae_u32     flags;
    void       *buf;
    uae_u32     len;
    struct bsd_socket sockets [64];
    int         nfds;
    uae_u32     sets [3];
    uae_u32     timeout;
    uae_u32     a_addr;
    uae_u32     a_addrlen;
    uae_u32     hostent;
    uae_u32     news;
    uae_u32     to, tolen, from, fromlen;
    uae_u32     name;
    uae_u32     type;
    int         dtablesize;
};

pthread_key_t global_key;

#define GL_O      (((struct bsd_gl_o **)(get_real_address (global)))[0])
#define GL_task   get_long (global + 20)
#define GL_signal get_long (global + 24)
#define GL_iomask get_long (global + 48)

#define S_GL_result(x) put_long (global + 16, x)
#define S_GL_errno(x)  __S_GL_errno (global, x)

/* This should check for range-violations. But then again, so should everything else.. */
#define bsd_findsocket(o, i) (o->sockets [i].s)
#define sock_getsock(i, o) (o->sockets [i].s)
#define sock_setsock(n, i, o) (o->sockets [i].s = n)

#define bsd_findfreesocket(o) __bsd_findfreesocket (0, o)
#define bsdsock_Signal(task, sig) uae_Signal (task, sig)

void bsdlib_install (void);
void bsdlib_reset (void);

static void *bsdlib_threadfunc (void *arg);
uae_u32 bsdsocklib_Init (uae_u32 global, uae_u32 init);
uae_u32 bsdsocklib_Socket (int domain, int type, int protocol, uae_u32 s, uae_u32 global);
uae_u32 bsdsocklib_Connect (uae_u32 s, uae_u32 a_addr , unsigned int addrlen, uae_u32 global);
uae_u32 bsdsocklib_Sendto (uae_u32 s, uae_u32 msg, uae_u32 len, uae_u32 flags, uae_u32 to, uae_u32 tolen, uae_u32 global);
uae_u32 bsdsocklib_Recvfrom (uae_u32 s, uae_u32 buf, uae_u32 len, uae_u32 flags, uae_u32 from, uae_u32 fromlen, uae_u32 global);
static uae_u32 bsdsocklib_demux (void);
uae_u32 bsdsocklib_Setsockopt (uae_u32 s, uae_u32 level, uae_u32 optname, uae_u32 optval, uae_u32 optlen, uae_u32 global);
uae_u32 bsdsocklib_CloseSocket (uae_u32 s, uae_u32 global);
void __S_GL_errno (uae_u32 global, int en);
uae_u32 bsdsocklib_Gethostbyname (uae_u32 name, uae_u32 hostent, uae_u32 global);
uae_u32 bsdsocklib_WaitSelect (int nfds, uae_u32 readfds, uae_u32 writefds, uae_u32 exceptfds, uae_u32 timeout, uae_u32 global);
uae_u32 bsdsocklib_abortSomething (uae_u32 global);
uae_u32 bsdsocklib_Bind (uae_u32 s, uae_u32 name, int a_namelen, uae_u32 global);
uae_u32 bsdsocklib_Listen (uae_u32 s, int backlog, uae_u32 global);
uae_u32 bsdsocklib_Accept (uae_u32 s, uae_u32 addr, uae_u32 addrlen, uae_s32 news, uae_u32 global);
void killthread_sighandler (int sig);
void sigio_sighandler (int sig);
int __bsd_findfreesocket (int first, struct bsd_gl_o *gl_o);
uae_u32 bsdthr_WaitSelect (struct bsd_gl_o *gl_o);
int bsd_amigaside_FD_ISSET (int n, uae_u32 set);
void bsd_amigaside_FD_ZERO (uae_u32 set);
void bsd_amigaside_FD_SET (int n, uae_u32 set);
uae_u32 bsdsocklib_Getprotobyname (uae_u32 name, uae_u32 a_p, uae_u32 global);
uae_u32 bsdsocklib_Getservbyname (uae_u32 name, uae_u32 proto, uae_u32 a_s, uae_u32 global);
uae_u32 bsdsocklib_Gethostname (uae_u32 name, uae_u32 namelen, uae_u32 global);
uae_u32 bsdsocklib_Inet_addr (uae_u32 cp, uae_u32 global);
uae_u32 bsdsocklib_Inet_NtoA (uae_u32 in, uae_u32 buf, uae_u32 global);
uae_u32 bsdsocklib_SocketBaseTagList (uae_u32 tags, uae_u32 global);
uae_u32 bsdsocklib_Shutdown (uae_u32 s, uae_u32 how, uae_u32 global);
uae_u32 bsdsocklib_findfreesocket (uae_u32 first, uae_u32 global);
uae_u32 bsdsocklib_Dup2Socket (uae_s32 fd1, uae_s32 fd2, uae_u32 global);
int copysockaddr_a2n (struct sockaddr_in *addr, uae_u32 a_addr, unsigned int len);
int copysockaddr_n2a (uae_u32 a_addr, struct sockaddr_in *addr, unsigned int len);
uae_u32 bsdthr_Accept_2 (int a_s, struct bsd_gl_o *gl_o);
uae_u32 bsdthr_Recv_2 (int s, struct bsd_gl_o *gl_o);
uae_u32 bsdthr_blockingstuff (int s, uae_u32 (*tryfunc)(int, struct bsd_gl_o *), struct bsd_gl_o *gl_o);
uae_u32 bsdthr_SendRecvAcceptConnect (uae_u32 (*tryfunc)(int, struct bsd_gl_o *), struct bsd_gl_o *gl_o);
uae_u32 bsdthr_Send_2 (int s, struct bsd_gl_o *gl_o);
uae_u32 bsdthr_Connect_2 (int s, struct bsd_gl_o *gl_o);
uae_u32 bsdsocklib_Getsockopt (uae_u32 s, uae_u32 level, uae_u32 optname, uae_u32 optval, uae_u32 optlen, uae_u32 global);
uae_u32 bsdsocklib_Getsockpeername (uae_u32 s, uae_u32 name, uae_u32 namelen, uae_u32 global, int which);
uae_u32 bsdsocklib_Gethostbyaddr (uae_u32 addr, uae_u32 len, uae_u32 type, uae_u32 hostent, uae_u32 global);
void sock_init (int s, struct bsd_gl_o *gl_o);
int sock_close (int s, struct bsd_gl_o *gl_o);
uae_u32 bsdsocklib_IoctlSocket (uae_u32 s, uae_u32 action, uae_u32 argp, uae_u32 global);

void __S_GL_errno (uae_u32 global, int e)
{
    int size = get_long (global + 36);
    put_long (global + 28, e);
    switch (size) {
      case 1: put_byte (get_long (global + 32), e); break;
      case 2: put_word (get_long (global + 32), e); break;
      case 4: put_long (get_long (global + 32), e); break;
    }
}

int bsd_amigaside_FD_ISSET (int n, uae_u32 set)
{
    uae_u32 foo = get_long (set + (n / 32));
    if (foo & (1 << (n % 32))) return 1;
    return 0;
}

void bsd_amigaside_FD_ZERO (uae_u32 set)
{
    put_long (set, 0);
    put_long (set + 4, 0);
}

void bsd_amigaside_FD_SET (int n, uae_u32 set)
{
    set = set + (n / 32);
    put_long (set, get_long (set) | (1 << (n % 32)));
}

void sock_init (int s, struct bsd_gl_o *gl_o)
{
    gl_o->sockets [s].s         = -1;
    gl_o->sockets [s].events    = 0;
    gl_o->sockets [s].eventmask = 0;
}

int sock_close (int s, struct bsd_gl_o *gl_o)
{
    int r = 0;
    int rs;
    if ((rs = sock_getsock (s, gl_o)) >= 0) {
        r = close (rs);
    }
    if (r == 0) sock_setsock (-1, s, gl_o);
    return r;
}


uae_u32 bsdthr_WaitSelect (struct bsd_gl_o *gl_o)
{
    fd_set sets [3];
    int i, s, max = 0, set, a_s;
    uae_u32 a_set;
    struct timeval tv;
    int r;

DEBUG_LOG ("WaitSelect: %d 0x%x 0x%x 0x%x 0x%x\n", gl_o->nfds, gl_o->sets [0], gl_o->sets [1], gl_o->sets [2], gl_o->timeout);
if (gl_o->timeout) fprintf (stderr, "            timeout %d %d\n", get_long (gl_o->timeout), get_long (gl_o->timeout + 4));
    FD_ZERO (&sets [0]);
    FD_ZERO (&sets [1]);
    FD_ZERO (&sets [2]);
    for (set = 0; set < 3; set++) {
        if (gl_o->sets [set] != 0) {
            a_set = gl_o->sets [set];
            for (i = 0; i < gl_o->nfds; i++) {
                if (bsd_amigaside_FD_ISSET (i, a_set)) {
                    s = sock_getsock (i, gl_o);
DEBUG_LOG ("WaitSelect: AmigaSide %d set. NativeSide %d.\n", i, s);
                    if (s == -1) {
                        write_log ("BSDSOCK: WaitSelect() called with invalid descriptor %d in set %d.\n", i, set);
                    } else {
                        FD_SET (s, &sets [set]);
                        if (max < s) max = s;
                    }
                }
            }
        }
    }
    max++;
    if (gl_o->timeout) {
        tv.tv_sec  = get_long (gl_o->timeout);
        tv.tv_usec = get_long (gl_o->timeout + 4);
    }
    r = select (max, &sets [0], &sets [1], &sets [2], (gl_o->timeout == 0) ? NULL : &tv);
    if( r > 0 ) {
        /* This is perhaps slightly inefficient, but I don't care.. */
        for (set = 0; set < 3; set++) {
            a_set = gl_o->sets [set];
            if (a_set != 0) {
                bsd_amigaside_FD_ZERO (a_set);
                for (i = 0; i < gl_o->nfds; i++) {
                    a_s = sock_getsock (i, gl_o);
                    if (a_s != -1) {
                        if (FD_ISSET (a_s, &sets [set])) bsd_amigaside_FD_SET (i, a_set);
                    }
                }
            }
        }
    } else if(r == 0) {         /* Timeout. I think we're supposed to clear the sets.. */
        for (set = 0; set < 3; set++) if (gl_o->sets [set] != 0) bsd_amigaside_FD_ZERO (gl_o->sets [set]);
    }
DEBUG_LOG ("WaitSelect: %d\n", r);
    return r;
}

/* @@@ This should do something to make sure something happens if it's called before the call it is to abort has begun... */
void killthread_sighandler (int sig)
{ }

void sigio_sighandler (int sig)
{
    uae_u32 global = (uae_u32) pthread_getspecific (global_key);
    if (global) {
	if (GL_iomask) bsdsock_Signal (GL_task, GL_iomask);
    }
DEBUG_LOG ("SIGIO: global 0x%x, 0x%x\n", global, global?GL_iomask:0);
}

uae_u32 bsdthr_Accept_2 (int a_s, struct bsd_gl_o *gl_o)
{
    int i, foo, s;
    long flags;

    foo = sizeof (struct sockaddr_in);
    if ((s = accept(a_s, &gl_o->addr, &foo)) >= 0) {
        if ((flags = fcntl (s, F_GETFL)) == -1) flags = 0;
        fcntl (s, F_SETFL, flags & ~O_NONBLOCK);                /* @@@ Don't do this if it's supposed to stay nonblocking... */
        i = gl_o->news;
        sock_init (i, gl_o);
        sock_setsock (s, i, gl_o);
        foo = get_long (gl_o->a_addrlen);
        if (foo > 16) put_long (gl_o->a_addrlen, 16);
        copysockaddr_n2a (gl_o->a_addr, &gl_o->addr, foo);
        return i;
    }
    return -1;
}

uae_u32 bsdthr_SendRecvAcceptConnect (uae_u32 (*tryfunc)(int, struct bsd_gl_o *), struct bsd_gl_o *gl_o)
{
    int s = sock_getsock (gl_o->s, gl_o);
    if (s < 0) {
        errno = EBADF;
        return -1;
    }
    return bsdthr_blockingstuff (s, tryfunc, gl_o);
}

uae_u32 bsdthr_Recv_2 (int s, struct bsd_gl_o *gl_o)
{
    int foo, l, i;
    if (gl_o->from == 0) {
        foo = recv (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL);
    } else {
        l = sizeof (struct sockaddr_in);
        i = get_long (gl_o->fromlen);
        copysockaddr_a2n (&gl_o->addr, gl_o->from, i);
DEBUG_LOG ("recv2: fromlen == %d\n", i);
        foo = recvfrom (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL, &gl_o->addr, &l);
        if (foo >= 0) {
            copysockaddr_n2a (gl_o->from, &gl_o->addr, l);
            if (i > 16) put_long (gl_o->fromlen, 16);
        }
    }
    return foo;
}

uae_u32 bsdthr_blockingstuff (int s, uae_u32 (*tryfunc)(int, struct bsd_gl_o *), struct bsd_gl_o *gl_o)
{
    int done = 0, foo;
    long flags;
    if ((flags = fcntl (s, F_GETFL)) == -1) flags = 0;
    fcntl (s, F_SETFL, flags | O_NONBLOCK);
    while (!done) {
        done = 1;
        foo = tryfunc (s, gl_o);
        if ((foo < 0) && !(flags & O_NONBLOCK)) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINPROGRESS)) {
                fd_set set;
                FD_ZERO (&set);
                FD_SET (s, &set);
                if (select (s + 1, (errno == EINPROGRESS) ? NULL : &set, (errno == EINPROGRESS) ? &set : NULL, NULL, NULL) == -1) {
                    fcntl (s, F_SETFL, flags);
                    return -1;
                }
                done = 0;
            }
        }
    }
    fcntl (s, F_SETFL, flags);
    return foo;
}

uae_u32 bsdthr_Send_2 (int s, struct bsd_gl_o *gl_o)
{
    if (gl_o->to == 0) {
        return send (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL);
    } else {
        int l = sizeof (struct sockaddr_in);
        copysockaddr_a2n (&gl_o->addr, gl_o->to, gl_o->tolen);
        return sendto (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL, &gl_o->addr, l);
    }
}

uae_u32 bsdthr_Connect_2 (int s, struct bsd_gl_o *gl_o)
{
    if (gl_o->action == 1) {
        gl_o->action = -1;
        return connect (s, &gl_o->addr, gl_o->addrlen);
    } else {
        int foo, bar;
        bar = sizeof (foo);
        if (getsockopt (s, SOL_SOCKET, SO_ERROR, &foo, &bar) == 0) {
            errno = foo;
        }
        return 0;       /* We have to return 0 to get out of bsdthr_blockingstuff () */
    }
}

static void *bsdlib_threadfunc (void *arg)
{
    uae_u32 global = (uae_u32) arg;
    struct bsd_gl_o *gl_o = GL_O;
    struct hostent *hostent;
    int foo, s, i, l;

DEBUG_LOG ("THREAD_START\n");
    pthread_setspecific (global_key, arg);
    while (1) {
        sem_wait (&gl_o->sem);
        switch (gl_o->action) {
          case 0:       /* kill thread (CloseLibrary) */
            for(i = 0; i < 64; i++) {           /* Are dtable is always 64, we just claim otherwise if it makes the app happy. */
                sock_close (i, gl_o);
            }
/* @@@ We should probably have a signal for this too, since it can block.. */
/*     Harmless though. */
DEBUG_LOG ("THREAD_DEAD\n");
            sem_destroy (&gl_o->sem);
            free (gl_o);
            return NULL;
          case 1:       /* Connect */
            bsdthr_SendRecvAcceptConnect (bsdthr_Connect_2, gl_o);      /* Only returns -1 for aborts, errors give 0. */
            S_GL_result ((errno == 0) ? 0 : -1);
            break;
/* @@@ Should check (from|to)len so it's 16.. */
          case 2:       /* Send[to] */
            S_GL_result (bsdthr_SendRecvAcceptConnect (bsdthr_Send_2, gl_o));
            break;
          case 3:       /* Recv[from] */
            S_GL_result (bsdthr_SendRecvAcceptConnect (bsdthr_Recv_2, gl_o));
            break;
          case 4:       /* Gethostbyname */
            if ((hostent = gethostbyname (get_real_address (gl_o->name)))) {
                put_long (gl_o->hostent + 8, hostent->h_addrtype);
                put_long (global + 40, htonl (((uae_u32 *) (hostent->h_addr))[0]));
                strncpy (get_real_address (get_long (gl_o->hostent)), hostent->h_name, 128);
                S_GL_result (gl_o->hostent);
            } else {
                S_GL_result (0);
            }
            break;
          case 5:       /* WaitSelect */
            S_GL_result (bsdthr_WaitSelect (gl_o));
            break;
          case 6:       /* Accept */
            S_GL_result (bsdthr_SendRecvAcceptConnect (bsdthr_Accept_2, gl_o));
            break;
          case 7:
            if ((hostent = gethostbyaddr (get_real_address (gl_o->a_addr), gl_o->a_addrlen, gl_o->type))) {
                put_long (gl_o->hostent + 8, hostent->h_addrtype);
                put_long (global + 40, htonl (((uae_u32 *) (hostent->h_addr))[0]));
                strncpy (get_real_address (get_long (gl_o->hostent)), hostent->h_name, 128);
                S_GL_result (gl_o->hostent);
            } else {
                S_GL_result (0);
            }
        }
        S_GL_errno (errno);
        bsdsock_Signal (GL_task, GL_signal);
    }
    return NULL;        /* Just to keep GCC happy.. */
}

uae_u32 bsdsocklib_Init (uae_u32 global, uae_u32 init)
{
    struct bsd_gl_o *gl_o;
    int i;

    if (init) {
/* Maybe this should have one cleanup, at the end? .. */
        if ((GL_O = calloc (1, sizeof (struct bsd_gl_o))) == NULL) {
            write_log ("BSDSOCK: Failed to allocate memory.\n");
            return 1;
        }
        gl_o = GL_O;
        if (sem_init (&gl_o->sem, 0, 0)) {
            write_log ("BSDSOCK: Failed to create semaphore.\n");
            free (gl_o);
            return 1;
        }
/* @@@ The thread should be PTHREAD_CREATE_DETACHED */
        if (pthread_create (&gl_o->thread, NULL, bsdlib_threadfunc, (void *) global)) {
            write_log ("BSDSOCK: Failed to create thread.\n");
            sem_destroy (&gl_o->sem);
            free (gl_o);
            return 1;
        }
        gl_o->dtablesize = 64;
        for (i = 0; i < gl_o->dtablesize; i++) {
            sock_init (i, gl_o);
        }
    } else {
        gl_o = GL_O;
        gl_o->action = 0;
        sem_post (&gl_o->sem);
    }
    return 1; /* 0 if we need a signal-poller, 1 if not. */
}

uae_u32 bsdsocklib_Socket (int domain, int type, int protocol, uae_u32 s, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    int i = s;
    if (i == -1) {
        S_GL_errno (EMFILE);
        return -1;
    }
    sock_init (i, gl_o);
    sock_setsock (socket(domain, type, protocol), i, gl_o);
    if (sock_getsock (i, gl_o) == -1) {
        S_GL_errno (errno);
        return -1;
    }
DEBUG_LOG ("bsdsocklib_Socket: Native %d, Amiga %d.\n", sock_getsock (i, gl_o), i);
    return i;
}

uae_u32 bsdsocklib_Connect (uae_u32 s, uae_u32 a_addr , unsigned int addrlen, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    if (copysockaddr_a2n (&gl_o->addr, a_addr, addrlen)) {
        write_log ("BSDSOCK: bsdsocklib_Connect: addrlen %d > %d or < 8\n" , addrlen , sizeof (struct sockaddr_in));
        S_GL_errno (ECONNREFUSED);      /* What should I set this to? */
        S_GL_result (-1);
        return 0;
    } else {
        gl_o->s = s;
        gl_o->addrlen = sizeof (struct sockaddr_in);
        gl_o->action = 1;
        sem_post (&gl_o->sem);
        return 1;
    }
}

uae_u32 bsdsocklib_Sendto (uae_u32 s, uae_u32 msg, uae_u32 len, uae_u32 flags, uae_u32 to, uae_u32 tolen, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    gl_o->s      = s;
    gl_o->buf    = get_real_address (msg);
    gl_o->len    = len;
    gl_o->flags  = flags;
    gl_o->to     = to;
    gl_o->tolen  = tolen;
    gl_o->action = 2;
    sem_post (&gl_o->sem);
    return 1;
}

uae_u32 bsdsocklib_Recvfrom (uae_u32 s, uae_u32 buf, uae_u32 len, uae_u32 flags, uae_u32 from, uae_u32 fromlen, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    gl_o->s      = s;
    gl_o->buf    = get_real_address (buf);
    gl_o->len    = len;
    gl_o->flags  = flags;
    gl_o->from   = from;
    gl_o->fromlen= fromlen;
    gl_o->action = 3;
    sem_post (&gl_o->sem);
    return 1;
}

uae_u32 bsdsocklib_Setsockopt (uae_u32 s, uae_u32 level, uae_u32 optname, uae_u32 optval, uae_u32 optlen, uae_u32 global)
{
    int r = setsockopt (sock_getsock (s, GL_O), level, optname, optval ? get_real_address (optval) : NULL, optlen);
    S_GL_errno (ENOPROTOOPT);
    return r;
}

/* @@@ This should probably go on the thread.. */
uae_u32 bsdsocklib_CloseSocket (uae_u32 s, uae_u32 global)
{
    S_GL_result (sock_close (s, GL_O));
    S_GL_errno (errno);
    return 0;
}

uae_u32 bsdsocklib_Gethostbyname (uae_u32 name, uae_u32 hostent, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    gl_o->name    = name;
    gl_o->hostent = hostent;
    gl_o->action  = 4;
    sem_post (&gl_o->sem);
    return 1;
}

uae_u32 bsdsocklib_Gethostbyaddr (uae_u32 addr, uae_u32 len, uae_u32 type, uae_u32 hostent, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    gl_o->a_addr    = addr;
    gl_o->a_addrlen = len;
    gl_o->type      = type;
    gl_o->hostent   = hostent;
    gl_o->action    = 7;
    sem_post (&gl_o->sem);
    return 1;
}

uae_u32 bsdsocklib_WaitSelect (int nfds, uae_u32 readfds, uae_u32 writefds, uae_u32 exceptfds, uae_u32 timeout, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    gl_o->nfds = nfds;
    gl_o->sets [0] = readfds;
    gl_o->sets [1] = writefds;
    gl_o->sets [2] = exceptfds;
    gl_o->timeout = timeout;
    gl_o->action = 5;
    sem_post (&gl_o->sem);
    return 1;
}

uae_u32 bsdsocklib_abortSomething (uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    pthread_kill (gl_o->thread, SIGUSR1);
    return 1;
}

uae_u32 bsdsocklib_Bind (uae_u32 s, uae_u32 name, int namelen, uae_u32 global)
{
    int rs = sock_getsock (s, GL_O);
    struct sockaddr_in addr;
    int r;
    if (copysockaddr_a2n (&addr, name, namelen)) {
        S_GL_errno (EACCES);
        return -1;
    }
    r = bind (rs, &addr, sizeof (struct sockaddr_in));
    S_GL_errno (errno);
    return r;
}

int copysockaddr_a2n (struct sockaddr_in *addr, uae_u32 a_addr, unsigned int len)
{
    if ((len > sizeof (struct sockaddr_in)) || (len < 8)) return 1;
    addr->sin_family      = get_byte (a_addr + 1);
    addr->sin_port        = htons (get_word (a_addr + 2));
    addr->sin_addr.s_addr = htonl (get_long (a_addr + 4));
    if (len > 8) memcpy (&addr->sin_zero, get_real_address (a_addr + 8), len - 8);   /* Pointless? */
    return 0;
}

int copysockaddr_n2a (uae_u32 a_addr, struct sockaddr_in *addr, unsigned int len)
{
    if (len < 8) return 1;
    put_byte (a_addr, 0);                       /* Anyone use this field? */
    put_byte (a_addr + 1, addr->sin_family);
    put_word (a_addr + 2, ntohs (addr->sin_port));
    put_long (a_addr + 4, ntohl (addr->sin_addr.s_addr));
    if (len > 8) memset (get_real_address (a_addr + 8), 0, len - 8);
    return 0;
}

uae_u32 bsdsocklib_Listen (uae_u32 s, int backlog, uae_u32 global)
{
    int r = listen (sock_getsock (s, GL_O), backlog);
    S_GL_errno (errno);
    return r;
}

uae_u32 bsdsocklib_Accept (uae_u32 s, uae_u32 addr, uae_u32 addrlen, uae_s32 news, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    if (news == -1) {
        S_GL_errno (EMFILE);
        S_GL_result (-1);
        return 0;
    }
    gl_o->s = s;
    gl_o->a_addr = addr;
    gl_o->a_addrlen = addrlen;
    gl_o->news = news;
    gl_o->action = 6;
    sem_post (&gl_o->sem);
    return 1;
}

int __bsd_findfreesocket (int first, struct bsd_gl_o *gl_o)
{
    int i;
    for (i = first; i < gl_o->dtablesize; i++) {
        if (sock_getsock (i, gl_o) == -1) {
            return i;
        }
    }
    return -1;
}

uae_u32 bsdsocklib_Getprotobyname (uae_u32 name, uae_u32 a_p, uae_u32 global)
{
    struct protoent *p = getprotobyname (get_real_address (name));
    if (p == NULL) {
        S_GL_errno (errno);
        return 0;
    }
    strncpy (get_real_address (get_long (a_p)), p->p_name, 128);
    put_long (a_p + 8, p->p_proto);
    return a_p;
}

uae_u32 bsdsocklib_Getservbyname (uae_u32 name, uae_u32 proto, uae_u32 a_s, uae_u32 global)
{
    struct servent *s = getservbyname (get_real_address (name), get_real_address (proto));
    if (s == NULL) {
        S_GL_errno (errno);
        return 0;
    }
    strncpy (get_real_address (get_long (a_s)), s->s_name, 128);
    put_long (a_s + 8, ntohs (s->s_port));
    strncpy (get_real_address (get_long (a_s + 12)), s->s_proto, 32);
    return a_s;
}

uae_u32 bsdsocklib_Gethostname (uae_u32 name, uae_u32 namelen, uae_u32 global)
{
    uae_u32 r = gethostname (get_real_address (name), namelen);
    S_GL_errno (errno);
    return r;
}

uae_u32 bsdsocklib_Inet_addr (uae_u32 cp, uae_u32 global)
{
    return htonl( inet_addr (get_real_address (cp)));
}

uae_u32 bsdsocklib_Getsockpeername (uae_u32 s, uae_u32 name, uae_u32 namelen, uae_u32 global, int which)
{
    int rs = sock_getsock (s, GL_O);
    struct sockaddr_in addr;
    int nl = sizeof (struct sockaddr_in);
    int a_nl;
    uae_u32 r;
    if (which) {
        r = getsockname (rs, &addr, &nl);
    } else {
        r = getpeername (rs, &addr, &nl);
    }
    if (r == 0) {
       a_nl = get_long (namelen);
       copysockaddr_n2a (name, &addr, a_nl);
       if (a_nl > 16) put_long (namelen, 16);
    } else {
        S_GL_errno (errno);
    }
    return r;
}

uae_u32 bsdsocklib_Inet_NtoA (uae_u32 in, uae_u32 buf, uae_u32 global)
{
    struct in_addr addr;
    addr.s_addr = ntohl (in);
    strncpy (get_real_address (buf), inet_ntoa (addr), 16);
    return buf;
}

uae_u32 bsdsocklib_Shutdown (uae_u32 s, uae_u32 how, uae_u32 global)
{
    int r = shutdown (sock_getsock (s, GL_O), how);
    S_GL_errno (errno);
    return r;
}

uae_u32 bsdsocklib_SocketBaseTagList (uae_u32 tags, uae_u32 global)
{
    uae_u32 tag, val, code, byref, set, index = 0, table;
    while (1) {
        tag = get_long (tags);
        if (tag == 0) return 0;
        val = get_long (tags + 4);
        tags += 8;
        index++;

        if (tag & 0x80000000) {
            code  = (tag & 0x7ffe) >> 1;
            byref = tag & 0x8000;
            set   = tag & 0x0001;
            if (tag & 0x7fff0000) {     /* Seriously unknown tag.. */
                write_log ("BSDSOCK: SocketBaseTagList: Unknown tag 0x%x.\n", tag);
                return index;
            }
DEBUG_LOG ("SocketBaseTagList: code == %d. 0x%x\n", code, tag);
            switch (code) {
              case 21:
              case 22:  /* Set errno */
              case 24:
                if (!set) return index;
                put_long (global + 32, byref ? get_long (val) : val);
                put_long (global + 36, code & 0x7);
                break;
              case 1:   /* sigint   */
              case 2:   /* sigio    */
              case 3:   /* sigurg   */
              case 4:   /* sigevent */
                if (set) {
                    put_long (global + 40 + 4*tag, byref ? get_long (val) : val);
                } else {
                    put_long (byref ? val : tags - 4, get_long (global + 40 + 4*tag));
                }
                break;
              case 9:   /* SBTC_FDCALLBACK */
                put_long (global + 60, byref ? get_long (val) : val);
                break;
              case 8:   /* SBTC_DTABLESIZE */
                if (set) {
                    if (byref) val = get_long (val);
                    if (val > 64) {
                        write_log ("BSDSOCK: SocketBaseTagList: Attempt to set DTABLESIZE to %d.\n", val);
                        return index;
                    }
                    GL_O->dtablesize = val;
                } else {
                    put_long (byref ? val : tags - 4, GL_O->dtablesize);
                }
                break;
              case 14:  /* SBTC_ERRNOSTRPTR */
                if (set) return index;
                table = get_long (global + 64);
                put_long (byref ? val : tags - 4, get_long (table + 4 * (byref ? get_long (val) : val)));
                break;
              default:
                write_log ("BSDSOCK: SocketBaseTagList: (0x%x) code %d, byref %x, set %x.\n", tag, code, byref, set);
                return index;
            }
        } else {
            /* This is not an amitcp-tag, but something utility.library would have handled. We need to handle it.. */
DEBUG_LOG ("bsdsocklib_SocketBaseTagList: Other tag 0x%x.\n", tag);
return index;
        }
    }
}

uae_u32 bsdsocklib_findfreesocket (uae_u32 first, uae_u32 global)
{
    return __bsd_findfreesocket (first, GL_O);
}

uae_u32 bsdsocklib_Dup2Socket (uae_s32 fd1, uae_s32 fd2, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    int rs, ns;
    if (fd2 == -1) {
        fd2 = bsd_findfreesocket (gl_o);
    }
    if ((fd2 > 63) || (fd2 == -1)) {
        S_GL_errno (EMFILE);
        return -1;
    }
    if (fd1 == -1) {
        sock_close (fd2, gl_o);         /* @@@ So what if this failes? Can't be bothered to check.. */
        sock_setsock (-2, fd2, gl_o);
        return fd2;
    }
    rs = sock_getsock (fd1, gl_o);
    if (rs < 0) {
        S_GL_errno (EBADF);
        return -1;
    }
    ns = dup (rs);
    if (rs == -1) {
        S_GL_errno (errno);
        return -1;
    }
    sock_close (fd2, gl_o);             /* @@@ So what if this failes? Can't be bothered to check.. */
    sock_setsock (ns, fd2, gl_o);
    return fd2;
}

/* @@@ This is really quite broken.. */
uae_u32 bsdsocklib_Getsockopt (uae_u32 s, uae_u32 level, uae_u32 optname, uae_u32 optval, uae_u32 optlen, uae_u32 global)
{
    int len = 0, r;
/* This is nice.. maybe I should check more eh? */
    if ((level == 0xffff) && (optname == 0x1007)) {
	level = 1;
	optname = 4;
    }
    if (optlen) len = get_long (optlen);
    r = getsockopt (sock_getsock (s, GL_O), level, optname, optval ? get_real_address (optval) : NULL, optlen ? &len : NULL);
    if (optlen) put_long (optlen, len);
    S_GL_errno (errno);
DEBUG_LOG ("Getsockopt: sock %d, level %d, 'name' %d, len %d -> %d, %d\n", s, level, optname, len, r, errno);
    return r;
}

uae_u32 bsdsocklib_IoctlSocket (uae_u32 s, uae_u32 action, uae_u32 argp, uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;
    int sock = sock_getsock (s, gl_o), r, argval = get_long (argp);
    long flags;
    if ((flags = fcntl (sock, F_GETFL)) == -1) {
	S_GL_errno (errno);
	return -1;
    }
    switch (action) {
      case 0x8004667D: /* FIOASYNC */
	r = fcntl (sock, F_SETFL, argval ? flags | O_ASYNC : flags & ~O_ASYNC);
	return r;
      case 0x8004667E: /* FIONBIO */
	r = fcntl (sock, F_SETFL, argval ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
	return r;
    }
DEBUG_LOG ("Ioctl: %d (%d), %d, %d\n", s, sock, action, argp);
    S_GL_errno (EINVAL);
    return -1;
}

char *bsfn[] = {"", "init", "socket", "bind", "listen", "accept", "connect", "", "send[to]",
	"recv[from]", "setsockopt", "closesocket", "waitselect", "abortsomething", "gethostbyname",
	"getprotobyname", "getservbyname", "gethostname", "inet_addr", "getsockpeername",
	"inet_ntoa", "socketbasetaglist", "shutdown", "findfreesocket", "dup2socket",
	"getsockopt", "getsockpeername", "gethostbyaddr", "ioctlsocket", ""};

#define ARG(x) (get_long (m68k_areg (regs, 7) + 4 * (x + 1)))
static uae_u32 bsdsocklib_demux (void)
{
if ((ARG (0) > 0) && (ARG (0) < 30)) {
    DEBUG_LOG ("bsdsocklib_demux: %s\n",
	bsfn [ARG (0)]);
} else DEBUG_LOG ("bsdsocklib_demux: %d\n", ARG (0));
    switch (ARG (0)) {
      case 1:  return bsdsocklib_Init (ARG (1), ARG (2));
      case 2:  return bsdsocklib_Socket (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5));
      case 3:  return bsdsocklib_Bind (ARG (1), ARG (2), ARG (3), ARG (4));
      case 4:  return bsdsocklib_Listen (ARG (1), ARG (2), ARG (3));
      case 5:  return bsdsocklib_Accept (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5));
      case 6:  return bsdsocklib_Connect (ARG (1), ARG (2), ARG (3), ARG (4));
      case 8:  return bsdsocklib_Sendto (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5), ARG (6), ARG (7));
      case 9:  return bsdsocklib_Recvfrom (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5), ARG (6), ARG (7));
      case 10: return bsdsocklib_Setsockopt (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5), ARG (6));
      case 11: return bsdsocklib_CloseSocket (ARG (1), ARG (2));
      case 12: return bsdsocklib_WaitSelect (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5), ARG (6));
      case 13: return bsdsocklib_abortSomething (ARG (1));
      case 14: return bsdsocklib_Gethostbyname (ARG (1), ARG (2), ARG (3));
      case 15: return bsdsocklib_Getprotobyname (ARG (1), ARG (2), ARG (3));
      case 16: return bsdsocklib_Getservbyname (ARG (1), ARG (2), ARG (3), ARG (4));
      case 17: return bsdsocklib_Gethostname (ARG (1), ARG (2), ARG (3));
      case 18: return bsdsocklib_Inet_addr (ARG (1), ARG (2));
      case 19: return bsdsocklib_Getsockpeername (ARG (1), ARG (2), ARG (3), ARG (4), 1);
      case 20: return bsdsocklib_Inet_NtoA (ARG (1), ARG (2), ARG (3));
      case 21: return bsdsocklib_SocketBaseTagList (ARG (1), ARG (2));
      case 22: return bsdsocklib_Shutdown (ARG (1), ARG (2), ARG (3));
      case 23: return bsdsocklib_findfreesocket (ARG (1), ARG (2));
      case 24: return bsdsocklib_Dup2Socket (ARG (1), ARG (2), ARG (3));
      case 25: return bsdsocklib_Getsockopt (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5), ARG (6));
      case 26: return bsdsocklib_Getsockpeername (ARG (1), ARG (2), ARG (3), ARG (4), 0);
      case 27: return bsdsocklib_Gethostbyaddr (ARG (1), ARG (2), ARG (3), ARG (4), ARG (5));
      case 28: return bsdsocklib_IoctlSocket (ARG (1), ARG (2), ARG (3), ARG (4));
case 1000: DEBUG_LOG ("### Amiga says: 0x%x 0x%x 0x%x   0x%x\n", ARG (1), ARG (2), ARG (3), ARG (4)); break;
case 1001: DEBUG_LOG ("### Amiga says: '%s'\n", get_real_address (ARG (1))); break;
      default: write_log ("BSDSOCK: Unknown trap #%d.\n", ARG (0));
    }
    return 0;
}

void bsdlib_install (void)
{
int foo;
    uaecptr a = here ();
    org (0xF0FFB0);
    calltrap (deftrap (bsdsocklib_demux));
    dw (RTS);
    org (a);          

/* @@@ Error-checking */
    pthread_key_create (&global_key, NULL);
    signal (SIGUSR1, killthread_sighandler);
foo=(int)    signal (SIGIO, sigio_sighandler);
DEBUG_LOG ("bsdlib_install: SIGIO was %d\n",foo);
    bsdlib_reset ();
}

/* @@@ This should do something */
void bsdlib_reset (void)
{
}
