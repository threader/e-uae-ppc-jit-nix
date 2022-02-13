/* #@! EC-bugs. (libfunc-args) */

/*

    Amiga-side of bsdsocket-emu.
    Copyright 2000-2001 Carl Drougge <carl.drougge@home.se> <bearded@longhaired.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


    (I'm assuming an Amiga .library can be GPL without the applications using it
     being GPL.)
*/

LIBRARY 'bsdsocket.library' , 4 , 1 , 'bsdsocket.library 4.1' IS
        lSocket(A1,A0,D2,D1,D0)         ,
        lBind(A0,D1,D0)                 ,
        lListen(A0,D1,D0)               ,
        lAccept(A1,A0,D1,D0)            ,
        lConnect(A0,D1,D0)              ,
        lSendto(A2,A1,A0,D3,D2,D1,D0)   ,
        lSend(A1,A0,D2,D1,D0)           ,
        lRecvfrom(A2,A1,A0,D2,D1,D0)    ,
        lRecv(A1,A0,D2,D1,D0)           ,
        lShutdown(A0,D1,D0)             ,
        lSetsockopt(A2,A1,A0,D3,D2,D1,D0),
        lGetsockopt(A1,A0,D2,D1,D0)     ,
        lGetsockname(A1,A0,D1,D0)       ,
        lGetpeername(A1,A0,D1,D0)       ,
        lIoctlSocket(A0,D1,D0)          ,
        lCloseSocket(D0)                ,
        lWaitSelect(A3,A2,A1,A0,D3,D2,D1,D0),
        lSetSocketSignals(D0,D1,D2)     ,
        lGetdtablesize()                ,
        lObtainSocket(D0,D1,D2,D3)      ,
        lReleaseSocket(D0,D1)           ,
        lReleaseCopyOfSocket(D0,D1)     ,
        lErrno()                        ,
        lSetErrnoPtr(A0,D0)             ,
        lInet_NtoA(D0)                  ,
        lInet_addr(A0)                  ,
        lInet_LnaOf(D0)                 ,
        lInet_NetOf(D0)                 ,
        lInet_MakeAddr(D0,D1)           ,
        lInet_network(A0)               ,
        lGethostbyname(A0)              ,
        lGethostbyaddr(A0,D1,D0)        ,
        lGetnetbyname(A0)               ,
        lGetnetbyaddr(D0,D1)            ,
        lGetservbyname(A1,A0,D1,D0)     ,
        lGetservbyport(D0,A0)           ,
        lGetprotobyname(A0)             ,
        lGetprotobynumber(D0)           ,
        lVsyslog(D0,A0,A1)              ,
        lDup2Socket(A0,D1,D0)           ,
        lSendmsg(D0,A0,D1)              ,
        lRecvmsg(D0,A0,D1)              ,
        lGethostname(A0,D0)             ,
        lGethostid()                    ,
        lSocketBaseTagList(A0)          ,
        lGetSocketEvents(A0)            ,
        afterlib , afterlib , afterlib


MODULE 'amitcp/netdb' , 'dos/dos' , 'dos/dostags' , 'amitcp/amitcp/socketbasetags' , 'amitcp/sys/errno'

OBJECT global
  otherside[ 4 ] : ARRAY OF LONG        -> Enough for a ptr on all CPUs?
  result                                -> offset 16
  task , signal
  errno
  extra_errno
  extra_errnosize
  hostent_addr[ 1 ] : ARRAY OF LONG     -> offset 40
  intrmask
  iomask
  urgmask
  eventmask
  fdcallback                            -> offset 60
  errortexts

-> Keep these at the end, native doesn't touch them (directly anyway)
  hostent : hostent
  hostent_name     [ 128 ] : ARRAY OF CHAR
  hostent_addr_list[   1 ] : ARRAY OF LONG
  protoent : protoent
  protoent_name[ 128 ] : ARRAY OF CHAR
  servent : servent
  servent_name[ 128 ] : ARRAY OF CHAR
  servent_proto[ 32 ] : ARRAY OF CHAR
  ntoa_buf[ 16 ] : ARRAY OF CHAR
