#ifndef DB_H_
#define DB_H_

#include <pthread.h>

typedef struct node {
    char *name;
    char *value;
    struct node *lchild;
    struct node *rchild;
    pthread_rwlock_t rwlock;
} node_t;

extern node_t head;

node_t *search(char *name, node_t *parent, node_t **parentp, int lt);

void db_query(char *name, char *result, int len);

int db_add(char *name, char *value);

int db_remove(char *name);

void interpret_command(char *command, char *response, int resp_capacity);

int db_print(char *filename);

void db_cleanup(void);

#endif
