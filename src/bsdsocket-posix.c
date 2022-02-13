 /*
  * UAE - The Un*x Amiga Emulator
  *
  * bsdsocket.library emulation - Unix
  *
  * Copyright 2000-2001 Carl Drougge <carl.drougge@home.se> <bearded@longhaired.org>
  * Copyright 2003-2004 Richard Drummond
  * Copyright 2004      Jeff Shepherd
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

#ifdef BSDSOCKET
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <signal.h>
#include <arpa/inet.h>
#endif

//#define DEBUG_BSDSOCKET
#ifdef DEBUG_BSDSOCKET
#define DEBUG_LOG write_log
#else
#define DEBUG_LOG(...) do {;} while(0)
#endif

void bsdlib_install (void);
void bsdlib_reset (void);

#ifdef BSDSOCKET

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
    uae_thread_id thread;
    uae_sem_t   sem;
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
    int         abort_socket [2];
};

static void clear_abort_sockets (struct bsd_gl_o *gl_o);

/*
 * Cheap and tacky replacement for thread-local storage
 *
 * This is used in signal handlers to locate the global
 * struct corresponding to the current thread.
 */
struct global_key {
    uae_thread_id id;
    uae_u32       global;
};

#define MAX_KEYS 64

static int               keys_used;
static struct global_key key_array[MAX_KEYS];
static uae_sem_t         key_sem;

static int add_key (uae_u32 global)
{
    int result = 0;
    uae_thread_id id = uae_thread_self ();

    uae_sem_wait (&key_sem);
    if (keys_used < MAX_KEYS) {
	key_array[keys_used].id     = id;
	key_array[keys_used].global = global;
	keys_used++;
	result = 1;
    }
    uae_sem_post (&key_sem);
    return result;
}

static void rem_key (void)
{
    int i;
    uae_thread_id id = uae_thread_self ();

    uae_sem_wait (&key_sem);
    for (i = keys_used - 1; i; i--) {
	if (key_array[i].id == id) {
	    int j;
	    for (j = i+1; j<keys_used; j++)
		key_array[j - 1] = key_array[j];
	    keys_used--;
	    break;
	}
    }
    uae_sem_post (&key_sem);
}

static uae_u32 get_key (void)
{
    int i;
    uae_u32 global = 0;
    uae_thread_id id = uae_thread_self ();

    uae_sem_wait (&key_sem);
    for (i = keys_used -1; i; i--) {
	if (key_array[i].id == id) {
	    global = key_array[i].global;
	    break;
	}
    }
    uae_sem_post (&key_sem);
    return global;
}

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

static int __bsd_findfreesocket (int first, const struct bsd_gl_o *gl_o)
{
    int i;
    for (i = first; i < gl_o->dtablesize; i++) {
        if (sock_getsock (i, gl_o) == -1) {
            return i;
        }
    }
    return -1;
}

#define bsd_findfreesocket(o) __bsd_findfreesocket (0, o)
#define bsdsock_Signal(task, sig) uae_Signal (task, sig)

