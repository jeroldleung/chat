#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 1000
#define SERVER_PORT 8080
#define BACKLOG 512
#define NICKLEN 32

struct client {
  int fd;
  char nick[NICKLEN];
};

struct chatstate {
  int socket;
  int numclients;
  int maxclientfd;
  struct client *clients[MAX_CLIENTS];
};

struct chatstate *chat;

void create_client(int clientfd) {
  struct client *c;
  c = (struct client *)malloc(sizeof(*c));
  if (c == NULL) {
    perror("Failed to create a new client\n");
    exit(1);
  }

  memset(c, 0, sizeof(*c));
  c->fd = clientfd;
  snprintf(c->nick, NICKLEN, "user#%d", clientfd);

  // update chat state
  chat->clients[clientfd] = c;
  chat->numclients += 1;
  if (clientfd > chat->maxclientfd) {
    chat->maxclientfd = clientfd;
  }
}

void free_client(struct client *c) {
  chat->clients[c->fd] = NULL;
  chat->numclients -= 1;
  if (chat->maxclientfd > c->fd) {
    goto freeit;
  }

  // Otherwise, update the maximum client fd
  for (int i = chat->maxclientfd - 1; i >= 0; i--) {
    if (chat->clients[i]) {
      chat->maxclientfd = chat->clients[i]->fd;
      goto freeit;
    }
  }
  chat->maxclientfd = -1; // no clients

freeit:
  close(c->fd);
  free(c);
}

void send_to_other_clients(int sender, char *msg, size_t len) {
  for (int i = 0; i <= chat->maxclientfd; i++) {
    if (chat->clients[i] == NULL || chat->clients[i]->fd == sender) {
      continue;
    }
    write(chat->clients[i]->fd, msg, len);
  }
}

int create_tcpserver() {
  int s;
  struct sockaddr_in addr;

  // create a socket
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    return -1;
  }

  // initialize the address structure
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVER_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // bind the address to socket and listen for incoming connections
  if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
      listen(s, BACKLOG) == -1) {
    close(s);
    return -1;
  }

  return s;
}

void init_chat() {
  // allocate memory for the chat
  chat = (struct chatstate *)malloc(sizeof(*chat));
  if (chat == NULL) {
    perror("Failed to allocate memory for chat");
    exit(1);
  }

  // initialize the chat
  memset(chat, 0, sizeof(*chat));
  chat->numclients = 0;
  chat->maxclientfd = -1;
  chat->socket = create_tcpserver();
  if (chat->socket == -1) {
    perror("Failed to create tcp server");
    exit(1);
  }
}

int main() {
  init_chat();
  printf("Chat server has started up, waiting for connections...\n");

  for (;;) {
    fd_set readfds;
    struct timeval t;
    int nready, maxfd;
    char buf[256];

    t.tv_sec = 1;
    t.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(chat->socket, &readfds);
    for (int i = 0; i <= chat->maxclientfd; i++) {
      if (chat->clients[i]) {
        FD_SET(i, &readfds);
      }
    }

    // the maximum file descriptor for select system call is either the chat
    // socket itself or one of our clients
    maxfd = chat->socket > chat->maxclientfd ? chat->socket : chat->maxclientfd;
    nready = select(maxfd + 1, &readfds, NULL, NULL, &t);
    if (nready == -1) {
      perror("Failed to call select system call\n");
      exit(1);
    }

    // new connection pending to accept
    if (FD_ISSET(chat->socket, &readfds)) {
      int fd;
      struct sockaddr_in addr;
      socklen_t len = sizeof(addr);

      if ((fd = accept(chat->socket, (struct sockaddr *)&addr, &len)) == -1) {
        perror("Failed to accept new connection\n");
        exit(1);
      }
      create_client(fd);
      char *msg = "Welcome to the chat!\n";
      write(fd, msg, strlen(msg));
      printf("Connected client fd: %d\n", fd);
    }

    // check if there are pending data clients sent us
    for (int i = 0; i <= chat->maxclientfd; i++) {
      if (chat->clients[i] == NULL) {
        continue;
      }
      if (FD_ISSET(i, &readfds)) {
        char msg[256];
        int n, len;
        n = read(i, buf, sizeof(buf));
        buf[n] = 0;
        if (n <= 0) {
          printf("Disconnected client fd: %d\n", i);
          free_client(chat->clients[i]);
          continue;
        }
        len = snprintf(msg, sizeof(msg), "%s> %s", chat->clients[i]->nick, buf);
        printf("%s", msg);
        send_to_other_clients(i, msg, len);
      }
    }
  }
}
