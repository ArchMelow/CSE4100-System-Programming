/* 
 * myshell.c
 *
 * Updated 8/2014 droh: 
 *   - New versions of open_clientfd and open_listenfd are reentrant and
 *     protocol independent.
 *
 *   - Added protocol-independent inet_ntop and inet_pton functions. The
 *     inet_ntoa and inet_aton functions are obsolete.
 *
 * Updated 7/2014 droh:
 *   - Aded reentrant sio (signal-safe I/O) routines
 * 
 * Updated 4/2013 droh: 
 *   - rio_readlineb: fixed edge case bug
 *   - rio_readnb: removed redundant EINTR check
 */
/* $begin myshell.c */
#include "myshell.h"
#include <errno.h>
#define MAXARGS   128
/************************** 
 * Error-handling functions
 **************************/
/* $begin errorfuns */
/* $begin unixerror */
void unix_error(char *msg) /* Unix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
/* $end unixerror */

void posix_error(int code, char *msg) /* Posix-style error */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(code));
    exit(0);
}

void gai_error(int code, char *msg) /* Getaddrinfo-style error */
{
    fprintf(stderr, "%s: %s\n", msg, gai_strerror(code));
    exit(0);
}

void app_error(char *msg) /* Application error */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}
/* $end errorfuns */

void dns_error(char *msg) /* Obsolete gethostbyname error */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}


/*********************************************
 * Wrappers for Unix process control functions
 ********************************************/

/* $begin forkwrapper */
pid_t Fork(void) 
{
    pid_t pid;

    if ((pid = fork()) < 0)
	unix_error("Fork error");
    return pid;
}
/* $end forkwrapper */

void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
    if (execve(filename, argv, envp) < 0)
	unix_error("Execve error");
}

/* $begin wait */
pid_t Wait(int *status) 
{
    pid_t pid;

    if ((pid  = wait(status)) < 0)
	unix_error("Wait error");
    return pid;
}
/* $end wait */

pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
    pid_t retpid;

    if ((retpid  = waitpid(pid, iptr, options)) < 0) 
	unix_error("Waitpid error");
    return(retpid);
}

/* $begin kill */
void Kill(pid_t pid, int signum) 
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
	unix_error("Kill error");
}
/* $end kill */

void Pause() 
{
    (void)pause();
    return;
}

unsigned int Sleep(unsigned int secs) 
{
    return sleep(secs);
}

unsigned int Alarm(unsigned int seconds) {
    return alarm(seconds);
}
 
void Setpgid(pid_t pid, pid_t pgid) {
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0)
	unix_error("Setpgid error");
    return;
}

pid_t Getpgrp(void) {
    return getpgrp();
}

/************************************
 * Wrappers for Unix signal functions 
 ***********************************/

/* $begin sigaction */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}
/* $end sigaction */

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
	unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
	unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set)
{ 
    if (sigfillset(set) < 0)
	unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
	unix_error("Sigaddset error");
    return;
}

void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
	unix_error("Sigdelset error");
    return;
}

int Sigismember(const sigset_t *set, int signum)
{
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
	unix_error("Sigismember error");
    return rc;
}

int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* always returns -1 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/*************************************************************
 * The Sio (Signal-safe I/O) package - simple reentrant output
 * functions that are safe for signal handlers.
 *************************************************************/

/* Private sio functions */

/* $begin sioprivate */
/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    
    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);
    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* $end sioprivate */

/* Public Sio functions */
/* $begin siopublic */

ssize_t sio_puts(char s[]) /* Put string */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); //line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* Based on K&R itoa() */  //line:csapp:sioltoa
    return sio_puts(s);
}

void sio_error(char s[]) /* Put error message and exit */
{
    sio_puts(s);
    _exit(1);                                      //line:csapp:sioexit
}
/* $end siopublic */

/*******************************
 * Wrappers for the SIO routines
 ******************************/
ssize_t Sio_putl(long v)
{
    ssize_t n;
  
    if ((n = sio_putl(v)) < 0)
	sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;
  
    if ((n = sio_puts(s)) < 0)
	sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[])
{
    sio_error(s);
}

/********************************
 * Wrappers for Unix I/O routines
 ********************************/

int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}

ssize_t Read(int fd, void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = read(fd, buf, count)) < 0) 
	unix_error("Read error");
    return rc;
}

ssize_t Write(int fd, const void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = write(fd, buf, count)) < 0)
	unix_error("Write error");
    return rc;
}

off_t Lseek(int fildes, off_t offset, int whence) 
{
    off_t rc;

    if ((rc = lseek(fildes, offset, whence)) < 0)
	unix_error("Lseek error");
    return rc;
}

void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
	unix_error("Close error");
}

int Select(int  n, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout) 
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
	unix_error("Select error");
    return rc;
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}

void Stat(const char *filename, struct stat *buf) 
{
    if (stat(filename, buf) < 0)
	unix_error("Stat error");
}

void Fstat(int fd, struct stat *buf) 
{
    if (fstat(fd, buf) < 0)
	unix_error("Fstat error");
}

/*********************************
 * Wrappers for directory function
 *********************************/

DIR *Opendir(const char *name) 
{
    DIR *dirp = opendir(name); 

    if (!dirp)
        unix_error("opendir error");
    return dirp;
}

