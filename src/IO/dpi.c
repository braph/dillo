/*
 * File: dpi.c
 *
 * Copyright (C) 2002-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * Dillo plugins (small programs that interact with dillo)
 *
 * Dillo plugins are designed to handle:
 *   bookmarks, cookies, FTP, downloads, files, preferences, https,
 *   datauri and a lot of any-to-html filters.
 */


#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>           /* for errno */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "../msg.h"
#include "../klist.h"
#include "IO.h"
#include "Url.h"
#include "../../dpip/dpip.h"

/* This one is tricky, some sources state it should include the byte
 * for the terminating NULL, and others say it shouldn't. */
# define D_SUN_LEN(ptr) ((size_t) (((struct sockaddr_un *) 0)->sun_path) \
                        + strlen ((ptr)->sun_path))

/* Solaris may not have this one... */
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif


typedef struct {
   int InTag;
   int Send2EOF;

   int DataTotalSize;
   int DataRecvSize;

   Dstr *Buf;

   int BufIdx;
   int TokIdx;
   int TokSize;
   int TokIsTag;

   ChainLink *InfoRecv;
   int Key;
} dpi_conn_t;


/*
 * Local data
 */
static Klist_t *ValidConns = NULL; /* Active connections list. It holds
                                    * pointers to dpi_conn_t structures. */


/*
 * Initialize local data
 */
void a_Dpi_init(void)
{
   /* empty */
}

/*
 * Close a FD handling EINTR
 */
static void Dpi_close_fd(int fd)
{
   int st;

   do
      st = close(fd);
   while (st < 0 && errno == EINTR);
}

/*
 * Create a new connection data structure
 */
static dpi_conn_t *Dpi_conn_new(ChainLink *Info)
{
   dpi_conn_t *conn = dNew0(dpi_conn_t, 1);

   conn->Buf = dStr_sized_new(8*1024);
   conn->InfoRecv = Info;
   conn->Key = a_Klist_insert(&ValidConns, conn);

   return conn;
}

/*
 * Free a connection data structure
 */
static void Dpi_conn_free(dpi_conn_t *conn)
{
   a_Klist_remove(ValidConns, conn->Key);
   dStr_free(conn->Buf, 1);
   dFree(conn);
}

/*
 * Check whether a conn is still valid.
 * Return: 1 if found, 0 otherwise
 */
static int Dpi_conn_valid(int key)
{
   return (a_Klist_get_data(ValidConns, key)) ? 1 : 0;
}

/*
 * Append the new buffer in 'dbuf' to Buf in 'conn'
 */
static void Dpi_append_dbuf(dpi_conn_t *conn, DataBuf *dbuf)
{
   if (dbuf->Code == 0 && dbuf->Size > 0) {
      dStr_append_l(conn->Buf, dbuf->Buf, dbuf->Size);
   }
}

/*
 * Split the data stream into tokens.
 * Here, a token is either:
 *    a) a dpi tag
 *    b) a raw data chunk
 *
 * Return Value: 0 upon a new token, -1 on not enough data.
 *
 * TODO: define an API and move this function into libDpip.a.
*/
static int Dpi_get_token(dpi_conn_t *conn)
{
   int i, resp = -1;
   char *buf = conn->Buf->str;

   if (conn->BufIdx == conn->Buf->len) {
      dStr_truncate(conn->Buf, 0);
      conn->BufIdx = 0;
      return resp;
   }

   if (conn->Send2EOF) {
      conn->TokIdx = conn->BufIdx;
      conn->TokSize = conn->Buf->len - conn->BufIdx;
      conn->BufIdx = conn->Buf->len;
      return 0;
   }

   _MSG("conn->BufIdx = %d; conn->Buf->len = %d\nbuf: [%s]\n",
        conn->BufIdx,conn->Buf->len, conn->Buf->str + conn->BufIdx);

   if (!conn->InTag) {
      /* search for start of tag */
      while (conn->BufIdx < conn->Buf->len && buf[conn->BufIdx] != '<')
         ++conn->BufIdx;
      if (conn->BufIdx < conn->Buf->len) {
         /* found */
         conn->InTag = 1;
         conn->TokIdx = conn->BufIdx;
      } else {
         MSG_ERR("[Dpi_get_token] Can't find token start\n");
      }
   }

   if (conn->InTag) {
      /* search for end of tag (EOT=" '>") */
      for (i = conn->BufIdx; i < conn->Buf->len; ++i)
         if (buf[i] == '>' && i >= 2 && buf[i-1] == '\'' && buf[i-2] == ' ')
            break;
      conn->BufIdx = i;

      if (conn->BufIdx < conn->Buf->len) {
         /* found EOT */
         conn->TokIsTag = 1;
         conn->TokSize = conn->BufIdx - conn->TokIdx + 1;
         ++conn->BufIdx;
         conn->InTag = 0;
         resp = 0;
      }
   }

   return resp;
}

