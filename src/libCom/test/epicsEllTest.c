/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "ellLib.h"
#include "epicsUnitTest.h"
#include "testMain.h"

struct myItem {
    ELLNODE node;
    int list;
    int num;
};

MAIN(epicsEllTest)
{
    ELLLIST list1;
    ELLLIST list2 = ELLLIST_INIT;
    int i1 = 1;
    struct myItem *pitem, *pfirst, *pick;

    testPlan(70);

    list1.count = 27;
    list1.node.next = (ELLNODE *) 0x01010101;
    list1.node.previous = (ELLNODE *) 0x10101010;

    ellInit(&list1);
    testOk1(list1.count == 0);
    testOk1(list1.node.next == NULL);
    testOk1(list1.node.previous == NULL);

    testOk1(list2.count == 0);
    testOk1(list2.node.next == NULL);
    testOk1(list2.node.previous == NULL);

    /* First build a couple lists and fill them with nodes. */
    pitem = (struct myItem *) malloc(sizeof(struct myItem));
    pitem->node.next = (ELLNODE *) 0x10101010;
    pitem->node.previous = (ELLNODE *) 0x10101010;
    pitem->list = 1;
    pitem->num = i1++;

    ellAdd(&list1, &pitem->node);

    testOk1(list1.count == 1);
    testOk1(list1.node.next == &pitem->node);
    testOk1(list1.node.previous == &pitem->node);
    testOk1(pitem->node.next == NULL);
    testOk1(pitem->node.previous == NULL);

    pfirst = pitem;
    while (i1 <= 21) {      /* put 21 nodes into LIST1 */
        pitem = (struct myItem *) malloc(sizeof(struct myItem));
        pitem->list = 1;
        pitem->num = i1++;
        ellAdd(&list1, &pitem->node);
    }

    testOk1(list1.count == 21);
    testOk1(list1.node.next == &pfirst->node);
    testOk1(list1.node.previous == &pitem->node);
    testOk1(pitem->node.next == NULL);

    testOk1(ellFirst(&list1) == &pfirst->node);
    testOk1(ellLast(&list1) == &pitem->node);
    testOk1(ellNext(&pitem->node) == NULL);
    testOk1(ellNext(&pfirst->node) == pfirst->node.next);

    testOk1(ellNth(&list1, 0) == NULL);

    pick = (struct myItem *)ellNth(&list1, 21);
    testOk1(pick == pitem);

    testOk1(ellNth(&list1, 22) == NULL);
    testOk1(ellNth(&list1, 1) == &pfirst->node);
    testOk1(ellNth(&list1, 2) == pfirst->node.next);
    testOk1(ellNth(&list1, 20) == pitem->node.previous);

    testOk1(ellPrevious(&pitem->node) == pitem->node.previous);
    testOk1(ellPrevious(&pfirst->node) == NULL);
    testOk1(ellPrevious(pfirst->node.next) == &pfirst->node);

    pick = (struct myItem *)ellGet(&list1);
    testOk1(pick == pfirst);
    testOk1(list1.node.next == pfirst->node.next);

    ellAdd(&list2, &pick->node);

    pick = (struct myItem *)ellGet(&list2);
    testOk1(pick == pfirst);
    testOk1(list2.node.next == NULL);
    testOk1(list2.node.previous == NULL);

    testOk1(ellCount(&list1) == 20);
    testOk1(ellCount(&list2) == 0);

    ellConcat(&list1, &list2);

    testOk1(ellCount(&list1) == 20);
    testOk1(ellCount(&list2) == 0);
    testOk1(list1.node.previous == &pitem->node);

    ellAdd(&list2, &pick->node);
    ellConcat(&list1, &list2);

    testOk1(ellCount(&list1) == 21);
    testOk1(ellCount(&list2) == 0);
    testOk1(list1.node.previous == &pick->node);
    testOk1(list2.node.next == NULL);
    testOk1(list2.node.previous == NULL);

    ellDelete(&list1, &pick->node);
    testOk1(ellCount(&list1) == 20);

    ellAdd(&list2, &pick->node);
    ellConcat(&list2, &list1);
    testOk1(ellFind(&list1, &pick->node) == -1);        /* empty list */
    testOk1(ellFind(&list2, &pick->node) == pick->num); /* first node */

    pick = (struct myItem *)ellNth(&list2, 18);
    testOk1(ellFind(&list2, &pick->node) == 18);        /* 18th node */

    ellDelete(&list2, &pick->node);
    ellInsert(&list2, NULL, &pick->node);               /* move #18 to top */
    testOk1(ellCount(&list2) == 21);
    testOk1(((struct myItem *)list2.node.next)->num == 18);

    testOk1(ellFind(&list2, ellNth(&list2, 21)) == 21);

    pick = (struct myItem *)ellGet(&list2);
    pitem = (struct myItem *)ellNth(&list2, 17);
    ellInsert(&list2, &pitem->node, &pick->node);
    testOk1(ellFind(&list2, ellNth(&list2, 21)) == 21);

    testOk1(((struct myItem *)ellFirst(&list2))->num == 1);
    testOk1(((struct myItem *)ellNth(&list2,17))->num == 17);
    testOk1(((struct myItem *)ellNth(&list2,18))->num == 18);

    pick = (struct myItem *)ellLast(&list2);
    pitem = (struct myItem *) malloc(sizeof(struct myItem));
    ellInsert(&list2, &pick->node, &pitem->node);
    testOk1(ellCount(&list2) == 22);
    testOk1(ellFind(&list2, ellNth(&list2, 22)) == 22);

    ellDelete(&list2, &pitem->node);
    free(pitem);

    ellExtract(&list2, ellNth(&list2,9), ellNth(&list2, 19), &list1);
    testOk1(ellCount(&list2) == 10);
    testOk1(ellCount(&list1) == 11);

    testOk1(ellFind(&list2, ellNth(&list2, 10)) == 10);
    testOk1(ellFind(&list1, ellNth(&list1, 11)) == 11);

    ellFree(&list2);
    testOk1(ellCount(&list2) == 0);

    pick = (struct myItem *)ellFirst(&list1);
    i1 = 1;
    while(pick != NULL) {
        pick->num = i1++;
        pick = (struct myItem *)ellNext(&pick->node);
    }
    pick = (struct myItem *)ellFirst(&list1);
    testOk1(pick != NULL);

    pitem = (struct myItem *)ellNStep(&pick->node, 3);
    testOk1(pitem != NULL);
    testOk1(pitem->num == 4);

    pitem = (struct myItem *)ellNStep(&pick->node, 30);
    testOk1(pitem == NULL);

    pitem = (struct myItem *)ellNStep(&pick->node, 10);
    testOk1(pitem != NULL);
    testOk1(pitem->num == 11);

    pitem = (struct myItem *)ellNStep(&pitem->node, 0);
    testOk1(pitem->num == 11);

    pitem = (struct myItem *)ellNStep(&pitem->node, -4);
    testOk1(pitem->num == 7);

    ellFree2(&list1, free);
    testOk1(ellCount(&list1) == 0);

    return testDone();
}