struct dirent *Readdir(DIR *dirp)
{
    struct dirent *dep;
    
    errno = 0;
    dep = readdir(dirp);
    if ((dep == NULL) && (errno != 0))
        unix_error("readdir error");
    return dep;
}

int Closedir(DIR *dirp) 
{
    int rc;

    if ((rc = closedir(dirp)) < 0)
        unix_error("closedir error");
    return rc;
}

/***************************************
 * Wrappers for memory mapping functions
 ***************************************/
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) 
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *) -1))
	unix_error("mmap error");
    return(ptr);
}

void Munmap(void *start, size_t length) 
{
    if (munmap(start, length) < 0)
	unix_error("munmap error");
}

/***************************************************
 * Wrappers for dynamic storage allocation functions
 ***************************************************/

void *Malloc(size_t size) 
{
    void *p;

    if ((p  = malloc(size)) == NULL)
	unix_error("Malloc error");
    return p;
}

void *Realloc(void *ptr, size_t size) 
{
    void *p;

    if ((p  = realloc(ptr, size)) == NULL)
	unix_error("Realloc error");
    return p;
}

void *Calloc(size_t nmemb, size_t size) 
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
	unix_error("Calloc error");
    return p;
}

void Free(void *ptr) 
{
    free(ptr);
}

/******************************************
 * Wrappers for the Standard I/O functions.
 ******************************************/
void Fclose(FILE *fp) 
{
    if (fclose(fp) != 0)
	unix_error("Fclose error");
}

FILE *Fdopen(int fd, const char *type) 
{
    FILE *fp;

    if ((fp = fdopen(fd, type)) == NULL)
	unix_error("Fdopen error");

    return fp;
}

char *Fgets(char *ptr, int n, FILE *stream) 
{
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
	app_error("Fgets error");

    return rptr;
}

FILE *Fopen(const char *filename, const char *mode) 
{
    FILE *fp;

    if ((fp = fopen(filename, mode)) == NULL)
	unix_error("Fopen error");

    return fp;
}

void Fputs(const char *ptr, FILE *stream) 
{
    if (fputs(ptr, stream) == EOF)
	unix_error("Fputs error");
}

size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    size_t n;

    if (((n = fread(ptr, size, nmemb, stream)) < nmemb) && ferror(stream)) 
	unix_error("Fread error");
    return n;
}

void Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    if (fwrite(ptr, size, nmemb, stream) < nmemb)
	unix_error("Fwrite error");
}


/**************************** 
 * Sockets interface wrappers
 ****************************/

int Socket(int domain, int type, int protocol) 
{
    int rc;

    if ((rc = socket(domain, type, protocol)) < 0)
	unix_error("Socket error");
    return rc;
}

void Setsockopt(int s, int level, int optname, const void *optval, int optlen) 
{
    int rc;

    if ((rc = setsockopt(s, level, optname, optval, optlen)) < 0)
	unix_error("Setsockopt error");
}

void Bind(int sockfd, struct sockaddr *my_addr, int addrlen) 
{
    int rc;

    if ((rc = bind(sockfd, my_addr, addrlen)) < 0)
	unix_error("Bind error");
}

void Listen(int s, int backlog) 
{
    int rc;

    if ((rc = listen(s,  backlog)) < 0)
	unix_error("Listen error");
}

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) 
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0)
	unix_error("Accept error");
    return rc;
}

void Connect(int sockfd, struct sockaddr *serv_addr, int addrlen) 
{
    int rc;

    if ((rc = connect(sockfd, serv_addr, addrlen)) < 0)
	unix_error("Connect error");
}

/*******************************
 * Protocol-independent wrappers
 *******************************/
/* $begin getaddrinfo */
void Getaddrinfo(const char *node, const char *service, 
                 const struct addrinfo *hints, struct addrinfo **res)
{
    int rc;

    if ((rc = getaddrinfo(node, service, hints, res)) != 0) 
        gai_error(rc, "Getaddrinfo error");
}
/* $end getaddrinfo */

void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags)
{
    int rc;

    if ((rc = getnameinfo(sa, salen, host, hostlen, serv, 
                          servlen, flags)) != 0) 
        gai_error(rc, "Getnameinfo error");
}

void Freeaddrinfo(struct addrinfo *res)
{
    freeaddrinfo(res);
}

void Inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (!inet_ntop(af, src, dst, size))
        unix_error("Inet_ntop error");
}

void Inet_pton(int af, const char *src, void *dst) 
{
    int rc;

    rc = inet_pton(af, src, dst);
    if (rc == 0)
	app_error("inet_pton error: invalid dotted-decimal address");
    else if (rc < 0)
        unix_error("Inet_pton error");
}

/*******************************************
 * DNS interface wrappers. 
 *
 * NOTE: These are obsolete because they are not thread safe. Use
 * getaddrinfo and getnameinfo instead
 ***********************************/

/* $begin gethostbyname */
struct hostent *Gethostbyname(const char *name) 
{
    struct hostent *p;

    if ((p = gethostbyname(name)) == NULL)
	dns_error("Gethostbyname error");
    return p;
}
/* $end gethostbyname */

struct hostent *Gethostbyaddr(const char *addr, int len, int type) 
{
    struct hostent *p;

