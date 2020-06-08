#ifndef _MS_QUEUE_HP_H_
#define _MS_QUEUE_HP_H_

#include <atomic>
#include <stdio.h>
#include <stdexcept>
#include "HazardPointers.hpp"


template<typename T>
class MSQueue {
    /* 
    // ����� ������������� ������� (Lock-Free Queue). ������� ��������� �� ����������� ������. 
    // ������ ������� ������ Node �������� ������ �� �������� � �� ������ � 
    // ��������� ��������� �� ��������� ������� ������. 
    // ������ ������ �������� ��������� ���������.
    */
private:
    struct Node {
        T* item;                    // ��������� �� ������
        std::atomic<Node*> next;    // ��������� ��������� �� ��������� �������

        Node(T* userItem) : item{ userItem }, next{ nullptr } { } // �����������

        // CAS (compare and swap). 
        // ������� ���������� ��������� next ��������� � cmp, 
        // � ���� ��� ����� - ���������� next � val �������
        bool casNext(Node* cmp, Node* val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    // CAS (compare and swap). 
    // ������� ���������� ��������� ��������� �� ����� tail � cmp, 
    // � ���� ��� ����� - ���������� tail � val �������
    bool casTail(Node* cmp, Node* val) {
        return tail.compare_exchange_strong(cmp, val);
    }

    // CAS (compare and swap). 
    // ������� ���������� ��������� ��������� �� ������ head � cmp, 
    // � ���� ��� ����� - ���������� head � val �������
    bool casHead(Node* cmp, Node* val) {
        return head.compare_exchange_strong(cmp, val);
    }

    // ��������� �� ������ � �����
    // ������������ alignas(2^6). ��� ������������ �� �������, � ������ �����, ������� ������ �������� ������� ������.
    // ��� �������� �������� ������ � �����������.
    alignas(128) std::atomic<Node*> head;
    alignas(128) std::atomic<Node*> tail;

    // ������������ ���������
    static const int MAX_THREADS = 128;
    const int maxThreads;

    // ������� ��� Hazard Pointers ��� ������ ����������
    HazardPointers<Node> hp{ 4, maxThreads };
    const int kHpTail = 0;
    const int kHpHead = 0;
    const int kHpNext = 1;

public:
    // �����������
    MSQueue(int maxThreads = MAX_THREADS) : maxThreads{ maxThreads } {
        Node* sentinelNode = new Node(nullptr);
        head.store(sentinelNode, std::memory_order_relaxed);
        tail.store(sentinelNode, std::memory_order_relaxed);
    }

    // ����������
    ~MSQueue() {
        while (pop(0) != nullptr);     // ��������� ��������� ���������� �������
        delete head.load();            // ������� ������
    }

    bool isEmpty() {
        return (head == tail);
    }

    // ��������� ������ �������� item �� ������ tid � �������
    void push(T* item, const int tid) {
        if (item == nullptr) throw std::invalid_argument("item can not be nullptr");
        
        Node* newNode = new Node(item);
        while (true) {
            Node* ltail = hp.protectPtr(kHpTail, tail, tid);
            if (ltail == tail.load()) {
                Node* lnext = ltail->next.load();
                if (lnext == nullptr){ 
                    if (ltail->casNext(nullptr, newNode)) {
                        // ��� ��������� ���� => ��������� ���� newNode � ������� ����������� ����� � newNode
                        casTail(ltail, newNode);
                        hp.clear(tid);
                        return;     // ������� ������� ��������
                    }
                } else casTail(ltail, lnext);
            }
        }
    }

    // ���������� �������� �� �������
    T* pop(const int tid) {
        Node* node = hp.protect(kHpHead, head, tid);
        while (node != tail.load()) {
            Node* lnext = hp.protect(kHpNext, node->next, tid);
            if (casHead(node, lnext)) {
                // ������ ����� ����� �������� lnext ����� �������� clear()
                T* item = lnext->item;  
                hp.clear(tid);
                hp.retire(node, tid);
                return item;
            }
            node = hp.protect(kHpHead, head, tid);
        }
        hp.clear(tid);
        return nullptr;     // ������� �����     
    }

    void clear() {
        while (pop(0) != nullptr);
    }
};

#endif