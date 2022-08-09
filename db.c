#include "./db.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLEN 256

node_t head = {"", "", 0, 0};

void lock(node_t *node, int lt) {
    if (lt == 0) {
        pthread_rwlock_rdlock(&node->rwlock);
    } else {
        pthread_rwlock_wrlock(&node->rwlock);
    }
}

node_t *node_constructor(char *arg_name, char *arg_value, node_t *arg_left,
                         node_t *arg_right) {
    size_t name_len = strlen(arg_name);
    size_t val_len = strlen(arg_value);

    if (name_len > MAXLEN || val_len > MAXLEN) return 0;

    node_t *new_node = (node_t *)malloc(sizeof(node_t));

    if (new_node == 0) return 0;

    if ((new_node->name = (char *)malloc(name_len + 1)) == 0) {
        free(new_node);
        return 0;
    }

    if ((new_node->value = (char *)malloc(val_len + 1)) == 0) {
        free(new_node->name);
        free(new_node);
        return 0;
    }

    if ((snprintf(new_node->name, MAXLEN, "%s", arg_name)) < 0) {
        free(new_node->value);
        free(new_node->name);
        free(new_node);
        return 0;
    } else if ((snprintf(new_node->value, MAXLEN, "%s", arg_value)) < 0) {
        free(new_node->value);
        free(new_node->name);
        free(new_node);
        return 0;
    }

    new_node->lchild = arg_left;
    new_node->rchild = arg_right;
    return new_node;
}

void node_destructor(node_t *node) {
    if (node->name != 0) free(node->name);
    if (node->value != 0) free(node->value);
    free(node);
}

void db_query(char *name, char *result, int len) {
    node_t *target;
    lock(&head, 0);
    target = search(name, &head, 0, 0);

    if (target == 0) {
        snprintf(result, len, "not found");
        return;
    } else {
        snprintf(result, len, "%s", target->value);
        pthread_rwlock_unlock(&target->rwlock);
        return;
    }
}

int db_add(char *name, char *value) {
    node_t *parent;
    node_t *target;
    node_t *newnode;
    lock(&head, 1);
    if ((target = search(name, &head, &parent, 1)) != 0) {
        pthread_rwlock_unlock(&target->rwlock);
        pthread_rwlock_unlock(&parent->rwlock);
        return (0);
    }

    newnode = node_constructor(name, value, 0, 0);
    pthread_rwlock_init(&newnode->rwlock, 0);
    if (strcmp(name, parent->name) < 0)
        parent->lchild = newnode;
    else
        parent->rchild = newnode;
    pthread_rwlock_unlock(&parent->rwlock);
    return (1);
}

int db_remove(char *name) {
    node_t *parent;
    node_t *dnode;
    node_t *next;

    lock(&head, 1);
    if ((dnode = search(name, &head, &parent, 1)) == 0) {
        pthread_rwlock_unlock(&parent->rwlock);
        return (0);
    }

    if (dnode->rchild == 0) {
        if (strcmp(dnode->name, parent->name) < 0)
            parent->lchild = dnode->lchild;
        else
            parent->rchild = dnode->lchild;

        pthread_rwlock_unlock(&parent->rwlock);
        pthread_rwlock_unlock(&dnode->rwlock);
        node_destructor(dnode);
    } else if (dnode->lchild == 0) {
        if (strcmp(dnode->name, parent->name) < 0)
            parent->lchild = dnode->rchild;
        else
            parent->rchild = dnode->rchild;

        pthread_rwlock_unlock(&parent->rwlock);
        pthread_rwlock_unlock(&dnode->rwlock);
        node_destructor(dnode);
    } else {
        pthread_rwlock_unlock(&parent->rwlock);
        next = dnode->rchild;
        node_t **pnext = &dnode->rchild;
        lock(next, 1);
        while (next->lchild != 0) {
            node_t *nextl = next->lchild;
            lock(nextl, 1);

            pthread_rwlock_unlock(&next->rwlock);
            pnext = &next->lchild;
            next = nextl;
        }

        dnode->name = realloc(dnode->name, strlen(next->name) + 1);
        dnode->value = realloc(dnode->value, strlen(next->value) + 1);

        snprintf(dnode->name, MAXLEN, "%s", next->name);
        snprintf(dnode->value, MAXLEN, "%s", next->value);
        *pnext = next->rchild;

        pthread_rwlock_unlock(&dnode->rwlock);
        pthread_rwlock_unlock(&next->rwlock);
        node_destructor(next);
    }

    return (1);
}

