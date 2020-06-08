#ifndef _HAZARD_POINTERS_H_
#define _HAZARD_POINTERS_H_

#include <atomic>
#include <vector>
#include <iostream>


template<typename T>
class HazardPointers {

private:
    static const int HP_MAX_THREADS = 128;                   // ������������ ���������� �������
    static const int HP_MAX_HPS = 4;                         // ������������ ���������� Hazard Pointers
    static const int CLPAD = 128 / sizeof(std::atomic<T*>);  // ������
    static const int HP_THRESHOLD_R = 0;                     // ������ ������
    static const int MAX_RETIRED = HP_MAX_THREADS * HP_MAX_HPS; // ������������ ���������� ��������� �������� � ������

    const int maxHPs;
    const int maxThreads;

    // Hazard Pointers
    std::atomic<T*>* hp[HP_MAX_THREADS];
    // �������. ������ �� ��� ����� �����. ��� ���������� ��� ��������� ������ �������.
    std::vector<T*> retiredList[HP_MAX_THREADS * CLPAD];

public:
    // �����������
    HazardPointers(int maxHPs = HP_MAX_HPS, int maxPtrs = HP_MAX_THREADS) : maxHPs{ maxHPs }, maxThreads{ maxPtrs } {
        for (int iptr = 0; iptr < HP_MAX_THREADS; iptr++) {
            // �� �������� HP_MAX_HPS ����� ����, 
            // ����� ���������� ���������� ����� hp ���������� ��� ������� ������ �������
            hp[iptr] = new std::atomic<T*>[CLPAD * 2]; 
            for (int ihp = 0; ihp < HP_MAX_HPS; ihp++)
                hp[iptr][ihp].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~HazardPointers() {
        for (int iptr = 0; iptr < HP_MAX_THREADS; iptr++) {
            delete[] hp[iptr];
            // ������� ��������� �����
            for (unsigned iret = 0; iret < retiredList[iptr * CLPAD].size(); iret++)
                delete retiredList[iptr * CLPAD][iret];
        }
    }

    // ������� ���� ���������� ������ tid
    void clear(const int tid) {
        for (int ihp = 0; ihp < maxHPs; ihp++)
            hp[tid][ihp].store(nullptr, std::memory_order_release);
    }

    // �������� ihp ��������� ��� ������ tid
    void clearOne(int ihp, const int tid) {
        hp[tid][ihp].store(nullptr, std::memory_order_release);
    }

    // ������ (����������) index ��������� ������ tid
    T* protect(int index, const std::atomic<T*>& atom, const int tid) {
        T* n = nullptr;
        T* ret;
        while ((ret = atom.load()) != n) {
            hp[tid][index].store(ret);
            n = ret;
        }
        return ret;
    }

    // ��������� �������� �������, �� ������� ��������� ptr, � HazardPointer ������ tid  
    T* protectPtr(int index, T* ptr, const int tid) {
        hp[tid][index].store(ptr);
        return ptr;
    }

    // ��������� �������� �������, �� ������� ��������� ptr, � �������� ����������� memory_order_release, � HazardPointer ������ tid  
    T* protectRelease(int index, T* ptr, const int tid) {
        hp[tid][index].store(ptr, std::memory_order_release);
        return ptr;
    }

    // �������� ������ �� retiredList ��� ������ tid
    void retire(T* ptr, const int tid) {
        retiredList[tid * CLPAD].push_back(ptr);

        if (retiredList[tid * CLPAD].size() < HP_THRESHOLD_R) return;
        
        for (unsigned iret = 0; iret < retiredList[tid * CLPAD].size(); ) {
            auto obj = retiredList[tid * CLPAD][iret];
            bool canDelete = true;
            for (int tid = 0; tid < maxThreads && canDelete; tid++)
                for (int ihp = maxHPs - 1; ihp >= 0; ihp--)
                    if (hp[tid][ihp].load() == obj) {
                        canDelete = false;
                        break;
                    }
            if (canDelete) {
                retiredList[tid * CLPAD].erase(retiredList[tid * CLPAD].begin() + iret);
                delete obj;
                continue;
            }
            iret++;
        }
    }
};

#endif