/*
 * Parse a dpi tag and take the appropriate actions
 */
static void Dpi_parse_token(dpi_conn_t *conn)
{
   char *tag, *cmd, *msg, *urlstr;
   DataBuf *dbuf;
   char *Tok = conn->Buf->str + conn->TokIdx;

   if (conn->Send2EOF) {
      /* we're receiving data chunks from a HTML page */
      dbuf = a_Chain_dbuf_new(Tok, conn->TokSize, 0);
      a_Chain_fcb(OpSend, conn->InfoRecv, dbuf, "send_page_2eof");
      dFree(dbuf);
      return;
   }

   tag = dStrndup(Tok, (size_t)conn->TokSize);
   _MSG("Dpi_parse_token: {%s}\n", tag);

   cmd = a_Dpip_get_attr(Tok, conn->TokSize, "cmd");
   if (strcmp(cmd, "send_status_message") == 0) {
      msg = a_Dpip_get_attr(Tok, conn->TokSize, "msg");
      a_Chain_fcb(OpSend, conn->InfoRecv, msg, cmd);
      dFree(msg);

   } else if (strcmp(cmd, "chat") == 0) {
      msg = a_Dpip_get_attr(Tok, conn->TokSize, "msg");
      a_Chain_fcb(OpSend, conn->InfoRecv, msg, cmd);
      dFree(msg);

   } else if (strcmp(cmd, "dialog") == 0) {
      /* For now will send the dpip tag... */
      a_Chain_fcb(OpSend, conn->InfoRecv, tag, cmd);

   } else if (strcmp(cmd, "start_send_page") == 0) {
      conn->Send2EOF = 1;
      urlstr = a_Dpip_get_attr(Tok, conn->TokSize, "url");
      a_Chain_fcb(OpSend, conn->InfoRecv, urlstr, cmd);
      dFree(urlstr);
      /* TODO: a_Dpip_get_attr(Tok, conn->TokSize, "send_mode") */

   } else if (strcmp(cmd, "reload_request") == 0) {
      urlstr = a_Dpip_get_attr(Tok, conn->TokSize, "url");
      a_Chain_fcb(OpSend, conn->InfoRecv, urlstr, cmd);
      dFree(urlstr);
   }
   dFree(cmd);

   dFree(tag);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * Get a new data buffer (within a 'dbuf'), save it into local data,
 * split in tokens and parse the contents.
 */
static void Dpi_process_dbuf(int Op, void *Data1, dpi_conn_t *conn)
{
   DataBuf *dbuf = Data1;
   int key = conn->Key;

   /* Very useful for debugging: show the data stream as received. */
   /* fwrite(dbuf->Buf, dbuf->Size, 1, stdout); */

   if (Op == IORead) {
      Dpi_append_dbuf(conn, dbuf);
      /* 'conn' has to be validated because Dpi_parse_token() MAY call abort */
      while (Dpi_conn_valid(key) && Dpi_get_token(conn) != -1) {
         Dpi_parse_token(conn);
      }

   } else if (Op == IOClose) {
      /* unused */
   }
}

/*
 * Start dpid.
 * Return: 0 starting now, 1 Error.
 */
static int Dpi_start_dpid(void)
{
   pid_t pid;
   int st_pipe[2], n, ret = 1;
   char buf[16];

   /* create a pipe to track our child's status */
   if (pipe(st_pipe))
      return 1;

   pid = fork();
   if (pid == 0) {
      /* This is the child process.  Execute the command. */
      char *path1 = dStrconcat(dGethomedir(), "/.dillo/dpid", NULL);
      Dpi_close_fd(st_pipe[0]);
      if (execl(path1, "dpid", NULL) == -1) {
         dFree(path1);
         if (execlp("dpid", "dpid", NULL) == -1) {
            MSG("Dpi_start_dpid (child): %s\n", dStrerror(errno));
            do
               n = write(st_pipe[1], "ERROR", 5);
            while (n == -1 && errno == EINTR);
            Dpi_close_fd(st_pipe[1]);
            _exit (EXIT_FAILURE);
         }
      }
   } else if (pid < 0) {
      /* The fork failed.  Report failure.  */
      MSG("Dpi_start_dpid: %s\n", dStrerror(errno));
      /* close the unused pipe */
      Dpi_close_fd(st_pipe[0]);
      Dpi_close_fd(st_pipe[1]);

   } else {
      /* This is the parent process, check our child status... */
      Dpi_close_fd(st_pipe[1]);
      do
         n = read(st_pipe[0], buf, 16);
      while (n == -1 && errno == EINTR);
      _MSG("Dpi_start_dpid: n = %d\n", n);
      if (n != 5) {
         ret = 0;
      } else {
         MSG("Dpi_start_dpid: %s\n", dStrerror(errno));
      }
   }

   return ret;
}

/*
 * Make a connection test for a UDS.
 * Return: 0 OK, 1 Not working.
 */
static int Dpi_check_uds(char *uds_name)
{
   struct sockaddr_un pun;
   int SockFD, ret = 1;

   if (access(uds_name, W_OK) == 0) {
      /* socket connection test */
      memset(&pun, 0, sizeof(struct sockaddr_un));
      pun.sun_family = AF_LOCAL;
      strncpy(pun.sun_path, uds_name, sizeof (pun.sun_path));

      if ((SockFD = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1 ||
          connect(SockFD, (struct sockaddr *) &pun, D_SUN_LEN(&pun)) == -1) {
         MSG("Dpi_check_uds: %s %s\n", dStrerror(errno), uds_name);
      } else {
         Dpi_close_fd(SockFD);
         ret = 0;
      }
   }
   return ret;
}

/*
 * Return the directory where the UDS are in,
 * NULL if it can't be found.
 */
static char *Dpi_get_dpid_uds_dir(void)
{
   FILE *in;
   char *saved_name_filename;    /*  :)  */
   char dpid_uds_dir[256], *p = NULL;

   saved_name_filename =
      dStrconcat(dGethomedir(), "/.dillo/dpi_socket_dir", NULL);
   in = fopen(saved_name_filename, "r");
   dFree(saved_name_filename);

   if (in != NULL) {
      fgets(dpid_uds_dir, 256, in);
      fclose(in);
      if ((p = strchr(dpid_uds_dir, '\n'))) {
         *p = 0;
      }
      if (access(dpid_uds_dir, F_OK) == 0) {
         p = dStrdup(dpid_uds_dir);
         _MSG("Dpi_get_dpid_uds_dir:: %s\n", p);
      }
   }

   _MSG("Dpi_get_dpid_uds_dir: %s \n", dStrerror(errno));
   return p;
}

/*
 * Return the dpid's UDS name, NULL on failure.
 */
static char *Dpi_get_dpid_uds_name(void)
{
   char *dpid_uds_dir, *dpid_uds_name = NULL;

   if ((dpid_uds_dir = Dpi_get_dpid_uds_dir()) != NULL)
      dpid_uds_name= dStrconcat(dpid_uds_dir, "/", "dpid.srs", NULL);

   dFree(dpid_uds_dir);
   return dpid_uds_name;
}

/*
 * Confirm that the dpid is running. If not, start it.
 * Return: 0 running OK, 1 starting (EAGAIN), 2 Error.
 */
static int Dpi_check_dpid(int num_tries)
{
   static int starting = 0;
   char *dpid_uds_name;
   int check_st = 1, ret = 2;

   if ((dpid_uds_name = Dpi_get_dpid_uds_name()))
      check_st = Dpi_check_uds(dpid_uds_name);

   _MSG("Dpi_check_dpid: dpid_uds_name=%s, check_st=%d\n",
        dpid_uds_name, check_st);

   if (check_st == 0) {
      /* connection test with dpi server passed */
      starting = 0;
      ret = 0;
   } else if (!dpid_uds_name || check_st) {
      if (!starting) {
         /* start dpid */
         if (Dpi_start_dpid() == 0) {
            starting = 1;
            ret = 1;
         }
      } else if (++starting < num_tries) {
         ret = 1;
      } else {
         /* we waited too much, report an error... */
         starting = 0;
      }
   }

   dFree(dpid_uds_name);
   _MSG("Dpi_check_dpid:: %s\n",
        (ret == 0) ? "OK" : (ret == 1 ? "EAGAIN" : "ERROR"));
   return ret;
}

/*
 * Confirm that the dpid is running. If not, start it.
 * Return: 0 running OK, 2 Error.
 */
static int Dpi_blocking_start_dpid(void)
{
   int cst, try = 0,
       n_tries = 12; /* 3 seconds */

   /* test the dpid, and wait a bit for it to start if necessary */
   while ((cst = Dpi_check_dpid(n_tries)) == 1) {
      MSG("Dpi_blocking_start_dpid: try %d\n", ++try);
      usleep(250000); /* 1/4 sec */
   }
   return cst;
}

/*
 * Return the UDS name of a dpi server.
 * (A query is sent to dpid and then its answer parsed)
 * note: as the available servers and/or the dpi socket directory can
 *       change at any time, we'll ask each time. If someday we find
 *       that connecting each time significantly degrades performance,
 *       an optimized approach can be tried.
 */
static char *Dpi_get_server_uds_name(const char *server_name)
{
   char *dpid_uds_dir, *dpid_uds_name = NULL,
         *server_uds_name = NULL;
   int st;

   dReturn_val_if_fail (server_name != NULL, NULL);
   _MSG("Dpi_get_server_uds_name:: server_name = [%s]\n", server_name);

   dpid_uds_dir = Dpi_get_dpid_uds_dir();
   if (dpid_uds_dir) {
      struct sockaddr_un dpid;
      int sock, req_sz, rdlen;
      char buf[128], *cmd, *request, *rply;
      size_t buflen;

      /* Get the server's uds name from dpid */
      sock = socket(AF_LOCAL, SOCK_STREAM, 0);
      dpid.sun_family = AF_LOCAL;
      dpid_uds_name = dStrconcat(dpid_uds_dir, "/", "dpid.srs", NULL);
      _MSG("dpid_uds_name = [%s]\n", dpid_uds_name);
      strncpy(dpid.sun_path, dpid_uds_name, sizeof(dpid.sun_path));

      if (connect(sock, (struct sockaddr *) &dpid, D_SUN_LEN(&dpid)) == -1)
         perror("connect");
      /* ask dpid to check the server plugin and send its UDS name back */
      request = a_Dpip_build_cmd("cmd=%s msg=%s", "check_server", server_name);
      _MSG("[%s]\n", request);
      do
         st = write(sock, request, strlen(request));
      while (st < 0 && errno == EINTR);
      if (st < 0 && errno != EINTR)
         perror("writing request");
      dFree(request);
      shutdown(sock, 1); /* signals no more writes to dpid */

      /* Get the reply */
      rply = NULL;
      buf[0] = '\0';
      buflen = sizeof(buf)/sizeof(buf[0]);
      for (req_sz = 0; (rdlen = read(sock, buf, buflen)) != 0;
           req_sz += rdlen) {
         if (rdlen == -1 && errno == EINTR)
               continue;
         if (rdlen == -1) {
            perror(" ** Dpi_get_server_uds_name **");
            break;
         }
         rply = dRealloc(rply, (uint_t)(req_sz + rdlen + 1));
         if (req_sz == 0)
            rply[0] = '\0';
         strncat(rply, buf, (size_t)rdlen);
      }
      Dpi_close_fd(sock);
      _MSG("rply = [%s]\n", rply);

      /* Parse reply */
      if (rdlen == 0 && rply) {
         cmd = a_Dpip_get_attr(rply, (int)strlen(rply), "cmd");
         if (strcmp(cmd, "send_data") == 0)
            server_uds_name = a_Dpip_get_attr(rply, (int)strlen(rply), "msg");
         dFree(cmd);
         dFree(rply);
      }
   }
   dFree(dpid_uds_dir);
   dFree(dpid_uds_name);
   _MSG("Dpi_get_server_uds_name:: %s\n", server_uds_name);
   return server_uds_name;
}


/*
 * Connect a socket to a dpi server and return the socket's FD.
 * We have to ask 'dpid' (dpi daemon) for the UDS of the target dpi server.
 * Once we have it, then the proper file descriptor is returned (-1 on error).
 */
static int Dpi_connect_socket(const char *server_name, int retry)
{
   char *server_uds_name;
   struct sockaddr_un pun;
   int SockFD, err;

   /* Query dpid for the UDS name for this server */
   server_uds_name = Dpi_get_server_uds_name(server_name);
   _MSG("server_uds_name = [%s]\n", server_uds_name);

   if (access(server_uds_name, F_OK) != 0) {
      MSG("server socket was NOT found\n");
      return -1;
   }

   /* connect with this server's socket */
   memset(&pun, 0, sizeof(struct sockaddr_un));
   pun.sun_family = AF_LOCAL;
   strncpy(pun.sun_path, server_uds_name, sizeof (pun.sun_path));
   dFree(server_uds_name);

   if ((SockFD = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1)
      perror("[dpi::socket]");
   else if (connect(SockFD, (void*)&pun, D_SUN_LEN(&pun)) == -1) {
      err = errno;
      SockFD = -1;
      MSG("[dpi::connect] errno:%d %s\n", errno, dStrerror(errno));
      if (retry) {
         switch (err) {
            case ECONNREFUSED: case EBADF: case ENOTSOCK: case EADDRNOTAVAIL:
               /* the server may crash and its socket name survive */
               unlink(pun.sun_path);
               SockFD = Dpi_connect_socket(server_name, FALSE);
               break;
         }
      }
   }

   return SockFD;
}


/*
 * CCC function for the Dpi module
 */
void a_Dpi_ccc(int Op, int Branch, int Dir, ChainLink *Info,
               void *Data1, void *Data2)
{
   dpi_conn_t *conn;
   int SockFD = -1, st;

   dReturn_if_fail( a_Chain_check("a_Dpi_ccc", Op, Branch, Dir, Info) );

   if (Branch == 1) {
      if (Dir == BCK) {
         /* Send commands to dpi-server */
         switch (Op) {
         case OpStart:
            if ((st = Dpi_blocking_start_dpid()) == 0) {
               SockFD = Dpi_connect_socket(Data1, TRUE);
               if (SockFD != -1) {
                  int *fd = dNew(int, 1);
                  *fd = SockFD;
                  Info->LocalKey = fd;
                  a_Chain_link_new(Info, a_Dpi_ccc, BCK, a_IO_ccc, 1, 1);
                  a_Chain_bcb(OpStart, Info, Info->LocalKey, NULL);
                  /* tell the capi to start the receiving branch */
                  a_Chain_fcb(OpSend, Info, Info->LocalKey, "SockFD");
               }
            }

            if (st == 0 && SockFD != -1) {
               a_Chain_fcb(OpSend, Info, NULL, "DpidOK");
            } else {
               MSG_ERR("dpi.c: can't start dpi daemon\n");
               a_Dpi_ccc(OpAbort, 1, FWD, Info, NULL, "DpidERROR");
            }
            break;
         case OpSend:
            a_Chain_bcb(OpSend, Info, Data1, NULL);
            break;
         case OpEnd:
            a_Chain_bcb(OpEnd, Info, NULL, NULL);
            dFree(Info->LocalKey);
            dFree(Info);
            break;
         case OpAbort:
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            dFree(Info->LocalKey);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* FWD */
         /* Send commands to dpi-server (status) */
         switch (Op) {
         case OpAbort:
            a_Chain_fcb(OpAbort, Info, NULL, Data2);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }

   } else if (Branch == 2) {
      if (Dir == FWD) {
         /* Receiving from server */
         switch (Op) {
         case OpSend:
            /* Data1 = dbuf */
            Dpi_process_dbuf(IORead, Data1, Info->LocalKey);
            break;
         case OpEnd:
            a_Chain_fcb(OpEnd, Info, NULL, NULL);
            Dpi_conn_free(Info->LocalKey);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      } else {  /* BCK */
         switch (Op) {
         case OpStart:
            conn = Dpi_conn_new(Info);
            Info->LocalKey = conn;

            /* Hack: for receiving HTTP through the DPI framework */
            if (strcmp(Data2, "http") == 0) {
               conn->Send2EOF = 1;
            }

            a_Chain_link_new(Info, a_Dpi_ccc, BCK, a_IO_ccc, 2, 2);
            a_Chain_bcb(OpStart, Info, NULL, Data1); /* IORead, SockFD */
            break;
         case OpAbort:
            a_Chain_bcb(OpAbort, Info, NULL, NULL);
            Dpi_conn_free(Info->LocalKey);
            dFree(Info);
            break;
         default:
            MSG_WARN("Unused CCC\n");
            break;
         }
      }
   }
}

/*! Send DpiBye to dpid
 * Note: currently disabled. Maybe it'd be better to have a
 * dpid_idle_timeout variable in the config file.
 */
void a_Dpi_bye_dpid()
{
   char *DpiBye_cmd;
   struct sockaddr_un sa;
   size_t sun_path_len, addr_len;
   char *srs_name;
   int new_socket;

   srs_name = Dpi_get_dpid_uds_name();
   sun_path_len = sizeof(sa.sun_path);

   sa.sun_family = AF_LOCAL;

   if ((new_socket = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
      MSG("a_Dpi_bye_dpid: %s\n", dStrerror(errno));
   }
   strncpy(sa.sun_path, srs_name, sizeof (sa.sun_path));
   addr_len = D_SUN_LEN(&sa);
   if (connect(new_socket, (struct sockaddr *) &sa, addr_len) == -1) {
      MSG("a_Dpi_bye_dpid: %s\n", dStrerror(errno));
      MSG("%s\n", sa.sun_path);
   }
   DpiBye_cmd = a_Dpip_build_cmd("cmd=%s", "DpiBye");
   (void) write(new_socket, DpiBye_cmd, strlen(DpiBye_cmd));
   dFree(DpiBye_cmd);
   Dpi_close_fd(new_socket);
}


/*
 * Send a command to a dpi server, and block until the answer is got.
 * Return value: the dpip tag answer as an string, NULL on error.
 */
char *a_Dpi_send_blocking_cmd(const char *server_name, const char *cmd)
{
   int cst, SockFD;
   ssize_t st;
   char buf[16384], *retval = NULL;

   /* test the dpid, and wait a bit for it to start if necessary */
   if ((cst = Dpi_blocking_start_dpid()) != 0) {
      return retval;
   }

   SockFD = Dpi_connect_socket(server_name, TRUE);
   if (SockFD != -1) {
      /* TODO: handle the case of (st < strlen(cmd)) */
      do
         st = write(SockFD, cmd, strlen(cmd));
      while (st == -1 && errno == EINTR);

      /* TODO: if the answer is too long... */
      do
         st = read(SockFD, buf, 16384);
      while (st < 0 && errno == EINTR);

      if (st == -1)
         perror("[a_Dpi_send_blocking_cmd]");
      else if (st > 0)
         retval = dStrndup(buf, (size_t)st);

      Dpi_close_fd(SockFD);

   } else {
      perror("[a_Dpi_send_blocking_cmd]");
   }

   return retval;
}

