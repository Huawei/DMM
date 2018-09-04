#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

static struct sockaddr_in g_src;
int srcPort = 0;
int destPort = 0;
int times = 0;

#ifdef DEBUG
#define	DBG(fmt, arg...) do { \
	DBG(fmt, ##arg); \
} while(0)
#else
#define	DBG(fmt, arg...) ((void)0)
#endif

static void
setArgsDefault ()
{
  memset (&g_src, 0, sizeof (g_src));
  g_src.sin_family = AF_INET;
  g_src.sin_addr.s_addr = inet_addr ("0.0.0.0");
  g_src.sin_port = htons (7895);
  bzero (&(g_src.sin_zero), 8);

  times = 1;
}

static int
process_arg (int argc, char *argv[])
{
  int opt = 0;
  int error = 0;
  const char *optstring = "s:a:t:";

  if (argc < 5)
    {
      DBG
        ("Param info :-p dest_portid; -d dest_serverIP; -a src_portid; -s src_clientIP; -t times; \n");
      return -1;
    }
  setArgsDefault ();
  while ((opt = getopt (argc, argv, optstring)) != -1)
    {
      switch (opt)
        {
        case 's':
          g_src.sin_addr.s_addr = inet_addr (optarg);
          break;
        case 'a':
          g_src.sin_port = htons (atoi (optarg));
          break;
        case 't':
          times = atoi (optarg);
          break;
        }
    }
  return 0;
}

int
main (int argc, char *argv[])
{

  int sockfd, ret;
  int newSocket;
  socklen_t addr_size;
  static struct sockaddr_in accept_addr;
  char buffer[1024];
  pid_t childpid;

  /*
   * check command line arguments
   */
  if (0 != process_arg (argc, argv))
    {
      DBG ("Error in argument.%d\n", argc);
      exit (1);
    }

  sockfd = socket (AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      DBG ("Error in connection.\n");
      exit (1);
    }
  DBG ("Server Socket is created. %d\n", sockfd);

  ret = bind (sockfd, (struct sockaddr *) &g_src, sizeof (g_src));
  if (ret < 0)
    {
      DBG ("Error in binding.\n");
      exit (1);
    }

  DBG ("Bind sucess port %d\n", g_src.sin_port);

  if (listen (sockfd, 10) == 0)
    {
      DBG ("Listening on %s....\n", inet_ntoa (g_src.sin_addr));
    }
  else
    {
      DBG ("Error in binding.\n");
    }

  while (1)
    {
      newSocket =
        accept (sockfd, (struct sockaddr *) &accept_addr, &addr_size);
      if (newSocket < 0)
        {
          DBG ("Error: Exiting here pid %d", getpid ());
          exit (1);
        }

      DBG ("Connection accepted from %s:%d fd %d\n",
           inet_ntoa (accept_addr.sin_addr),
           ntohs (accept_addr.sin_port), newSocket);
      if ((childpid = fork ()) == 0)
        {
          DBG ("[ PID %d] Child process Created. Pid %d \r\n", getpid (),
               getpid ());
          DBG ("[ PID %d] Closing fd %d\n", getpid (), sockfd);
          close (sockfd);

          while (1)
            {
              memset (buffer, '\0', 1024 * sizeof (char));
              recv (newSocket, buffer, 1024, 0);
              if (strcmp (buffer, "#exit") == 0)
                {
                  DBG ("Disconnected from %s:%d\n",
                       inet_ntoa (newAddr.sin_addr),
                       ntohs (newAddr.sin_port));
                  break;
                }
              else
                {
                  DBG ("[PID %d]Client: %s\n", getpid (), buffer);
                  send (newSocket, buffer, strlen (buffer), 0);
                  bzero (buffer, sizeof (buffer));
                }
            }

          DBG ("[PID %d]Closing socket %d\r\n", getpid (), newSocket);
          close (newSocket);
        }

    }

  DBG ("[PID %d]Process exiting... %d\r\n", getpid (), getpid ());
  return 0;
}
