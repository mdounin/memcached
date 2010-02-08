#ifndef MEMCACHED_REPLICATION_H
#define MEMCACHED_REPLICATION_H
#define REPCACHED_VERSION "2.2"
#include <netdb.h>

enum CMD_TYPE {
  REPLICATION_REP,
  REPLICATION_DEL,
  REPLICATION_DEFER_DEL,
  REPLICATION_FLUSH_ALL,
  REPLICATION_DEFER_FLUSH_ALL,
  REPLICATION_MARUGOTO_END,
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

Q_ITEM *qi_new(enum CMD_TYPE type, R_CMD *cmd, bool);
void    qi_free(Q_ITEM *);
int     qi_free_list();
int     replication_cmd(conn *, Q_ITEM *);
int     get_qi_count();

#endif