ENDOBJECT

DEF trap = $F0FFB0
DEF signal_b = -1
DEF global : global

PROC lSocket( dum1 , dum2 , protocol , type , domain )
  DEF s , e = 0
  IF ( s := trap( global , findfreesock() , protocol , type , domain , 2 )) >= 0 THEN e := markused( s )
  IF e  -> Shouldn't be, but we're supposed to check..
    seterrno( e )
    lCloseSocket( s )
    RETURN -1
  ENDIF
ENDPROC s

PROC lBind( name , namelen , s ) IS trap( global , namelen , name , s , 3 );
PROC lListen( dum1 , backlog , s ) IS trap( global , backlog , s , 4 )

PROC lAccept( addrlen , addr , dum1 , s )
  DEF e = 0
  IF trap( global , findfreesock() , addrlen , addr , s , 5 ) THEN waitforit()
  IF global.result >= 0 THEN e := markused( global.result )
  IF e  -> Shouldn't be, but we're supposed to check..
    seterrno( e )
    lCloseSocket( s )
    RETURN -1
  ENDIF
ENDPROC global.result

PROC lConnect( addr , addrlen , s )
  IF trap( global , addrlen , addr , s , 6 ) THEN waitforit()
ENDPROC global.result

PROC lSend( dum1 , msg , flags , len , s )
  IF trap( global , 0 , NIL , flags , len , msg , s , 8 ) THEN waitforit()
ENDPROC global.result

PROC lRecv( dum1 , buf , flags , len , s )
  IF trap( global , 0 , NIL , flags , len , buf , s , 9 ) THEN waitforit()
ENDPROC global.result

PROC lSetsockopt( dum1 , dum2 , optval , optlen , optname , level , s )
ENDPROC trap( global , optlen , optval , optname , level , s , 10 )

PROC lCloseSocket( s )
  IF trap( global , s , 11 ) THEN waitforit()
  IF global.result = 0 THEN markunused( s )
ENDPROC global.result

PROC lWaitSelect( timeout , exceptfds , writefds , readfds , dum1 , dum2 , sigmask , nfds )
  IF trap( global , timeout , exceptfds , writefds , readfds , nfds , 12 )
    IF waitforit( sigmask )     -> A signal in sigmask arrived
      IF global.result < 0 THEN global.result := 0
    ENDIF
  ELSE
    IF sigmask THEN ^sigmask := 0
  ENDIF
ENDPROC global.result

PROC lGetdtablesize() IS 64
PROC lErrno() IS global.errno

PROC lSetErrnoPtr( ptr , size )
  global.extra_errno     := ptr
  global.extra_errnosize := size
ENDPROC

-> Both these return just the first addr and name.
PROC lGethostbyname( name )
  setup_hostent()
  IF trap( global , global.hostent , name , 14 ) THEN waitforit()
ENDPROC global.result

PROC lGethostbyaddr( addr , type , len )
  setup_hostent()
  IF trap( global , global.hostent , type , len , addr , 27 ) THEN waitforit()
ENDPROC global.result

PROC setup_hostent()
  -> Simplify life on the native-side a little, reset all ptrs here.
  global.hostent.name        := global.hostent_name
  global.hostent.aliases     := NIL
  global.hostent.addr_list   := global.hostent_addr_list
  global.hostent.addr_list[] := global.hostent_addr
  global.hostent.length      := 4
ENDPROC

PROC lGetprotobyname( name )
  global.protoent.name    := global.protoent_name
  global.protoent.aliases := [ NIL ] : LONG
ENDPROC trap( global , global.protoent , name , 15 )

PROC lGetservbyname( proto , name , dum1 , dum2 )
  global.servent.name    := global.servent_name
  global.servent.aliases := [ NIL ] : LONG
  global.servent.proto   := global.servent_proto
ENDPROC trap( global , global.servent , proto , name , 16 )

