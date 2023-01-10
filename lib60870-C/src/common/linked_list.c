/*
 *  Copyright 2013-2022 Michael Zillgith
 *
 *  This file is part of lib60870-C
 *
 *  lib60870-C is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870-C is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870-C.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#include "linked_list.h"
#include "lib_memory.h"

#ifndef CONFIG_COMPILE_WITHOUT_COMMON

LinkedList
LinkedList_getLastElement(LinkedList list)
{
    while (list->next != NULL) {
        list = list->next;
    }
    return list;
}

LinkedList
LinkedList_create()
{
    LinkedList newList;

    newList = (LinkedList) GLOBAL_MALLOC(sizeof(struct sLinkedList));
    newList->data = NULL;
    newList->next = NULL;

    return newList;
}

/**
 * Destroy list (free). Also frees element data with helper function.
 */
void
LinkedList_destroyDeep(LinkedList list, LinkedListValueDeleteFunction valueDeleteFunction)
{
    LinkedList nextElement = list;
    LinkedList currentElement;

    do {
        currentElement = nextElement;
        nextElement = currentElement->next;

        if (currentElement->data != NULL)
            valueDeleteFunction(currentElement->data);

        GLOBAL_FREEMEM(currentElement);
    }
    while (nextElement != NULL);
}

void
LinkedList_destroy(LinkedList list)
{
    LinkedList_destroyDeep(list, Memory_free);
}

/**
 * Destroy list (free) without freeing the element data
 */
void
LinkedList_destroyStatic(LinkedList list)
{
    LinkedList nextElement = list;
    LinkedList currentElement;

    do {
        currentElement = nextElement;
        nextElement = currentElement->next;
        GLOBAL_FREEMEM(currentElement);
    }
    while (nextElement != NULL);
}

int
LinkedList_size(LinkedList list)
{
    LinkedList nextElement = list;
    int size = 0;

    while (nextElement->next != NULL) {
        nextElement = nextElement->next;
        size++;
    }

    return size;
}

void
LinkedList_add(LinkedList list, void* data)
{
    LinkedList newElement = LinkedList_create();

    newElement->data = data;

    LinkedList listEnd = LinkedList_getLastElement(list);

    listEnd->next = newElement;
}

bool
LinkedList_remove(LinkedList list, const void* data)
{
    LinkedList lastElement = list;

    LinkedList currentElement = list->next;

    while (currentElement != NULL) {
        if (currentElement->data == data) {
            lastElement->next = currentElement->next;
            GLOBAL_FREEMEM(currentElement);
            return true;
        }

        lastElement = currentElement;
        currentElement = currentElement->next;
    }

    return false;
}

LinkedList
LinkedList_insertAfter(LinkedList list, void* data)
{
    LinkedList originalNextElement = LinkedList_getNext(list);

    LinkedList newElement = LinkedList_create();

    newElement->data = data;
    newElement->next = originalNextElement;

    list->next = newElement;

    return newElement;
}

LinkedList
LinkedList_getNext(LinkedList list)
{
    return list->next;
}

LinkedList
LinkedList_get(LinkedList list, int index)
{
    LinkedList element = LinkedList_getNext(list);

    int i = 0;

    while (i < index) {
        element = LinkedList_getNext(element);

        if (element == NULL)
            return NULL;

        i++;
    }

    return element;
}

void*
LinkedList_getData(LinkedList self)
{
    return self->data;
}

#endif

