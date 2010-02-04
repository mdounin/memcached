#ifndef MEMCACHED_REPLICATION_H
#define MEMCACHED_REPLICATION_H
#include <netdb.h>
#define REP_BACKUP 0
#define REP_MASTER 1
#define Q_ITEM_MAX 1024

enum CMD_TYPE {
  REPLICATION_SET,
  REPLICATION_DEL,
  REPLICATION_DEFER_DEL,
  REPLICATION_FLUSH_ALL,
  REPLICATION_DEFER_FLUSH_ALL,
};

typedef struct queue_item_t Q_ITEM;
struct queue_item_t {
  enum CMD_TYPE  type;
  char          *key;
  rel_time_t     time;
  Q_ITEM        *next;
};

typedef struct replication_cmd_t R_CMD;
struct replication_cmd_t {
  char       *key;
  int         keylen;
  rel_time_t  time;
};

Q_ITEM *qi_new(enum CMD_TYPE type, R_CMD *cmd);
void    qi_free(Q_ITEM *);
int     qi_free_list();
int     replication_cmd(conn *, Q_ITEM *);

#endif