    if ((p = gethostbyaddr(addr, len, type)) == NULL)
	dns_error("Gethostbyaddr error");
    return p;
}

/************************************************
 * Wrappers for Pthreads thread control functions
 ************************************************/

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
	posix_error(rc, "Pthread_create error");
}

void Pthread_cancel(pthread_t tid) {
    int rc;

    if ((rc = pthread_cancel(tid)) != 0)
	posix_error(rc, "Pthread_cancel error");
}

void Pthread_join(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
	posix_error(rc, "Pthread_join error");
}

/* $begin detach */
void Pthread_detach(pthread_t tid) {
    int rc;

    if ((rc = pthread_detach(tid)) != 0)
	posix_error(rc, "Pthread_detach error");
}
/* $end detach */

void Pthread_exit(void *retval) {
    pthread_exit(retval);
}

pthread_t Pthread_self(void) {
    return pthread_self();
}
 
void Pthread_once(pthread_once_t *once_control, void (*init_function)()) {
    pthread_once(once_control, init_function);
}

/*******************************
 * Wrappers for Posix semaphores
 *******************************/

void Sem_init(sem_t *sem, int pshared, unsigned int value) 
{
    if (sem_init(sem, pshared, value) < 0)
	unix_error("Sem_init error");
}

void P(sem_t *sem) 
{
    if (sem_wait(sem) < 0)
	unix_error("P error");
}

void V(sem_t *sem) 
{
    if (sem_post(sem) < 0)
	unix_error("V error");
}

/****************************************
 * The Rio package - Robust I/O functions
 ****************************************/

/*
 * rio_readn - Robustly read n bytes (unbuffered)
 */
/* $begin rio_readn */
ssize_t rio_readn(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nread = read(fd, bufp, nleft)) < 0) {
	    if (errno == EINTR) /* Interrupted by sig handler return */
		nread = 0;      /* and call read() again */
	    else
		return -1;      /* errno set by read() */ 
	} 
	else if (nread == 0)
	    break;              /* EOF */
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);         /* Return >= 0 */
}
/* $end rio_readn */

/*
 * rio_writen - Robustly write n bytes (unbuffered)
 */
/* $begin rio_writen */
ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return */
		nwritten = 0;    /* and call write() again */
	    else
		return -1;       /* errno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}
/* $end rio_writen */


/* 
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* Refill if buf is empty */
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
			   sizeof(rp->rio_buf));
	if (rp->rio_cnt < 0) {
	    if (errno != EINTR) /* Interrupted by sig handler return */
		return -1;
	}
	else if (rp->rio_cnt == 0)  /* EOF */
	    return 0;
	else 
	    rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;          
    if (rp->rio_cnt < n)   
	cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/* $end rio_read */

/*
 * rio_readinitb - Associate a descriptor with a read buffer and reset buffer
 */
/* $begin rio_readinitb */
void rio_readinitb(rio_t *rp, int fd) 
{
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;  
    rp->rio_bufptr = rp->rio_buf;
}
/* $end rio_readinitb */

/*
 * rio_readnb - Robustly read n bytes (buffered)
 */
/* $begin rio_readnb */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;
    
    while (nleft > 0) {
	if ((nread = rio_read(rp, bufp, nleft)) < 0) 
            return -1;          /* errno set by read() */ 
	else if (nread == 0)
	    break;              /* EOF */
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);         /* return >= 0 */
}
/* $end rio_readnb */

/* 
 * rio_readlineb - Robustly read a text line (buffered)
 */
/* $begin rio_readlineb */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) { 
        if ((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c;
	    if (c == '\n') {
                n++;
     		break;
            }
	} else if (rc == 0) {
	    if (n == 1)
		return 0; /* EOF, no data read */
	    else
		break;    /* EOF, some data was read */
	} else
	    return -1;	  /* Error */
    }
    *bufp = 0;
    return n-1;
}
/* $end rio_readlineb */

/**********************************
 * Wrappers for robust I/O routines
 **********************************/
ssize_t Rio_readn(int fd, void *ptr, size_t nbytes) 
{
    ssize_t n;
  
    if ((n = rio_readn(fd, ptr, nbytes)) < 0)
	unix_error("Rio_readn error");
    return n;
}

void Rio_writen(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n)
	unix_error("Rio_writen error");
}

void Rio_readinitb(rio_t *rp, int fd)
{
    rio_readinitb(rp, fd);
} 

ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
	unix_error("Rio_readnb error");
    return rc;
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
	unix_error("Rio_readlineb error");
    return rc;
} 

/******************************** 
 * Client/server helper functions
 ********************************/
/*
 * open_clientfd - Open connection to server at <hostname, port> and
 *     return a socket descriptor ready for reading and writing. This
 *     function is reentrant and protocol-independent.
 * 
 *     On error, returns -1 and sets errno.  
 */
/* $begin open_clientfd */
int open_clientfd(char *hostname, char *port) {
    int clientfd;
    struct addrinfo hints, *listp, *p;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV;  /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG;  /* Recommended for connections */
    Getaddrinfo(hostname, port, &hints, &listp);
  
    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue; /* Socket failed, try the next */

        /* Connect to the server */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) 
            break; /* Success */
        Close(clientfd); /* Connect failed, try another */  //line:netp:openclientfd:closefd
    } 

    /* Clean up */
    Freeaddrinfo(listp);
    if (!p) /* All connects failed */
        return -1;
    else    /* The last connect succeeded */
        return clientfd;
}
/* $end open_clientfd */

