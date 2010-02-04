/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *
 */
#include "memcached.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static Q_ITEM *q_freelist  = NULL;
static int     q_itemcount = 0;

Q_ITEM *qi_new(enum CMD_TYPE type, R_CMD *cmd)
{
    Q_ITEM *q = NULL;
    char       *key;
    int         keylen = 0;
    rel_time_t  time   = 0;

    if(q_freelist){
        q = q_freelist;
        q_freelist = q->next;
    }

    if(NULL == q){
        if(q_itemcount == Q_ITEM_MAX)
            return(NULL);
        q = malloc(sizeof(Q_ITEM));
        if (NULL == q)
            return(NULL);
        q_itemcount++;
        if (settings.verbose > 1)
            fprintf(stderr,"replication: alloc c=%d\n", q_itemcount);
    }

    switch (type) {
    case REPLICATION_SET:
    case REPLICATION_DEL:
        key    = cmd->key;
        keylen = cmd->keylen;
        break;
    case REPLICATION_DEFER_DEL:
        key    = cmd->key;
        keylen = cmd->keylen;
        time   = cmd->time;
        break;
    case REPLICATION_FLUSH_ALL:
        break;
    case REPLICATION_DEFER_FLUSH_ALL:
        time   = cmd->time;
        break;
    default:
        fprintf(stderr,"replication: got unknown command:%d\n", type);
        return(NULL);
    }

    if (keylen > 0) {
        q->key = malloc(keylen + 1);
        if(NULL == q->key){
            qi_free(q);
            q = NULL;
        }else{
            memcpy(q->key, key, keylen);
            *(q->key + keylen) = 0;
        }
    }
    q->type = type;
    q->time = time;
    q->next = NULL;

    return(q);
}

void qi_free(Q_ITEM *q)
{
    if(q){
        if(q->key){
            free(q->key);
            q->key = NULL;
        }
        q->next = q_freelist;
        q_freelist = q;
    }
}

int qi_free_list()
{
    int     c = 0;
    Q_ITEM *q = NULL;

    while(q = q_freelist){
        q_itemcount--;
        c++;
        q_freelist = q->next;
        free(q);
    }
    return(c);
}

static int replication_get_num(char *p, int n)
{
    int  l;
    char buff[64];

    sprintf(buff, "%u", n);
    l = strlen(buff);
    if(p) memcpy(p, buff, l);
    return(l);
}

int replication_call_set(char *key, size_t keylen)
{
    R_CMD r;
    r.key    = key;
    r.keylen = keylen;
    return(replication(REPLICATION_SET, &r));
}

int replication_call_del(char *key, size_t keylen)
{
    R_CMD r;
    r.key    = key;
    r.keylen = keylen;
    return(replication(REPLICATION_DEL, &r));
}

int replication_call_defer_del(char *key, size_t keylen, rel_time_t time)
{
    R_CMD r;
    r.key    = key;
    r.keylen = keylen;
    r.time   = time;
    return(replication(REPLICATION_DEFER_DEL, &r));
}

int replication_call_flush_all()
{
    R_CMD r;
    return(replication(REPLICATION_FLUSH_ALL, &r));
}

int replication_call_defer_flush_all(const rel_time_t time)
{
    R_CMD r;
    r.time   = time;
    return(replication(REPLICATION_DEFER_FLUSH_ALL, &r));
}

static int replication_del(conn *c, char *k)
{
    int   l = 0;
    char *s = "delete ";
    char *n = "\r\n";
    char *p = NULL;

    l += strlen(s);
    l += strlen(k);
    l += strlen(n);
    if(c->wsize < c->wbytes + l){
        if(p = malloc(c->wbytes + l)){
            memcpy(p, c->wbuf, c->wbytes);
            free(c->wbuf);
            c->wbuf  = p;
            c->wsize = c->wbytes + l;
        }else{
            fprintf(stderr, "replication: del malloc error\n");
            return(-1);
        }
    }
    p = c->wbuf + c->wbytes;
    memcpy(p, s, strlen(s));
    p += strlen(s);
    memcpy(p, k, strlen(k));
    p += strlen(k);
    memcpy(p, n, strlen(n));
    p += strlen(n);
    c->wbytes = p - c->wbuf;
    c->wcurr  = c->wbuf;
    return(0);
}