PROC lGethostname( name , len ) IS trap( global , len , name , 17 )
PROC lInet_addr( cp ) IS trap( global , cp , 18 )
PROC lGetsockname( namelen , name , dum1 , s ) IS trap( global , namelen , name , s , 19 )
PROC lGetpeername( namelen , name , dum1 , s ) IS trap( global , namelen , name , s , 26 )
PROC lInet_NtoA( in ) IS trap( global , global.ntoa_buf , in , 20 )
PROC lSocketBaseTagList( tags ) IS trap( global , tags , 21 )
PROC lShutdown( dum1 , how , s ) IS trap( global, how , s , 22 )

PROC lDup2Socket( dum1 , fd2 , fd1 )
  DEF e
  IF fd1 = -1
    IF fd2 = -1
      seterrno( EMFILE )
      RETURN -1
    ENDIF
    e := markused( fd2 )
    IF e THEN RETURN seterrno( e ) BUT -1
    fd1 := trap( global , fd2 , fd1 , 24 )
    IF fd1 = -1 THEN markunused( fd2 )
    RETURN fd1
  ENDIF
  IF fd2 = -1
    IF ( fd2 := findfreesock() ) = -1
      seterrno( EMFILE )
      RETURN -1
    ENDIF
  ENDIF
  fd1 := trap( global , fd2 , fd1 , 24 )
  IF ( fd1 <> fd2 ) THEN markunused( fd2 )
ENDPROC fd1

PROC lSendto( dum1 , to , msg , tolen , flags , len , s )
  IF trap( global , tolen , to , flags , len , msg , s , 8 ) THEN waitforit()
ENDPROC global.result

PROC lRecvfrom( fromlen , from , buf , flags , len , s )
  IF trap( global , fromlen , from , flags , len , buf , s , 9 ) THEN waitforit()
ENDPROC global.result

PROC lGetsockopt( optlen , optval , optname , level , s ) IS trap( global , optlen , optval , optname , level , s , 25 )

PROC lIoctlSocket( argp , request , s ) IS trap( global , argp , request , s , 28 )

-> @@@ Error-checking!
PROC main()
  DEF foo : PTR TO LONG

  global.signal := Shl( 1 , signal_b := AllocSignal( -1 ))
  global.task   := FindTask( NIL )
  global.errno  := 0
  global.extra_errnosize := 0
  global.intrmask   := SIGBREAKF_CTRL_C
  global.iomask     := 0
  global.urgmask    := 0
  global.eventmask  := 0
  global.fdcallback := 0