uae_u32 bsdsocklib_Init (uae_u32 global, uae_u32 init);
uae_u32 bsdsocklib_Socket (int domain, int type, int protocol, uae_u32 s, uae_u32 global);
uae_u32 bsdsocklib_Connect (uae_u32 s, uae_u32 a_addr , unsigned int addrlen, uae_u32 global);
uae_u32 bsdsocklib_Sendto (uae_u32 s, uae_u32 msg, uae_u32 len, uae_u32 flags, uae_u32 to, uae_u32 tolen, uae_u32 global);
uae_u32 bsdsocklib_Recvfrom (uae_u32 s, uae_u32 buf, uae_u32 len, uae_u32 flags, uae_u32 from, uae_u32 fromlen, uae_u32 global);
uae_u32 bsdsocklib_Setsockopt (uae_u32 s, uae_u32 level, uae_u32 optname, uae_u32 optval, uae_u32 optlen, uae_u32 global);
uae_u32 bsdsocklib_CloseSocket (uae_u32 s, uae_u32 global);
uae_u32 bsdsocklib_Gethostbyname (uae_u32 name, uae_u32 hostent, uae_u32 global);
uae_u32 bsdsocklib_WaitSelect (int nfds, uae_u32 readfds, uae_u32 writefds, uae_u32 exceptfds, uae_u32 timeout, uae_u32 global);
uae_u32 bsdsocklib_abortSomething (uae_u32 global);
uae_u32 bsdsocklib_Bind (uae_u32 s, uae_u32 name, int a_namelen, uae_u32 global);
uae_u32 bsdsocklib_Listen (uae_u32 s, int backlog, uae_u32 global);
uae_u32 bsdsocklib_Accept (uae_u32 s, uae_u32 addr, uae_u32 addrlen, uae_s32 news, uae_u32 global);
uae_u32 bsdsocklib_Getprotobyname (uae_u32 name, uae_u32 a_p, uae_u32 global);
uae_u32 bsdsocklib_Getservbyname (uae_u32 name, uae_u32 proto, uae_u32 a_s, uae_u32 global);
uae_u32 bsdsocklib_Gethostname (uae_u32 name, uae_u32 namelen, uae_u32 global);
uae_u32 bsdsocklib_Inet_addr (uae_u32 cp, uae_u32 global);
uae_u32 bsdsocklib_Inet_NtoA (uae_u32 in, uae_u32 buf, uae_u32 global);
uae_u32 bsdsocklib_SocketBaseTagList (uae_u32 tags, uae_u32 global);
uae_u32 bsdsocklib_Shutdown (uae_u32 s, uae_u32 how, uae_u32 global);
uae_u32 bsdsocklib_findfreesocket (uae_u32 first, uae_u32 global);
uae_u32 bsdsocklib_Dup2Socket (uae_s32 fd1, uae_s32 fd2, uae_u32 global);
uae_u32 bsdsocklib_Getsockopt (uae_u32 s, uae_u32 level, uae_u32 optname, uae_u32 optval, uae_u32 optlen, uae_u32 global);
uae_u32 bsdsocklib_Getsockpeername (uae_u32 s, uae_u32 name, uae_u32 namelen, uae_u32 global, int which);
uae_u32 bsdsocklib_Gethostbyaddr (uae_u32 addr, uae_u32 len, uae_u32 type, uae_u32 hostent, uae_u32 global);
uae_u32 bsdsocklib_IoctlSocket (uae_u32 s, uae_u32 action, uae_u32 argp, uae_u32 global);

/*
 * Map amiga (s|g)etsockopt level into native one
 */
static int mapsockoptlevel (int level)
{
    switch (level) {
	case 0xffff:
	    return SOL_SOCKET;
	case 0:
	    return IPPROTO_IP;
	case 1:
	    return IPPROTO_ICMP;
	case 2:
	    return IPPROTO_IGMP;
#ifdef IPPROTO_IPIP
	case 4:
	    return IPPROTO_IPIP;
#endif
	case 6:
	    return IPPROTO_TCP;
	case 8:
	    return IPPROTO_EGP;
	case 12:
	    return IPPROTO_PUP;
	case 17:
	    return IPPROTO_UDP;
	case 22:
	    return IPPROTO_IDP;
#ifdef IPPROTO_TP
	case 29:
	    return IPPROTO_TP;
#endif
	case 98:
	    return IPPROTO_ENCAP;
	default:
	    DEBUG_LOG ("Unknown sockopt level %d\n", level);
	    return level;
    }
}

/*
 * Map amiga (s|g)etsockopt optname into native one
 */
