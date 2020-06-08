#ifndef _MS_QUEUE_HP_H_
#define _MS_QUEUE_HP_H_

#include <atomic>
#include <stdio.h>
#include <stdexcept>
#include "HazardPointers.hpp"


template<typename T>
class MSQueue {
    /* 
    // Класс неблокирующей очереди (Lock-Free Queue). Очередь построена на односвязном списке. 
    // Каждый элемент списка Node содержит ссылку на хранимые в нём данные и 
    // атомарный указатель на следующий элемент списка. 
    // Голова списка является фиктивным элементом.
    */
private:
    struct Node {
        T* item;                    // Указатель на данные
        std::atomic<Node*> next;    // Атомарный указатель на следующий элемент

        Node(T* userItem) : item{ userItem }, next{ nullptr } { } // Конструктор

        // CAS (compare and swap). 
        // Функция производит сравнение next указателя с cmp, 
        // и если они равны - обменивает next и val местами
        bool casNext(Node* cmp, Node* val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    // CAS (compare and swap). 
    // Функция производит сравнение указателя на хвост tail и cmp, 
    // и если они равны - обменивает tail и val местами
    bool casTail(Node* cmp, Node* val) {
        return tail.compare_exchange_strong(cmp, val);
    }

    // CAS (compare and swap). 
    // Функция производит сравнение указателя на голову head и cmp, 
    // и если они равны - обменивает head и val местами
    bool casHead(Node* cmp, Node* val) {
        return head.compare_exchange_strong(cmp, val);
    }

    // Указатили на голову и хвост
    // Используется alignas(2^6). Это выравнивание по границе, а именно адрес, который кратен заданной степени двойки.
    // Это повышает скорость работы с указателями.
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;

    // Максимальное коичество
    static const int MAX_THREADS = 128;
    const int maxThreads;

    // Создаем два Hazard Pointers для метода извлечения
    HazardPointers<Node> hp{ 4, maxThreads };
    const int kHpTail = 0;
    const int kHpHead = 0;
    const int kHpNext = 1;

public:
    // Конструктор
    MSQueue(int maxThreads = MAX_THREADS) : maxThreads{ maxThreads } {
        Node* sentinelNode = new Node(nullptr);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }

    // Деструктор
    ~MSQueue() {
        while (pop(0) != nullptr);     // Полностью извлекаем содержимое очереди
        delete head.load();            // Очищаем память
    }

    bool isEmpty() {
        return (head == tail);
    }

    // Включение нового элемента item из потока tid в очередь
    void push(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        
        Node* newNode = new Node(item);
        while (true) {
            Node* ltail = hp.protectPtr(kHpTail, tail, tid);
            if (ltail == tail.load()) {
                Node* lnext = ltail->next.load();
                if (lnext == nullptr){ 
                    if (ltail->casNext(nullptr, newNode)) {
                        // Это последний узел => добавляем сюда newNode и пробуем переместить хвост в newNode
                        casTail(ltail, newNode);
                        hp.clear(tid);
                        return;     // Элемент успешно добавлен
                    }
                } else casTail(ltail, lnext);
            }
        }
    }

    // Извлечение элемента из очереди
    T* pop(const int tid) {
        Node* node = hp.protect(kHpHead, head, tid);
        while (node != tail.load()) {
            Node* lnext = hp.protect(kHpNext, node->next, tid);
            if (casHead(node, lnext)) {
                // Другой поток может очистить lnext после текущего clear()
                T* item = lnext->item;  
                hp.clear(tid);
                hp.retire(node, tid);
                return item;
            }
            node = hp.protect(kHpHead, head, tid);
        }
        hp.clear(tid);
        return nullptr;     // Очередь пуста     
    }

    void clear() {
        while (pop(0) != nullptr);
    }
};

#endif