-> These are taken from the win-version, maybe they're correct, maybe not.
  global.errortexts := [ 'No error' ,
                         'Operation not permitted' ,
                         'No such file or directory' ,
                         'No such process' ,
                         'Interrupted system call' ,
                         'Input/output error' ,
                         'Device not configured' ,
                         'Argument list too long' ,
                         'Exec format error' ,
                         'Bad file descriptor' ,
                         'No child processes' ,
                         'Resource deadlock avoided' ,
                         'Cannot allocate memory' ,
                         'Permission denied' ,
                         'Bad address' ,
                         'Block device required' ,
                         'Device busy' ,
                         'Object exists' ,
                         'Cross-device link' ,
                         'Operation not supported by device' ,
                         'Not a directory' ,
                         'Is a directory' ,
                         'Invalid argument' ,
                         'Too many open files in system' ,
                         'Too many open files' ,
                         'Inappropriate ioctl for device' ,
                         'Text file busy' ,
                         'File too large' ,
                         'No space left on device' ,
                         'Illegal seek' ,
                         'Read-only file system' ,
                         'Too many links' ,
                         'Broken pipe' ,
                         'Numerical argument out of domain' ,
                         'Result too large' ,
                         'Resource temporarily unavailable' ,
                         'Operation now in progress' ,
                         'Operation already in progress' ,
                         'Socket operation on non-socket' ,
                         'Destination address required' ,
                         'Messbge too long' ,
                         'Protocol wrong type for socket' ,
                         'Protocol not available' ,
                         'Protocol not supported' ,
                         'Socket type not supported' ,
                         'Operation not supported' ,
                         'Protocol family not supported' ,
                         'Address family not supported by protocol family' ,
                         'Address already in use' ,
                         'Can''t assign requested address' ,
                         'Network is down' ,
                         'Network is unreachable' ,
                         'Network dropped connection on reset' ,
                         'Software caused connection abort' ,
                         'Connection reset by peer' ,
                         'No buffer space available' ,
                         'Socket is already connected' ,
                         'Socket is not connected' ,
                         'Can''t send after socket shutdown' ,
                         'Too many references: can''t splice' ,
                         'Connection timed out' ,
                         'Connection refused' ,
                         'Too many levels of symbolic links' ,
                         'File name too long' ,
                         'Host is down' ,
                         'No route to host' ,
                         'Directory not empty' ,
                         'Too many processes' ,
                         'Too many users' ,
                         'Disc quota exceeded' ,
                         'Stale NFS file handle' ,
                         'Too many levels of remote in path' ,
                         'RPC struct is bad' ,
                         'RPC version wrong' ,
                         'RPC prog. not avail' ,
                         'Program version wrong' ,
                         'Bad procedure for program' ,
                         'No locks available' ,
                         'Function not implemented' ,
                         'Inappropriate file type or format' ,
                         'PError 0' ,
                         'No error' ,
                         'Unknown host' ,
                         'Host name lookup failure' ,
                         'Unknown server error' ,
                         'No address associated with name' ]

  IF ( trap( 1 , global , 1 ) = 0 )     -> 0 if for the old one, which needs the poller. */
-> It seems uae_Signal() works in 0.8.14, so I'm getting rid of the poller. It sure sucked. =)
-> Nice'n'kludgy..
    foo := {alllibs}
    IF foo[1] = 0
      foo[] := dosbase            -> Not needed for the new poller.
-> Hmm.. this should just be a task (not a process).. (And use timer.device rather than Delay())
-> Or perhaps it should be at a much lower pri and not wait at all? (Now testing that. Should still be a task though..)
-> @@@ The poller tends to crash with illegal instruction (different instruction each time too).
->     This is probably something overwriting it. Anyone got any idea what?
      CreateNewProc( [ NP_ENTRY , {poller} , NP_NAME , 'bsdsocket_poller' , NP_PRIORITY , -128 , NIL ] )
      foo[1] := 1
    ENDIF
  ENDIF
ENDPROC
alllibs: LONG 0,0
poller:
  MOVEA.L $4.W,A6
  MOVE.L  #0,-(A7)
_loop:
  JSR     $F0FFB0
  TST.L   D0
  BEQ     _loop
  JSR     Signal(A6)
  BRA     _loop

/*  Old poller that calls Delay() all the time.
poller:
  MOVE.L  alllibs(PC),A5
  MOVEA.L $4.W,A4
  MOVE.L  #0,-(A7)
_loop:
  MOVE.L  A5,A6
  MOVEQ.L #1,D1
  JSR     Delay(A6)
_pollagain:
  JSR     $F0FFB0
  TST.L   D0
  BEQ     _loop
  MOVEA.L A4,A6
  JSR     Signal(A6)
  BRA     _pollagain
*/



-> Free stuff. The poller really shouldn't run when nothing is using it..
PROC close()
  trap( 0 , global , 1 )
  IF ( signal_b >= 0 ) THEN FreeSignal( signal_b )
ENDPROC

-> We should actually wait for sigurg too, I think. But since nothing handles that yet anyway..
PROC waitforit( sigmask_ptr = NIL )

  DEF sigs , sigmask

  sigmask := IF sigmask_ptr THEN ^sigmask_ptr ELSE 0

  sigs := Wait( global.signal OR sigmask OR global.intrmask )
  IF ( sigs AND global.signal )      -> The call completed. It's my opinion that all other signals were delivered later. =)  (For efficiency)
    IF( sigmask_ptr ) THEN ^sigmask_ptr := 0 
    IF ( sigs := sigs AND Not( global.signal )) THEN Signal( global.task , sigs )
  ELSE                               -> We got interrupted. Deliver stuff from intrmask, but not from sigmask.