static int mapsockoptname (int level, int optname)
{
    switch (level) {

	case SOL_SOCKET:
	    switch (optname) {
		case 0x0001:
		    return SO_DEBUG;
		case 0x0002:
		    return SO_ACCEPTCONN;
		case 0x0004:
		    return SO_REUSEADDR;
		case 0x0008:
		    return SO_KEEPALIVE;
		case 0x0010:
		    return SO_DONTROUTE;
		case 0x0020:
		    return SO_BROADCAST;
#ifdef SO_USELOOPBACK
		case 0x0040:
		    return SO_USELOOPBACK;
#endif
		case 0x0080:
		    return SO_LINGER;
		case  0x0100:
		    return SO_OOBINLINE;
#ifdef SO_REUSEPORT
		case 0x0200:
		    return SO_REUSEPORT;
#endif
		case 0x1001:
		    return SO_SNDBUF;
		case 0x1002:
		    return SO_RCVBUF;
		case 0x1003:
		    return SO_SNDLOWAT;
		case 0x1004:
		    return SO_RCVLOWAT;
		case 0x1005:
		    return SO_SNDTIMEO;
		case 0x1006:
		    return SO_RCVTIMEO;
		case 0x1007:
		    return SO_ERROR;
		case 0x1008:
		    return SO_TYPE;

		default:
		    DEBUG_LOG ("Invalid setsockopt option %x for level %d\n",
			       optname, level);
		    return -1;
	    }
	    break;

	case IPPROTO_IP:
	    switch (optname) {
		case 1:
		    return IP_OPTIONS;
		case 2:
		    return IP_HDRINCL;
		case 3:
		    return IP_TOS;
		case 4:
		    return IP_TTL;
		case 5:
		    return IP_RECVOPTS;
		case 6:
		    return IP_RECVRETOPTS;
		case 8:
		    return IP_RETOPTS;
		case 9:
		    return IP_MULTICAST_IF;
		case 10:
		    return IP_MULTICAST_TTL;
		case 11:
		    return IP_MULTICAST_LOOP;
		case 12:
		    return IP_ADD_MEMBERSHIP;

		default:
		    DEBUG_LOG ("Invalid setsockopt option %x for level %d\n",
			       optname, level);
		    return -1;
	    }
	    break;

	case IPPROTO_TCP:
	    switch (optname) {
		case 1:
		    return TCP_NODELAY;
		case 2:
		    return TCP_MAXSEG;

		default:
		    DEBUG_LOG ("Invalid setsockopt option %x for level %d\n",
			       optname, level);
		    return -1;
	    }
	    break;

        default:
	    DEBUG_LOG ("Unknown level %d\n", level);
	    return -1;
    }
}

/*
 * map host errno to amiga errno
 */
static int mapErrno (int e)
{
    switch (e) {
	case EINTR:             e = 4;  break;
	case EDEADLK:		e = 11; break;
	case EAGAIN:		e = 35; break;
	case EINPROGRESS:	e = 36; break;
	case EALREADY:		e = 37; break;
	case ENOTSOCK:		e = 38; break;
	case EDESTADDRREQ:	e = 39; break;
	case EMSGSIZE:		e = 40; break;
	case EPROTOTYPE:	e = 41; break;
	case ENOPROTOOPT:	e = 42; break;
	case EPROTONOSUPPORT:	e = 43; break;
	case ESOCKTNOSUPPORT:	e = 44; break;
	case EOPNOTSUPP:	e = 45;	break;
	case EPFNOSUPPORT:	e = 46; break;
	case EAFNOSUPPORT:	e = 47; break;
	case EADDRINUSE:	e = 48; break;
	case EADDRNOTAVAIL:	e = 49; break;
	case ENETDOWN:		e = 50; break;
	case ENETUNREACH:	e = 51; break;
	case ENETRESET:		e = 52; break;
	case ECONNABORTED:	e = 53; break;
	case ECONNRESET:	e = 54; break;
	case ENOBUFS:		e = 55; break;
	case EISCONN:		e = 56; break;
	case ENOTCONN:		e = 57; break;
	case ESHUTDOWN:		e = 58; break;
	case ETOOMANYREFS:	e = 59; break;
	case ETIMEDOUT:		e = 60; break;
	case ECONNREFUSED:	e = 61; break;
	case ELOOP:		e = 62; break;
	case ENAMETOOLONG:	e = 63; break;
	default: break;
    }
    return e;
}

/*
 * Set amiga-side errno
 */
static void __S_GL_errno (uae_u32 global, int errnum)
{
    int size = get_long (global + 36);
    int e    = mapErrno (errnum);
    put_long (global + 28, e);
    switch (size) {
	case 1: put_byte (get_long (global + 32), e); break;
	case 2: put_word (get_long (global + 32), e); break;
	case 4: put_long (get_long (global + 32), e); break;
    }
}

/*
 * Manipulate amiga-side socket sets
 */
static int bsd_amigaside_FD_ISSET (int n, uae_u32 set)
{
    uae_u32 foo = get_long (set + (n / 32));
    if (foo & (1 << (n % 32)))
	return 1;
    return 0;
}

static void bsd_amigaside_FD_ZERO (uae_u32 set)
{
    put_long (set, 0);
    put_long (set + 4, 0);
}

static void bsd_amigaside_FD_SET (int n, uae_u32 set)
{
    set = set + (n / 32);
    put_long (set, get_long (set) | (1 << (n % 32)));
}



static void sock_init (int s, struct bsd_gl_o *gl_o)
{
    gl_o->sockets [s].s         = -1;
    gl_o->sockets [s].events    = 0;
    gl_o->sockets [s].eventmask = 0;
}

