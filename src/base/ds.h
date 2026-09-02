#ifndef BASE_DS_H
#define BASE_DS_H

#define QueuePush(head, tail, node) ((head) == 0 ? ((head) = (tail) = (node), (node)->next = 0) : ((tail)->next = (node)), (tail) = (node), (node)->next = 0)
#define QueuePop(head, tail, node) ((head) == (tail) ? (head) = 0, (tail) = 0 : (tail) = (tail)->next)

#endif // BASE_DS_H