/*  
 * open_listenfd - Open and return a listening socket on port. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns -1 and sets errno.
 */
/* $begin open_listenfd */
int open_listenfd(char *port) 
{
    struct addrinfo hints, *listp, *p;
    int listenfd, optval=1;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
    hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
    Getaddrinfo(NULL, port, &hints, &listp);

    /* Walk the list for one that we can bind to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue;  /* Socket failed, try the next */

        /* Eliminates "Address already in use" error from bind */
        Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,    //line:netp:csapp:setsockopt
                   (const void *)&optval , sizeof(int));

        /* Bind the descriptor to the address */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* Success */
        Close(listenfd); /* Bind failed, try the next */
    }

    /* Clean up */
    Freeaddrinfo(listp);
    if (!p) /* No address worked */
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0) {
        Close(listenfd);
	return -1;
    }
    return listenfd;
}
/* $end open_listenfd */

/****************************************************
 * Wrappers for reentrant protocol-independent helpers
 ****************************************************/
int Open_clientfd(char *hostname, char *port) 
{
    int rc;

    if ((rc = open_clientfd(hostname, port)) < 0) 
	unix_error("Open_clientfd error");
    return rc;
}

int Open_listenfd(char *port) 
{
    int rc;

    if ((rc = open_listenfd(port)) < 0)
	unix_error("Open_listenfd error");
    return rc;
}

/* $begin shellmain */

/* Function prototypes */
void eval(char *cmdline, char **pipe_cmd, int is_pipe);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 
char*** parse_pipe(char ***pipe_cmd, int *pipe_num);
int run_pipe(char ***pipe_res, int cnt, int pipe_num, int *previous_fd, int flag);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigchld_handler(int sig);
void sigttin_handler(int sig);
void sigttou_handler(int sig);
void add_job(pid_t pid, char* name, int stat, pid_t pgid, int is_piped);
void del_job(pid_t pid);


typedef struct proc{
    pid_t pid;
    char process_name[100];
    int stat; // 0 for Background, 1 for Stopped(Suspended), 2 for Running
    pid_t pgid;
    int is_piped; 
}proc;

proc tracked_jobs[100]; //Stores the jobs' states and all informations about them.
int tracked_num = 0; //How many jobs in the job list?
pid_t bg_grp; //obsolete variable
pid_t child_pid = 999999; //obsolete variable
pid_t stopped_grp; //obsolete variable

