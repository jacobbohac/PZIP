
#include <stdlib.h>
#include <string.h>
#include "node.h"

Node* node_Cut_Tail( Node* list ) {
    assert( list );
    {   Node* node = list->prev;
        if (node == list)   return NULL;
        node_Cut( node );
        return node;
    }
}