static int replication_defer_del(conn *c, char *k, rel_time_t exp)
{
    int   l = 0;
    char *s = "delete ";
    char *n = "\r\n";
    char *p = NULL;

    l += strlen(s);
    l += strlen(k);
    l += 1;
    l += replication_get_num(NULL, exp);
    l += strlen(n);
    if(c->wsize < c->wbytes + l){
        if(p = malloc(c->wbytes + l)){
            memcpy(p, c->wbuf, c->wbytes);
            free(c->wbuf);
            c->wbuf  = p;
            c->wsize = c->wbytes + l;
        }else{
            fprintf(stderr, "replication: del malloc error\n");
            return(-1);
        }
    }
    p = c->wbuf + c->wbytes;
    memcpy(p, s, strlen(s));
    p += strlen(s);
    memcpy(p, k, strlen(k));
    p += strlen(k);
    *(p++) = ' ';
    p += replication_get_num(p, exp);
    memcpy(p, n, strlen(n));
    p += strlen(n);
    c->wbytes = p - c->wbuf;
    c->wcurr  = c->wbuf;
    return(0);
}

static int replication_set(conn *c, item *i)
{
    int   r = 0;
    int exp = 0;
    int len = 0;
    char *s = "set ";
    char *n = "\r\n";
    char *p = NULL;

    if(i->exptime)
        exp = i->exptime + stats.started;
    len += strlen(s);
    len += i->nkey;
    len += 1;
    len += replication_get_num(NULL, 0);
    len += 1;
    len += replication_get_num(NULL, exp);
    len += 1;
    len += replication_get_num(NULL, i->nbytes - 2);
    len += strlen(n);
    len += i->nbytes;
    len += strlen(n);
    if(c->wsize < c->wbytes + len){
        if(p = malloc(c->wbytes + len)){
            memcpy(p, c->wbuf, c->wbytes);
            free(c->wbuf);
            c->wbuf  = p;
            c->wsize = c->wbytes + len;
            if (settings.verbose > 1)
                fprintf(stderr, "replication: get malloc ok\n");
        }else{
            fprintf(stderr, "replication: get malloc error\n");
            return(-1);
        }
    }
    p = c->wbuf + c->wbytes;
    memcpy(p, s, strlen(s));
    p += strlen(s);
    memcpy(p, ITEM_key(i), i->nkey);
    p += i->nkey;
    *(p++) = ' ';
    p += replication_get_num(p, 0);
    *(p++) = ' ';
    p += replication_get_num(p, exp);
    *(p++) = ' ';
    p += replication_get_num(p, i->nbytes - 2);
    memcpy(p, n, strlen(n));
    p += strlen(n);
    memcpy(p, ITEM_data(i), i->nbytes);
    p += i->nbytes;
    c->wbytes = p - c->wbuf;
    c->wcurr  = c->wbuf;
    return(0);
}

static int replication_flush_all(conn *c, rel_time_t exp)
{
    int len = 0;
    char *s = "flush_all ";
    char *n = "\r\n";
    char *p = NULL;

    len += strlen(s);
    if (exp > 0)
        len += replication_get_num(NULL, exp);
    len += strlen(n);

    if(c->wsize < c->wbytes + len){
        if(p = malloc(c->wbytes + len)){
            memcpy(p, c->wbuf, c->wbytes);
            free(c->wbuf);
            c->wbuf  = p;
            c->wsize = c->wbytes + len;
            if (settings.verbose > 1)
                fprintf(stderr, "replication: flush_all malloc ok\n");
        }else{
            fprintf(stderr, "replication: flush_all malloc error\n");
            return(-1);
        }
    }

    p = c->wbuf + c->wbytes;
    memcpy(p, s, strlen(s));
    p += strlen(s);
    if (exp > 0)
        p += replication_get_num(p, exp);
    memcpy(p, n, strlen(n));
    p += strlen(n);

    c->wbytes = p - c->wbuf;
    c->wcurr  = c->wbuf;
    return(0);
}

int replication_cmd(conn *c, Q_ITEM *q)
{
    item *i;

    switch (q->type) {
    case REPLICATION_SET:
        if(i = assoc_find(q->key, strlen(q->key)))
            return(replication_set(c, i));
        else
            return(replication_del(c, q->key));
    case REPLICATION_DEL:
        return(replication_del(c, q->key));
    case REPLICATION_DEFER_DEL:
        return(replication_defer_del(c, q->key, q->time));
    case REPLICATION_FLUSH_ALL:
        return(replication_flush_all(c, 0));
    case REPLICATION_DEFER_FLUSH_ALL:
        return(replication_flush_all(c, q->time));
    default:
        fprintf(stderr,"replication: got unknown command:%d\n", q->type);
        return(0);
    }
}