int main() 
{
    int is_pipe = 0;
    char cmdline[MAXLINE]; /* Command line */
    char *pipe_cmd = (char*)malloc(MAXLINE);
    memset(pipe_cmd, 0, sizeof(pipe_cmd)); //empty the string   
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGTTOU, SIG_IGN);
    Signal(SIGTTIN, sigttin_handler);    
    //char pipe_cmd[MAXARGS][MAXLINE]; /* For storing the commands including | to use execvp. Each argument has to be null-terminated. */
    // pipe_cmd will be parsed in parseline or parsepipe later on. 
    while (1) {
	/* Read */
        int pipe_num = 0;
	    printf("CSE4100-SP-P#3> ");                   
	    fgets(cmdline, MAXLINE, stdin);
        if(strchr(cmdline, '|')) is_pipe = 1; //if the command contains pipe, set flag
        if (feof(stdin))
	        exit(0);

        while (cmdline[strlen(cmdline)-2] == '|' || (is_pipe && !strcmp(cmdline, "\n"))){
            if (!strcmp(cmdline, "|\n")){
                printf("syntax error near unexpected token '|'\n");
                memset(cmdline, 0, sizeof(cmdline));
                break;
            } //breaks if the input is given as singleton '|'
            cmdline[strlen(cmdline)-1] = '\0'; //change \n to null 
            if (strcmp(cmdline, "\n")){ 
                strcat(pipe_cmd, cmdline); //copy the first line to pipe_cmd[pipe_num++] only if cmdline is NOT an empty one.
            }
            printf("> ");
            fgets(cmdline, MAXLINE, stdin);
        }
        if (is_pipe){
            strcat(pipe_cmd, cmdline); //copy the last command without a pipe, in a command containing pipes. OR the command comes at once (e.g "ls -al | grep abc | sort -r")
        }
        //printf("pipe CMD : %s\n", pipe_cmd);
	    /* Evaluate */
    	eval(cmdline, &pipe_cmd, is_pipe);
        memset(pipe_cmd, 0, sizeof(pipe_cmd));
        // don't forget to memset pipe_cmd to empty the stored previous commands and set is_pipe to 0.
        is_pipe = 0;
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline, char **pipe_cmd, int is_pipe) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char **pipe_args[MAXARGS];
    char ***pipe_res;
    //char *pipe_args[MAXARGS][MAXLINE]; /* Argument list for execvp() in pipe operations */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    int pipe_num = 0; // How many pipe commands are in pipe_res?
    pid_t pid_p, pid_np;           /* Process id for the first, second case (command does/ does not contain pipe)*/
    
    char program_path[100] = "/bin/";  /* Import Shell Commands from /bin (ls, su, ... etc..)*/
    sigset_t mask;
    sigset_t mask_chld;
    Sigfillset(&mask);
    Sigemptyset(&mask_chld);
    Sigaddset(&mask_chld, SIGCHLD);

    /*
    if (is_pipe){
        char *** pipe_res = parse_pipe(&pipe_cmd, &pipe_num); //pipe_res is the parsed pipe commands.
        int previous_fd = 0;
        int status_p;
        if ((pid_p = Fork()) == 0){ //child
            run_pipe(pipe_res,0,pipe_num,&previous_fd); //pipe_num is assigned in parse_pipe().  
            exit(0);
        }
        //parent
        waitpid(pid_p, &status_p, 0);
        //add background jobs code

    }*/
        strcpy(buf, cmdline);
        bg = parseline(buf, argv); //If the command has no | in it
        if (argv[0] == NULL)  
	        return;   /* Ignore empty lines */
        strcat(program_path, argv[0]); /* program_path would be /bin/argv[0], where argv[0] is the command */
        if (access(program_path, F_OK) == 0){
            //file exists
        } else {
            program_path[0] = '\0'; //null-terminate the string. (empty the string)
            strcpy(program_path, argv[0]); // program_path = argv[0] ex) ./a.out
        }
        if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
            Sigprocmask(SIG_BLOCK, &mask_chld, NULL);  
            if((pid_np = Fork()) == 0) { /* Child runs user job */
                Sigprocmask(SIG_UNBLOCK, &mask_chld, NULL);
                    if (is_pipe){
                        char ***pipe_res = parse_pipe(&pipe_cmd, &pipe_num);//pipe_res is the parsed pipe commands. 
                        if (!pipe_res) exit(2); //if pipe_res == NULL, we can say that it has invalid & in the middle of the string.
                        int previous_fd = 0;
                        if (run_pipe(pipe_res, 0, pipe_num, &previous_fd, 0)== 2){
                            Sigprocmask(SIG_BLOCK, &mask, NULL);
                            Sigprocmask(SIG_UNBLOCK, &mask, NULL);
                            exit(1); //SIGTSTP
                        }
                    }
                    else{
                        if (execve(program_path, argv, environ) < 0) {	//ex) /bin/ls ls -al & -> import the commands from /bin
                            printf("%s: Command not found.\n", argv[0]);
                            exit(0);
                        }
                    }
            }
                    
	    /* Parent waits for foreground job to terminate */
        /* In everything running in foreground, all signals are handled by the below part. */
        /* SIGCHLD handler does handle foreground processes, but due to the blocking conditions it does nothing inside it. */
	        if (!bg){
                    add_job(pid_np, cmdline, 2, pid_np, 0);
	                int status_np;
                    waitpid(pid_np, &status_np, WUNTRACED);
                    //printf("pid_np : %d\n", pid_np);
                    int cnt = 0;
                    for (cnt=0; cnt<tracked_num; cnt++){
                        if (tracked_jobs[cnt].pid == pid_np){ 
                            break;
                        }
                    }
                    if (WIFSTOPPED(status_np) || WSTOPSIG(status_np) == SIGTSTP){
                        Sigprocmask(SIG_BLOCK, &mask, NULL);
                        tracked_jobs[cnt].stat = 1;
                        Sigprocmask(SIG_UNBLOCK, &mask, NULL);
                        return;
                    }
                    if (WIFEXITED(status_np) || WTERMSIG(status_np) == SIGINT){                       
                        if (!is_pipe || (is_pipe && WEXITSTATUS(status_np)!= 1))
                            del_job(pid_np);  
                        if (WEXITSTATUS(status_np) == 1){
                            tracked_jobs[cnt].stat = 1;
                        }
                        else if (WEXITSTATUS(status_np) == 2){
                            return;
                        } //stopped from pipe command;
                        //turn this foreground job to background job to be restarted by fg command or bg command   

                        //delete job if it is terminated and reaped by the parent
                    }
                    Sigprocmask(SIG_BLOCK, &mask, NULL);
                    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    //if (WIFEXITED(status_np))
                    //printf("child %d terminated normally with exit status = %d\n", pid_np, WEXITSTATUS(status_np));
	            }

	        else{
                int status;
                setpgid(pid_np, pid_np); //important; put each background processes to different process groups.
                //when there is background process!
                //We are not waiting for the process to terminate (the parent process do not hang)
                Sigprocmask(SIG_BLOCK, &mask, NULL);
                add_job(pid_np, cmdline, 0, pid_np, 0); //add state 0
                printf("[%d]     %d\n", tracked_num, pid_np);
                Sigprocmask(SIG_UNBLOCK, &mask, NULL);
            }
        }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    char s[100];
    if (!strcmp(argv[0], "quit")) /* quit command */
	exit(0);  
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	return 1;
    if (!strcmp(argv[0], "cd")){ //'cd' is a builtin command for the shell. It is NOT an external process.
        //printf("%s\n", getcwd(s, 100));
        if (argv[1] == NULL || !strcmp("~", argv[1]) || !strcmp("$HOME", argv[1])){ // The commands are respectively, [ "cd", "cd ~", "cd $HOME" ]
            char *home_dir = getenv("HOME");
            chdir(home_dir);
            return 1;
        }
        if (!strcmp(argv[1], "..")) /* If the command is 'cd ..', navigate to the previous directory */
            chdir("..");
        else { // The command is 'cd (SPECIFIC DIRECTORY)', so navigate to that directory
            if (chdir(argv[1]) == -1) // The file or directory to navigate to does not exist (chdir() == -1)
                printf("cd : %s : No such file or directory\n", argv[1]);
        }
        //printf("%s\n", getcwd(s, 100));
        return 1;
    }
    if (!strcmp(argv[0], "exit")){ //'exit' is a builtin command for the shell, but it has to exit all the child processes.
        exit(0);
    }
    if (!strcmp(argv[0], "jobs")){
        for (int i=0; i<tracked_num; i++){
            if (tracked_jobs[i].stat == 2 || tracked_jobs[i].stat == 0) //if background or foreground
                printf("[%d]    Running               %s", i+1, tracked_jobs[i].process_name);
            else if (tracked_jobs[i].stat == 1)
                printf("[%d]    Stopped               %s", i+1, tracked_jobs[i].process_name);
        }
        return 1;
    }
    if (!strcmp(argv[0], "bg")){
        if (argv[1] == NULL){
            printf("Invalid bg input. Please try again.\n");
            return 1;
        }
        if (argv[1][0] == '%'){
            char jobnum_char[10];
            int job_num;
            strcpy(jobnum_char, argv[1]+1);
            job_num = atoi(jobnum_char);
            if (job_num > tracked_num || job_num == 0){
                printf("bg : %%%d : No such job\n", job_num);
                return 1;
            }
            if (tracked_jobs[job_num-1].stat == 1){ //If the process is stopped
                kill(tracked_jobs[job_num-1].pid, SIGCONT); //send SIGCONT to the corresponding process to run it in background.
                tracked_jobs[job_num-1].stat = 0;
            }
        }
        else 
            printf("Invalid bg input. Please try again.\n");
        return 1;
    }

    if (!strcmp(argv[0], "fg")){
        if (argv[1] == NULL){
            printf("Invalid fg command. Please try again.\n");
            return 1;
        }
        if (argv[1][0] == '%'){
            char jobnum_char[10];
            int job_num;
            int status;
            sigset_t mask;
            Sigfillset(&mask);
            strcpy(jobnum_char, argv[1]+1);
            job_num = atoi(jobnum_char);
            if (job_num > tracked_num || job_num == 0){
                printf("fg : %%%d : No such job\n", job_num);
                return 1;
            }
            Signal(SIGTTOU, SIG_IGN);
            Signal(SIGTTIN, sigttin_handler);
            if (tracked_jobs[job_num-1].stat == 1){ //If the process is stopped
                tcsetpgrp(0, getpgid(tracked_jobs[job_num-1].pid)); 
                kill(tracked_jobs[job_num-1].pid, SIGCONT); //send SIGCONT to the corresponding process to run it on foreground.
                waitpid(tracked_jobs[job_num-1].pid, &status, WUNTRACED); //wait until the foreground job is stopped by ctrl+z or terminated.
                tcsetpgrp(0, getpgid(getpid()));
                if (WSTOPSIG(status)==SIGTSTP){
                    kill(tracked_jobs[job_num-1].pid, SIGTSTP);
                    return 1;
                }
                tracked_jobs[job_num-1].stat = 2; //set the state as foreground.
                del_job(tracked_jobs[job_num-1].pid);
            }
            else if (tracked_jobs[job_num-1].stat == 0){ //If background

                tcsetpgrp(0, getpgid(tracked_jobs[job_num-1].pid));
                kill(tracked_jobs[job_num-1].pid, SIGCONT);
                waitpid(tracked_jobs[job_num-1].pid, &status, WUNTRACED);
                tcsetpgrp(0, getpgid(getpid()));

                if (WTERMSIG(status)==SIGINT){
                    Sigprocmask(SIG_BLOCK, &mask, NULL);
                    del_job(tracked_jobs[job_num-1].pid);
                    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    kill(tracked_jobs[job_num-1].pgid, SIGINT);
                    return 1;
                }
                if (WIFEXITED(status)){
                    Sigprocmask(SIG_BLOCK, &mask, NULL);
                    del_job(tracked_jobs[job_num-1].pid);
                    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    return 1;
                }
                if (WSTOPSIG(status) == SIGTSTP){
                    Sigprocmask(SIG_BLOCK, &mask, NULL);
                    printf("CHILD STOPPED\n");
                    tracked_jobs[job_num-1].stat = 1;
                    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
                    kill(tracked_jobs[job_num-1].pgid, SIGTSTP);
                    return 1;
                }
            }
        }
        else
            printf("Invalid fg input. Please try again.\n");
        return 1;
    }

    if (!strcmp(argv[0], "kill")){
        if (argv[1] == NULL){
            printf("Invalid kill input. Please try again.\n");
            return 1;
        }
        if (argv[1][0] == '%'){
            char jobnum_char[10];
            int job_num;
            strcpy(jobnum_char, argv[1]+1); //copy the job number
            job_num = atoi(jobnum_char);
            if (job_num > tracked_num || job_num == 0){
                printf("kill : %%%d : No such job\n", job_num);
                return 1; 
            }
            kill(tracked_jobs[job_num-1].pid, SIGKILL); //send SIGKILL to the corresponding process
            del_job(tracked_jobs[job_num-1].pid);
        }
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*(buf) == '"'){
    	buf++;
    	delim = strchr(buf, '"'); //find double quotation mark after removing trailing spaces
    }
    else if (*(buf) == 39){
    	buf++;
    	delim = strchr(buf, 39); //single quotation mark
    }
    else {
    	delim = strchr(buf, ' '); //find the next space
    }
    while (delim != NULL) {
	argv[argc++] = buf;
	*delim = '\0';
	
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    	if (*(buf) == '"'){
    		buf++;
            //printf("DOUBLE Q : %s\n", buf);
    		delim = strchr(buf, '"'); //find double quotation mark after removing trailing spaces
    	}
    	else if (*(buf) == 39){
    		buf++;
            //printf("SINGLE Q : %s\n", buf);
    		delim = strchr(buf, 39); //single quotation mark
    	}
    	else {
            //printf("SPACE : %s\n", buf);
    		delim = strchr(buf, ' '); //find the next space
    	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;


    int lastarg_len = strlen(argv[argc-1]);

    /* Should the job run in the background? */
    if ((bg = (argv[argc-1][lastarg_len-1] == '&')) != 0){
        *(argv[argc-1]+lastarg_len-1) = 0;
        if (lastarg_len == 1){
           argv[argc-1] = NULL;
           argc--; // if the last element of argv contained only '&', then delete the last argument.
        }
    }

    return bg;
}
/* $end parseline */

/* use c1 | c2 | c3.. */

/* returns the parsed commands compatible for execvp() */
char*** parse_pipe(char ***pipe_cmd, int *pipe_num)
{
    int i, j;
    int is_bg = 0; // flag to tell if the piped command has invalid &.
    char *argv[MAXARGS];
    char sliced[MAXARGS][MAXLINE]; //slice the commands with respect to '|' : ex) ls -al | grep abc -> "ls -al", "grep abc"
    char ***pipe_args = (char***)malloc(sizeof(char**)*MAXARGS);
    char tmp[MAXARGS][MAXLINE];
    for(i=0; i<MAXARGS; i++){
        pipe_args[i] = (char**)malloc(sizeof(char*)*MAXARGS);
        for(j=0; j<MAXARGS; j++){
            pipe_args[i][j] = (char*)malloc(sizeof(char)*MAXLINE);
        }
    }//allocate space for pipe_args
    char *ptr = strtok(**pipe_cmd, "|");
    strcpy(sliced[0], ptr);
    int cnt = 1;
    while(ptr != NULL){
        ptr = strtok(NULL, "|");
        if (ptr != NULL) {
            //printf("%s\n", ptr);
            strcpy(sliced[cnt], ptr);
            cnt++; 
        }
    }
    for(i=0; i<cnt-1; i++)
        strcat(sliced[i], "\n"); //fit to parseline except for the last line since it already has a newline
    
    for(i=0; i<cnt; i++){
        strcpy(tmp[i], sliced[i]);
    }
    int argcc[10] = {0,}; //The number of arguments in a single command. (pipe)
    for(i=0; i<cnt; i++){
        is_bg = 0; 
        is_bg = parseline(sliced[i], argv);
        if (is_bg && i!=cnt-1){ // If & is detected in the middle of the string rather than the last character, error.
            printf("syntax error near unexpected token `|'\n");
            return NULL; 
        }
        //printf("SLICED IN LOOP : %s\n", sliced[i]);
        for(j=0; argv[j]!=NULL; j++){
            strcpy(pipe_args[i][j], argv[j]);
            argcc[i]++;
            //printf("ARGS, PIPE: %s........... %s\n", argv[j], pipe_args[i][j]);
        }
        pipe_args[i][j] = NULL; //null-terminate a line of command args
        for(j=0; argv[j]!=NULL; j++)
            memset(argv[j], 0, sizeof(argv[j]));
        for(j=0; j<cnt; j++){
            memset(sliced[j], 0, sizeof(sliced[j]));
            strcpy(sliced[j], tmp[j]);
        }
    }
    /*for(i=0; i<cnt; i++){
        printf("argcc: %d\n", argcc[i]);
        for(j=0; j<argcc[i]; j++){
            printf("PIPE %s\n", pipe_args[i][j]);
        }
    }*/
    *pipe_num = cnt; // saves the number of pipe commands
    return pipe_args; //return the saved pipe commands to be used in execvp in run_pipe.
}

int run_pipe(char ***pipe_res, int cnt, int pipe_num, int *previous_fd, int flag)
{ 
    Signal(SIGTTOU, sigttou_handler); 
    if (flag==1){
        exit(0);
    }
    if (flag==2){
        return 2;
    }
    int i;
    char program_path[100] = "/bin/";
    char **line = (char**)malloc(sizeof(char*)*MAXARGS);
    for (i=0; i<MAXARGS; i++)
        line[i] = (char*)malloc(MAXLINE);
    for (i=0; i<MAXARGS; i++)
        memset(line[i], 0, sizeof(line[i]));
    for (i=0; pipe_res[cnt][i]!= NULL; i++){
        strcpy(line[i], pipe_res[cnt][i]);
    }
    line[i] = NULL; 
    strcat(program_path, line[0]); // ex) /bin/ls
    if (access(program_path, F_OK) == 0){
            //file exists
    } else {
            program_path[0] = '\0'; //null-terminate the string.
            strcpy(program_path, line[0]); // program_path = line[0]  ex) ./a.out
    }    
    int pid, status;
    int pfd[2];
    if (cnt == pipe_num) 
        exit(0);
    pipe(pfd);
    if ((pid = Fork()) == 0){
        dup2(*previous_fd, 0);
        if (cnt+1 != pipe_num)
            dup2(pfd[1], 1);
        close(pfd[0]);
        if (execvp(program_path, line) < 0){
            if (strcmp(program_path, line[0]))
                printf("%s : Command not found.\n", line[0]);
            else 
                printf("%s : No such file or directory.\n", line[0]); 
            exit(0);
        }
        //sends executed output to pfd[1], so parent can read it in its pfd[0].
        exit(0);
    }
    close(pfd[1]);
    *previous_fd = pfd[0]; //store previous command output to use it in the next recursive call.
    waitpid(pid, &status, WUNTRACED);
    if (WSTOPSIG(status) == SIGTSTP){ //If SIGTSTP return 2, which exit value lets it not being deleted from the job list(temporary)
        for (int i=0; i<tracked_num; i++){
            if (tracked_jobs[i].pid == pid)
                tracked_jobs[i].stat = 1;
        }
        return 2;
    }
    if (WTERMSIG(status) == SIGINT){
        del_job(pid);
        flag = 1;
    } //If SIGINT, set flag to be 1, so it could just exit in the next recursive call.
    run_pipe(pipe_res,cnt+1,pipe_num,previous_fd,flag); //recursive call to next pipe.
}


void sigint_handler(int sig)
{
    for (int i=0; i<tracked_num; i++){
        if (tracked_jobs[i].pid == child_pid)
            kill(tracked_jobs[i].pid, SIGINT);
    }
    return;
}

/* $end myshell.c */
void sigchld_handler(int sig) //for background processes
{
    int olderrno = errno;
    int status;
    int i =0;
    sigset_t mask;
    pid_t pid;
    Sigfillset(&mask);
    Sigprocmask(SIG_BLOCK, &mask, NULL);


    if ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) { //This is intended for use in only background processes.

        for (i=0; i<tracked_num; i++){
            if (tracked_jobs[i].pid == pid){
                break;
            }
        }
 
        if (tracked_jobs[i].stat != 0) //this SIGCHLD handler can be accessed by foreground jobs, so just block them from here.
            return;
        

        if (WTERMSIG(status)==SIGINT){
            Sigprocmask(SIG_BLOCK, &mask, NULL);
            del_job(tracked_jobs[i].pid); //delete the terminated job by receiving SIGINT from the child process from the job list.
            Sigprocmask(SIG_UNBLOCK, &mask, NULL);
            return;
        }
        if (WIFEXITED(status)){
            Sigprocmask(SIG_BLOCK, &mask, NULL);
            del_job(tracked_jobs[i].pid); //delete the terminated job from the job list.
            Sigprocmask(SIG_UNBLOCK, &mask, NULL);
            return;
        }
        if (WSTOPSIG(status) == SIGTSTP){
            Sigprocmask(SIG_BLOCK, &mask, NULL);
            tracked_jobs[i].stat = 1; //Change the status value of the STOPPED job to 1.
            Sigprocmask(SIG_UNBLOCK, &mask, NULL);
            return;
        }
    }
    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
    errno = olderrno;
}