static int sock_close (int s, struct bsd_gl_o *gl_o)
{
    int r = 0;
    int rs;

    if ((rs = sock_getsock (s, gl_o)) >= 0) {
        DEBUG_LOG ("sock_close amiga=%d native=%d\n", s, rs);
        r = close (rs);

    if (r == 0) sock_setsock (-1, s, gl_o);
       else DEBUG_LOG ("Error:%d\n", errno);
    }
    return r;
}

/*
 * Map amiga sockaddr to host sockaddr
 */
static int copysockaddr_a2n (struct sockaddr_in *addr, uae_u32 a_addr,
			     unsigned int len)
{
    if ((len > sizeof (struct sockaddr_in)) || (len < 8))
	return 1;

    addr->sin_family      = get_byte (a_addr + 1);
    addr->sin_port        = htons (get_word (a_addr + 2));
    addr->sin_addr.s_addr = htonl (get_long (a_addr + 4));

    if (len > 8)
	memcpy (&addr->sin_zero, get_real_address (a_addr + 8), len - 8);   /* Pointless? */

    return 0;
}

/*
 * Map host sockaddr to amiga sockaddr
 */
static int copysockaddr_n2a (uae_u32 a_addr, const struct sockaddr_in *addr,
			     unsigned int len)
{
    if (len < 8)
	return 1;

    put_byte (a_addr, 0);                       /* Anyone use this field? */
    put_byte (a_addr + 1, addr->sin_family);
    put_word (a_addr + 2, ntohs (addr->sin_port));
    put_long (a_addr + 4, ntohl (addr->sin_addr.s_addr));

    if (len > 8)
	memset (get_real_address (a_addr + 8), 0, len - 8);

    return 0;
}



static uae_u32 bsdthr_WaitSelect (struct bsd_gl_o *gl_o)
{
    fd_set sets [3];
    int i, s, max = 0, set, a_s;
    uae_u32 a_set;
    struct timeval tv;
    int r;

    DEBUG_LOG ("WaitSelect: %d 0x%x 0x%x 0x%x 0x%x\n",
		gl_o->nfds, gl_o->sets [0], gl_o->sets [1],
		gl_o->sets [2], gl_o->timeout);

    if (gl_o->timeout)
	DEBUG_LOG ("WaitSelect: timeout %d %d\n",
		   get_long (gl_o->timeout), get_long (gl_o->timeout + 4));

    FD_ZERO (&sets [0]);
    FD_ZERO (&sets [1]);
    FD_ZERO (&sets [2]);

    /* Set up the abort socket */
    FD_SET (gl_o->abort_socket[0], &sets[0]);
    FD_SET (gl_o->abort_socket[0], &sets[2]);
    max = gl_o->abort_socket[0];

    /* Map the Amiga sockets sets to host sets */
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

    if (r > 0 ) {
	/* Got an abort signal - this probably means that
	 * that the Amiga side of WaitSelect received the
	 * signals that it is waiting on */
	if (FD_ISSET (gl_o->abort_socket[0], &sets[0])) {
	    int chr;
	    /* read from the abortsocket to reset it */
	    DEBUG_LOG ("WaitSelect: Aborted from signal\n");
	    read (gl_o->abort_socket[0], &chr, sizeof(chr));

	    r = 0;

	    for (set = 0; set < 3; set++)
		if (gl_o->sets [set] != 0)
		    bsd_amigaside_FD_ZERO (gl_o->sets [set]);
	} else {
	    /* This is perhaps slightly inefficient, but I don't care.. */
	    for (set = 0; set < 3; set++) {
		a_set = gl_o->sets [set];
		if (a_set != 0) {
		    bsd_amigaside_FD_ZERO (a_set);
		    for (i = 0; i < gl_o->nfds; i++) {
			a_s = sock_getsock (i, gl_o);
			if (a_s != -1) {
			    if (FD_ISSET (a_s, &sets [set]))
				bsd_amigaside_FD_SET (i, a_set);
			}
		    }
		}
	    }
	}
    } else
	if (r == 0) {
	    /* Timeout. I think we're supposed to clear the sets.. */
	    for (set = 0; set < 3; set++)
		if (gl_o->sets [set] != 0)
		    bsd_amigaside_FD_ZERO (gl_o->sets [set]);
	}

    DEBUG_LOG ("WaitSelect: %d\n", r);

    return r;
}

