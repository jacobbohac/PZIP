#ifndef NODE_H
#define NODE_H

#include "inc.h"

/* The 'node' module implements basic doubly-linked lists with headers. */
/*                                                                      */
/* 'Node' is an abstract parent class which is subclassed by SeeState,  */
/* Deterministic_Node and Deterministic_Context.                                            */
/*                                                                      */
/* Except of course that we don't have true subclassing in C so we fake */
/* it by abusing the cast operator. :)                                  */

typedef struct Node Node;
struct Node {
    Node* next;
    Node* prev;
}; 

/* NB: The 'znode_*' macros are logically private to this file        */
/* -- they are never directly referenced anywhere else:               */
/*     znode_Init: Set node to empty list (points to self both ways). */
/*     znode_Cut:  Remove node from linklist.                         */
/*     znode_Fix:  Set node's neighbors to point back to it.          */
/*     znode_Add:  Insert 'node' as first element of 'list'.          */

#define znode_Init(list)     do { (list)->next = (list); (list)->prev = (list); } while(0)
#define znode_Cut(node)      do { (node)->prev->next = (node)->next; (node)->next->prev = (node)->prev; znode_Init(node); } while(0)
#define znode_Fix(node)      do { (node)->prev->next = (node); (node)->next->prev = (node); } while(0)
#define znode_Add(list,node) do { (node)->prev = (list); (node)->next = (list)->next; node_Fix(node); } while(0)

/* The next four simply cast arg to Node* and call the matching fn above: */
#define node_Init(list)            znode_Init((Node*)(list))
#define node_Cut(node)             znode_Cut((Node*)(node))
#define node_Fix(node)             znode_Fix((Node*)(node))
#define node_Add(list,node)        znode_Add((Node*)list,(Node*)node)

#define node_Next(node)           (void*)(((Node*)node)->next)


Node* node_Cut_Tail( Node* node );


#endif  /* NODE_H */