void sigtstp_handler(int sig)
{
    for (int i=0; i<tracked_num; i++){
        if (tracked_jobs[i].stat == 2){ //send SIGTSTP to all the processes running on foreground.
            kill(tracked_jobs[i].pid, SIGTSTP); 
            printf("[%d]  Stopped       %s", i+1, tracked_jobs[i].process_name);
        }
    }
    return;
}

void sigttin_handler(int sig)
{
    return;
}

void sigttou_handler(int sig)
{
    Signal(SIGTTOU, SIG_IGN);
    return;
}
/*adds a job in the job list tracked_jobs*/
void add_job(int pid, char *name, int stat, pid_t pgid, int is_piped)
{
    tracked_jobs[tracked_num].pid = pid;
    strcpy(tracked_jobs[tracked_num].process_name, name);
    tracked_jobs[tracked_num].stat = stat;
    tracked_jobs[tracked_num].pgid = pgid;
    tracked_jobs[tracked_num].is_piped = is_piped;
    //printf("I ADDED %d  %s", tracked_jobs[tracked_num].pid, tracked_jobs[tracked_num].process_name);
    tracked_num++;   
}
/*deletes a job in the job list tracked_jobs*/
void del_job(int pid)
{
    //printf("WHICH BEING DELETED : %d\n", pid);
    for (int i=0; i<tracked_num; i++) {
        if (tracked_jobs[i].pid == pid){
            //printf("I DELETED %d  %s", tracked_jobs[i].pid, tracked_jobs[i].process_name);
            for (int j=i+1; j<tracked_num; j++){
                tracked_jobs[j-1] = tracked_jobs[j];
            }
            tracked_num--;
        }
    }
}