trap(sigs,global.intrmask,sigmask,global.signal,1000)
    trap( global , 13 )
-> Are we supposed to deliver the sig if it's in both intrmask and sigmask?
    IF ( sigmask_ptr ) THEN ^sigmask_ptr := sigs AND sigmask
    IF ( sigs := sigs AND global.intrmask ) THEN Signal( global.task , sigs )
    Wait( global.signal )            -> Wait for the call to abort
    IF ( sigmask_ptr ) THEN RETURN ^sigmask_ptr         -> If we were interrupted by one of these, we return <> 0 to show it.
  ENDIF

ENDPROC FALSE







PROC lSetSocketSignals(a,b,c)   IS trap( 'SetSocketSignals' , 1001 )
PROC lObtainSocket(a,b,c,d)     IS trap( 'ObtainSocket' , 1001 )
PROC lReleaseSocket(a,b)        IS trap( 'ReleaseSocket' , 1001 )
PROC lReleaseCopyOfSocket(a,b)  IS trap( 'ReleaseCopyOfSocket' , 1001 )
PROC lInet_LnaOf(a)             IS trap( 'Inet_LnaOf' , 1001 )
PROC lInet_NetOf(a)             IS trap( 'Inet_NetOf' , 1001 )
PROC lInet_MakeAddr(a,b)        IS trap( 'Inet_MakeAddr' , 1001 )
PROC lInet_network(a)           IS trap( 'Inet_network' , 1001 )
PROC lGetnetbyname(a)           IS trap( 'Getnetbyname' , 1001 )
PROC lGetnetbyaddr(a,b)         IS trap( 'Getnetbyaddr' , 1001 )
PROC lGetprotobynumber(a)       IS trap( 'Getprotobynumber' , 1001 )
PROC lGetservbyport(a,b)        IS trap( 'Getservbyport' , 1001 )
PROC lVsyslog(a,b,c)            IS trap( 'Vsyslog' , 1001 )
PROC lSendmsg(a,b,c)            IS trap( 'Sendmsg' , 1001 )
PROC lRecvmsg(a,b,c)            IS trap( 'Recvmsg' , 1001 )
PROC lGethostid()               IS trap( 'Gethostid' , 1001 )
PROC lGetSocketEvents(a)        IS trap( 'GetSocketEvents' , 1001 )

-> Maybe there are hidden calls? Can't hurt to catch them..
PROC afterlib() IS trap( 'Call to function after library jumptable!' , 1001 )


PROC markunused( s )
  DEF cb , e = 0
  IF cb := global.fdcallback
    MOVE.L  s,D0
    MOVEQ.L #FDCB_FREE,D1
    e := cb()
  ENDIF
ENDPROC e

PROC markused( s )
  DEF cb , e = 0
  IF cb := global.fdcallback
    MOVE.L  s,D0
    MOVEQ.L #FDCB_ALLOC,D1
    e := cb()
  ENDIF
ENDPROC e

PROC findfreesock()
  DEF s = 0 , cb , ok
  cb := global.fdcallback
  REPEAT
    s := trap( global , s , 23 )
    IF s = -1 THEN RETURN -1
    ok := TRUE
    IF cb
      MOVE.L  s,D0
      MOVEQ.L #FDCB_CHECK,D1
      IF cb()
        ok := FALSE
        s++
      ENDIF
    ENDIF
  UNTIL ok
trap(cb,s,49,42,1000)
ENDPROC s

PROC seterrno( e )
  global.errno := e
  IF global.extra_errno
    IF global.extra_errnosize = 1
      PutChar( global.extra_errno , e )
    ELSEIF global.extra_errno = 2
      PutInt( global.extra_errno , e )
    ELSE
      PutLong( global.extra_errno , e )
    ENDIF
  ENDIF
ENDPROC