static uae_u32 bsdthr_Accept_2 (int a_s, struct bsd_gl_o *gl_o)
{
    int i, foo, s;
    long flags;

    foo = sizeof (struct sockaddr_in);
    if ((s = accept(a_s, (struct sockaddr *)&gl_o->addr, &foo)) >= 0) {
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

static uae_u32 bsdthr_Recv_2 (int s, struct bsd_gl_o *gl_o)
{
    int result;

    DEBUG_LOG ("Recv[from]: socket=%d len=%d -> ", s, gl_o->len);

    if (gl_o->from == 0) {
        result = recv (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL);
    } else {
	int l = sizeof (struct sockaddr_in);
	int i = get_long (gl_o->fromlen);
	copysockaddr_a2n (&gl_o->addr, gl_o->from, i);

	result = recvfrom (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL,
			   (struct sockaddr *)&gl_o->addr, &l);
	if (result >= 0) {
	    copysockaddr_n2a (gl_o->from, &gl_o->addr, l);
	    if (i > 16)
		put_long (gl_o->fromlen, 16);
	}
    }

    DEBUG_LOG ("result=%d errno=%d\n", result, errno);

    return result;
}

static uae_u32 bsdthr_Send_2 (int s, struct bsd_gl_o *gl_o)
{
    if (gl_o->to == 0) {
        return send (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL);
    } else {
        int l = sizeof (struct sockaddr_in);
        copysockaddr_a2n (&gl_o->addr, gl_o->to, gl_o->tolen);
        return sendto (s, gl_o->buf, gl_o->len, gl_o->flags | MSG_NOSIGNAL, (struct sockaddr *)&gl_o->addr, l);
    }
}

static uae_u32 bsdthr_Connect_2 (int s, struct bsd_gl_o *gl_o)
{
    if (gl_o->action == 1) {
	int retval;
	gl_o->action = -1;
	retval = connect (s, (struct sockaddr *)&gl_o->addr, gl_o->addrlen);
	DEBUG_LOG ("Connect returns %d, errno %d\n", retval, errno);
	return retval;
    } else {
	int foo, bar;
	bar = sizeof (foo);
	if (getsockopt (s, SOL_SOCKET, SO_ERROR, &foo, &bar) == 0) {
	    DEBUG_LOG ("Connect error %d\n", foo);
	    errno = foo;
	}
	return 0;       /* We have to return 0 to get out of bsdthr_blockingstuff () */
    }
}

#define BLOCKSTUFF_READ  1
#define BLOCKSTUFF_WRITE 2
#define BLOCKSTUFF_READWRITE  (BLOCKSTUFF_READ | BLOCKSTUFF_WRITE)

/*
 * Call socket function tryfunc and, if the function would block,
 * make the socket non-blocking and emulate the block by waiting
 * for socket activity with a select.
 */
static uae_u32 bsdthr_blockingstuff (int s, uae_u32 (*tryfunc)(int, struct bsd_gl_o *),
				     struct bsd_gl_o *gl_o, int readwrite)
{
    int done = 0, foo;
    int aborted = 0;
    long flags;

    /* Make the socket non-blocking */
    if ((flags = fcntl (s, F_GETFL)) == -1)
	flags = 0;
    fcntl (s, F_SETFL, flags | O_NONBLOCK);

    while (!done) {
        done = 1;

        /* Call our socket function */
	foo = tryfunc (s, gl_o);

        /* Would the socket block? */
	if ((foo < 0) && !(flags & O_NONBLOCK)) {
	    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINPROGRESS)) {
	        /* Yes - do the select */
		int max;
	        int nfds;
		fd_set readset;
		fd_set writeset;
        	FD_ZERO (&readset);
		FD_ZERO (&writeset);

		/* Set up the abort socket */
		FD_SET (gl_o->abort_socket[0], &readset);
		max = gl_o->abort_socket[0];

		if (readwrite & BLOCKSTUFF_READ)
		    FD_SET (s, &readset);
		if (readwrite & BLOCKSTUFF_WRITE)
		    FD_SET (s, &writeset);
		if ( s > max)
		    max = s;

		DEBUG_LOG ("Doing select on socket %d -> ", s);

		if ((nfds = select (max + 1, &readset, &writeset, NULL, NULL)) == -1) {
		    DEBUG_LOG ("got error %d\n", errno);
		    fcntl (s, F_SETFL, flags);
		    return -1;
		}

		if (FD_ISSET (gl_o->abort_socket[0], &readset)) {
		    /* We received an abort signal */
		    int chr;

		    DEBUG_LOG ("aborted from Amiga side\n");

		    /* read from abort_socket to reset it */
		    read (gl_o->abort_socket[0], &chr, sizeof(chr));
		    aborted = 1;
		    foo = -1;
		} else {
		    /* We got socket activity - do the loop
		     * again - and hence retry our socket
		     * function */
		    done = 0;
		    DEBUG_LOG ("done");
		}
	    }
	}
    }
    /* Restore original socket flags */
    fcntl (s, F_SETFL, flags);

    if (aborted)
	errno = EINTR;

    return foo;
}