node_t *search(char *name, node_t *parent, node_t **parentpp, int lt) {
    node_t *next;
    node_t *result;

    if (strcmp(name, parent->name) < 0) {
        next = parent->lchild;
    } else {
        next = parent->rchild;
    }

    lock(next, lt);
    pthread_rwlock_unlock(&parent->rwlock);
    if (next == NULL) {
        result = NULL;
    } else {
        if (strcmp(name, next->name) == 0) {
            result = next;
        } else {
            pthread_rwlock_unlock(&next->rwlock);
            return search(name, next, parentpp, lt);
        }
    }

    if (parentpp != NULL) {
        *parentpp = parent;
    }
    return result;
}

static inline void print_spaces(int lvl, FILE *out) {
    for (int i = 0; i < lvl; i++) {
        fprintf(out, " ");
    }
}

void db_print_recurs(node_t *node, int lvl, FILE *out) {
    print_spaces(lvl, out);

    if (node == NULL) {
        fprintf(out, "(null)\n");
        return;
    }

    lock(node, 0);
    if (node == &head) {
        fprintf(out, "(root)\n");
    } else {
        fprintf(out, "%s %s\n", node->name, node->value);
    }

    db_print_recurs(node->lchild, lvl + 1, out);
    db_print_recurs(node->rchild, lvl + 1, out);
    pthread_rwlock_unlock(&node->rwlock);
}

int db_print(char *filename) {
    FILE *out;
    if (filename == NULL) {
        db_print_recurs(&head, 0, stdout);
        return 0;
    }

    while (isspace(*filename)) {
        filename++;
    }

    if (*filename == '\0') {
        db_print_recurs(&head, 0, stdout);
        return 0;
    }

    if ((out = fopen(filename, "w+")) == NULL) {
        return -1;
    }

    db_print_recurs(&head, 0, out);
    fclose(out);

    return 0;
}

void db_cleanup_recurs(node_t *node) {
    if (node == NULL) {
        return;
    }

    db_cleanup_recurs(node->lchild);
    db_cleanup_recurs(node->rchild);

    node_destructor(node);
}

void db_cleanup() {
    db_cleanup_recurs(head.lchild);
    db_cleanup_recurs(head.rchild);
}

void interpret_command(char *command, char *response, int len) {
    char value[MAXLEN];
    char ibuf[MAXLEN];
    char name[MAXLEN];
    int sscanf_ret;

    if (strlen(command) <= 1) {
        snprintf(response, len, "ill-formed command");
        return;
    }

    switch (command[0]) {
        case 'q':

            sscanf_ret = sscanf(&command[1], "%255s", name);
            if (sscanf_ret < 1) {
                snprintf(response, len, "ill-formed command");
                return;
            }
            db_query(name, response, len);
            if (strlen(response) == 0) {
                snprintf(response, len, "not found");
            }

            return;

        case 'a':

            sscanf_ret = sscanf(&command[1], "%255s %255s", name, value);
            if (sscanf_ret < 2) {
                snprintf(response, len, "ill-formed command");
                return;
            }
            if (db_add(name, value)) {
                snprintf(response, len, "added");
            } else {
                snprintf(response, len, "already in database");
            }

            return;

        case 'd':

            sscanf_ret = sscanf(&command[1], "%255s", name);
            if (sscanf_ret < 1) {
                snprintf(response, len, "ill-formed command");
                return;
            }
            if (db_remove(name)) {
                snprintf(response, len, "removed");
            } else {
                snprintf(response, len, "not in database");
            }

            return;

        case 'f':

            sscanf_ret = sscanf(&command[1], "%255s", name);
            if (sscanf_ret < 1) {
                snprintf(response, len, "ill-formed command");
                return;
            }

            FILE *finput = fopen(name, "r");
            if (!finput) {
                snprintf(response, len, "bad file name");
                return;
            }
            while (fgets(ibuf, sizeof(ibuf), finput) != 0) {
                pthread_testcancel();
                interpret_command(ibuf, response, len);
            }
            fclose(finput);
            snprintf(response, len, "file processed");
            return;

        default:
            snprintf(response, len, "ill-formed command");
            return;
    }
}
