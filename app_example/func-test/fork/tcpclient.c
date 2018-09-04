#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEBUG

#ifdef DEBUG
#define	DBG(fmt, arg...) do { \
	printf(fmt, ##arg); \
} while(0)
#else
#define	DBG(fmt, arg...) ((void)0)
#endif

static struct sockaddr_in g_dest;
static struct sockaddr_in g_src;
int srcPort = 0;
int destPort = 0;
int times = 0;

void
random_str (char *str, const int len)
{
  static const char alphaNum[] =
    "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "0123456789";
  int i = 0;

  for (i = 0; i < len; i++)
    {
      str[i] = alphaNum[rand () % (sizeof (alphaNum) - 1)];
    }

  str[len] = 0;
}

static void
setArgsDefault ()
{

  memset (&g_dest, 0, sizeof (g_dest));
  g_dest.sin_family = AF_INET;
  g_dest.sin_addr.s_addr = inet_addr ("127.0.0.1");
  g_dest.sin_port = htons (12345);
  bzero (&(g_dest.sin_zero), 8);

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
  const char *optstring = "p:d:s:a:t:";

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
        case 'p':
          g_dest.sin_port = htons (atoi (optarg));
          break;
        case 'd':
          g_dest.sin_addr.s_addr = inet_addr (optarg);
          break;
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

  int clientSocket, ret, i;
  char sndbuffer[1024];
  char rcvbuffer[1024];
  int result = 0;

  /*
   * check command line arguments
   */
  if (0 != process_arg (argc, argv))
    {
      DBG ("Error in argument.%d\n", argc);
      exit (1);
    }

  clientSocket = socket (AF_INET, SOCK_STREAM, 0);
  if (clientSocket < 0)
    {
      DBG ("Error in connection.\n");
      exit (1);
    }
  DBG ("[INFO]Client Socket is created.\n");

  ret = bind (clientSocket, (struct sockaddr *) &g_src, sizeof (g_src));
  if (ret < 0)
    {
      DBG ("Error in binding.\n");
      exit (1);
    }

  DBG ("[INFO]Bind to client aaddress port 0\n");

  ret = connect (clientSocket, (struct sockaddr *) &g_dest, sizeof (g_dest));
  if (ret < 0)
    {
      DBG ("Error in connection.\n");
      exit (1);
    }
  DBG ("[INFO]Connected to Server.\n");

  memset (sndbuffer, '\0', 1024 * sizeof (char));
  memset (rcvbuffer, '\0', 1024 * sizeof (char));

  for (i = 1; i <= times; i++)
    {
      DBG ("Client: \t");
      random_str (sndbuffer, 50);
      send (clientSocket, sndbuffer, strlen (sndbuffer), 0);

      if (recv (clientSocket, rcvbuffer, 1024, 0) < 0)
        {
          DBG ("Error in receiving data.\n");
        }
      else
        {
          DBG ("Server: \t%s\n", rcvbuffer);
        }
      if (0 != strcmp (sndbuffer, rcvbuffer))
        {
          result = -1;
          break;
        }
    }

  /* Send exit message to server */
  strcpy (sndbuffer, "#exit");
  send (clientSocket, sndbuffer, strlen (sndbuffer), 0);

  DBG ("Result = %s\n", (result == 0) ? "Success" : "Fail");
  close (clientSocket);
  DBG ("Disconnecting from server.\n");
  return 0;
}