static uae_u32 bsdthr_SendRecvAcceptConnect (uae_u32 (*tryfunc)(int, struct bsd_gl_o *), struct bsd_gl_o *gl_o, int readwrite)
{
    int s = sock_getsock (gl_o->s, gl_o);
    if (s < 0) {
        errno = EBADF;
        return -1;
    }
    return bsdthr_blockingstuff (s, tryfunc, gl_o, readwrite);
}

static void *bsdlib_threadfunc (void *arg)
{
    uae_u32 global = (uae_u32) arg;
    struct bsd_gl_o *gl_o = GL_O;
    struct hostent *hostent;
    int foo, s, i, l;

    DEBUG_LOG ("THREAD_START\n");
    add_key (global);
    while (1) {
        uae_sem_wait (&gl_o->sem);
        switch (gl_o->action) {
          case 0:       /* kill thread (CloseLibrary) */
            for(i = 0; i < 64; i++) {           /* Are dtable is always 64, we just claim otherwise if it makes the app happy. */
                sock_close (i, gl_o);
            }
/* @@@ We should probably have a signal for this too, since it can block.. */
/*     Harmless though. */
            DEBUG_LOG ("THREAD_DEAD\n");
	    rem_key ();
            uae_sem_destroy (&gl_o->sem);
            free (gl_o);
            return NULL;
          case 1:       /* Connect */
            bsdthr_SendRecvAcceptConnect (bsdthr_Connect_2, gl_o, BLOCKSTUFF_WRITE);      /* Only returns -1 for aborts, errors give 0. */
            S_GL_result ((errno == 0) ? 0 : -1);
            break;
/* @@@ Should check (from|to)len so it's 16.. */
          case 2:       /* Send[to] */
            S_GL_result (bsdthr_SendRecvAcceptConnect (bsdthr_Send_2, gl_o, BLOCKSTUFF_WRITE));
            break;
          case 3:       /* Recv[from] */
            S_GL_result (bsdthr_SendRecvAcceptConnect (bsdthr_Recv_2, gl_o, BLOCKSTUFF_READ));
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
            S_GL_result (bsdthr_SendRecvAcceptConnect (bsdthr_Accept_2, gl_o, BLOCKSTUFF_READ));
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
    clear_abort_sockets (gl_o);
    return NULL;        /* Just to keep GCC happy.. */
}

static void sigio_sighandler (int sig)
{
    uae_u32 global = (uae_u32) get_key ();

    if (global) {
	if (GL_iomask)
	    bsdsock_Signal (GL_task, GL_iomask);
    }
    write_log ("SIGIO: global 0x%x, 0x%x\n", global, global?GL_iomask:0);
}



static int init_abort_sockets (struct bsd_gl_o *gl_o)
{
    int result = -1;

    /* TODO: Handle the cases when socketpair and or PF_UNIX
     * sockets are not available */
    if (socketpair (PF_UNIX, SOCK_STREAM, 0, &gl_o->abort_socket[0]) == 0)
	if (fcntl (gl_o->abort_socket[0], F_SETFL, O_NONBLOCK) < 0)
	    write_log ("BSDSOCK: Set nonblock failed %d\n", errno);
	else
	    result = 0;
    else
	write_log ("BSDSOCK: Failed to create abort sockets\n");

    return result;
}

static void free_abort_sockets (struct bsd_gl_o *gl_o)
{
    close (gl_o->abort_socket[0]);
    close (gl_o->abort_socket[1]);
}

static void clear_abort_sockets (struct bsd_gl_o *gl_o)
{
    int chr;
    int num;

    while ((num = read (gl_o->abort_socket[0], &chr, sizeof(chr))) >= 0) {
	DEBUG_LOG ("Sockabort got %d bytes\n", num);
	;
    }
}

static void do_abort_sockets (struct bsd_gl_o *gl_o)
{
    int chr = 1;
    DEBUG_LOG ("Sock abort!!\n");
    write (gl_o->abort_socket[1], &chr, sizeof (chr));
}

uae_u32 bsdsocklib_Init (uae_u32 global, uae_u32 init)
{
    struct bsd_gl_o *gl_o;
    int i;

    if (init) {
	if ((GL_O = calloc (1, sizeof (struct bsd_gl_o))) != NULL) {
	    gl_o = GL_O;
	    if (uae_sem_init (&gl_o->sem, 0, 0) == 0) {
		if (init_abort_sockets (gl_o) == 0) {
		    if (uae_start_thread (bsdlib_threadfunc, (void *) global,
					  &gl_o->thread) == 0) {
			gl_o->dtablesize = 64;
			for (i = 0; i < gl_o->dtablesize; i++)
			    sock_init (i, gl_o);
		    } else {
			write_log ("BSDSOCK: Failed to create thread.\n");
			free_abort_sockets (gl_o);
		    }
		} else
		    uae_sem_destroy (&gl_o->sem);
	   } else {
		write_log ("BSDSOCK: Failed to create semaphore.\n");
		free (gl_o);
	   }
	} else
            write_log ("BSDSOCK: Failed to allocate memory.\n");
    } else {
	uae_thread_id thread;

	gl_o = GL_O;
	thread = gl_o->thread;
        gl_o->action = 0;
        uae_sem_post (&gl_o->sem);
        uae_wait_thread (thread);
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
        uae_sem_post (&gl_o->sem);
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
    uae_sem_post (&gl_o->sem);
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
    uae_sem_post (&gl_o->sem);
    return 1;
}

uae_u32 bsdsocklib_Setsockopt (uae_u32 amisock, uae_u32 level, uae_u32 optname,
			       uae_u32 optval, uae_u32 optlen, uae_u32 global)
{
    int s = sock_getsock (amisock, GL_O);
    int nativelevel = mapsockoptlevel (level);
    int result;

    if (s == -1) {
        S_GL_errno (EBADF);
	return -1;
    }

    result = setsockopt (s, nativelevel, mapsockoptname (nativelevel, optname),
    				 optval ? get_real_address (optval) : NULL, optlen);

    S_GL_errno (errno);

    DEBUG_LOG ("setsockopt: sock %d, level %d, 'name' %d(%d), len %d -> %d, %d\n",
	       s, level, optname, mapsockoptname (nativelevel, optname), optlen,
	       result, errno);

    return result;
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
    uae_sem_post (&gl_o->sem);
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
    uae_sem_post (&gl_o->sem);
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
    uae_sem_post (&gl_o->sem);
    return 1;
}

uae_u32 bsdsocklib_abortSomething (uae_u32 global)
{
    struct bsd_gl_o *gl_o = GL_O;

    int chr = 1;
    DEBUG_LOG ("Aborting select\n");
    write (gl_o->abort_socket[1], &chr, sizeof (chr));

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
    r = bind (rs, (struct sockaddr *)&addr, sizeof (struct sockaddr_in));
    S_GL_errno (errno);
    return r;
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
    uae_sem_post (&gl_o->sem);
    return 1;
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
        r = getsockname (rs, (struct sockaddr *)&addr, &nl);
    } else {
        r = getpeername (rs, (struct sockaddr *)&addr, &nl);
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
#ifdef O_ASYNC
	r = fcntl (sock, F_SETFL, argval ? flags | O_ASYNC : flags & ~O_ASYNC);
	return r;
#else
	/* O_ASYNC is only available on Linux and BSD systems */
	return fcntl (sock, F_GETFL);
#endif
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

#endif /* BSDSOCKET */

#define ARG(x) (get_long (m68k_areg (regs, 7) + 4 * (x + 1)))
static uae_u32 bsdsocklib_demux (void)
{
#ifdef BSDSOCKET
    if (currprefs.socket_emu != 0) {
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
    }
#endif /* BSDSOCKET */
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

#ifdef BSDSOCKET
    uae_sem_init (&key_sem, 0, 1);

    foo= (int) signal (SIGIO, sigio_sighandler);
    DEBUG_LOG ("bsdlib_install: SIGIO was %d\n", foo);
    bsdlib_reset ();
#endif
}

/* @@@ This should do something */
void bsdlib_reset (void)
{
}
