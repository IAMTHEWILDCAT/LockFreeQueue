#ifndef _MSQUEUE_TESTER_H_
#define _MSQUEUE_TESTER_H_

#include <windows.h>
#include <locale>
#include <stdexcept>
#include <thread>
#include <string>
#include <time.h>
#include "MSQueue.hpp"

using namespace std;

class Reader {
	// �����, �������� ���� ������ ����� number, �������� maxIter ��� ������ �� ������� queue
	MSQueue<int>* queue; // ��������� �� ������������� ������� MSQueue ���� int
	unsigned maxIter;	 // ���������� ������� ������ pop()
	unsigned number;	 // ����� ������
	bool notSilence;	 // ����� ���������� (� �������/�����)
	double* time;		 // ��������� �� ����������, ���� ����� ���������� ����� ���������� ������ pop()
public:
	Reader(MSQueue<int>* _queue, unsigned _maxIter, unsigned _number, bool _notSilence, double* _time) {
		queue = _queue;
		number = _number;
		maxIter = _maxIter;
		notSilence = _notSilence;
		time = _time;
	}
	void operator()() {
		double time_ms = 0;
		unsigned count = maxIter - 1;
		clock_t start, stop;
		while (count) {
			start = clock();
			int* el = (queue->pop(number));
			stop = clock();
			if (el != nullptr) {
				time_ms += ((double)stop - start) * 1000.0 / CLOCKS_PER_SEC;
				if (notSilence) cout << "<--(" << number << ")" << endl;
				count--;
			}
		}
		*time += time_ms / (double)maxIter;
		return;
	}
};

class Writer {
	// �����, �������� ���� ������ ����� number, ������������ maxIter ��� ������ � ������� queue
	MSQueue<int>* queue; // ��������� �� ������������� ������� MSQueue ���� int
	unsigned maxIter;	 // ���������� ������� ������ push()
	unsigned number;	 // ����� ������
	bool notSilence;	 // ����� ���������� (� �������/�����)
	double* time;		 // ��������� �� ����������, ���� ����� ���������� ����� ���������� ������ push()
public:
	Writer(MSQueue<int>* _queue, unsigned _maxIter, unsigned _number, bool _notSilence, double* _time) {
		queue = _queue;
		number = _number;
		maxIter = _maxIter;
		notSilence = _notSilence;
		time = _time;
	}

	void operator()() {
		int* arr = (int*)calloc(maxIter, sizeof(int));
		if (arr == NULL) throw bad_alloc();

		for (int i = 0; i < maxIter; i++)
			arr[i] = i;

		double time_ms = 0;
		clock_t start, stop;
		for (unsigned i = 0; i < maxIter; i++) {
			start = clock();
			queue->push(&arr[i], number);
			stop = clock();
			time_ms += ((double)stop - start) * 1000.0 / CLOCKS_PER_SEC;
			
			if (notSilence) cout << "-->(" << number << ")" << endl;
		}
		*time += time_ms / (double)maxIter;
		free(arr);
		return;
	}
};

// ������� WINAPI ��� ������ Reader, ����������� ��������� ��������� ������ � ��������� ������
DWORD WINAPI reader(LPVOID a) {
	Reader reader = *(Reader*)a;
	reader();
	ExitThread(0);
}

// ������� WINAPI ��� ������ Writer, ����������� ��������� ��������� ������ � ��������� ������
DWORD WINAPI writer(LPVOID a) {
	Writer writer = *(Writer*)a;
	writer();
	ExitThread(0);
}

class MSQueueTests {
	// �����, ��������������� ��� ���������� ����� � ������ ������ ������������� ������� MSQueue � ���������� ������������� ������������� �������
	int epochs = 1;
	int maxItems = 256;
	unsigned short countWriteThreads = 1;
	unsigned short countReadThreads = 1;
	bool notSilence = true;

	void showLine() {
		cout << "+=================================================================================+" << endl;
	}

	short getConfig(string q, string ifError, short min, short max, short def) {
		showLine();
		cout << q << endl;
		short num = def;
		cin >> num;
		showLine();
		if (num < min || num > max) {
			cout << ifError << endl;
			num = getConfig(q, ifError, min, max, def);
		}
		return num;
	}

	double getMidTime(double* timers, unsigned count) {
		double midTimeWrite = 0;
		for (size_t i = 0; i < count; i++)
			midTimeWrite += timers[i];
		return midTimeWrite / ((double)count * epochs);
	}

	// ������������ MSQueue �� �������� � ������� ����������
	void testByParams() {
		MSQueue<int>* queue = new MSQueue<int>(countWriteThreads + countReadThreads);
		Writer* writers = (Writer*)calloc(countWriteThreads, sizeof(Writer));
		Reader* readers = (Reader*)calloc(countReadThreads, sizeof(Reader));

		double* writeTimers = (double*)calloc(countWriteThreads, sizeof(double));
		double* readTimers = (double*)calloc(countReadThreads, sizeof(double));
		
		if (queue == NULL || writers == NULL || readers == NULL || writeTimers == NULL || readTimers == NULL) throw bad_alloc();

		for (size_t i = 0; i < countWriteThreads; i++)
			writers[i] = Writer(queue, maxItems / countWriteThreads, i, notSilence, &writeTimers[i]);
		for (size_t i = 0; i < countReadThreads; i++)
			readers[i] = Reader(queue, maxItems / countReadThreads, i + countWriteThreads, notSilence, &readTimers[i]);

		for (int iep = 0; iep < epochs; iep++) {

			for (size_t i = 0; i < countWriteThreads; i++)
				CreateThread(NULL, 0, writer, &writers[i], 0, NULL);
			for (size_t i = 0; i < countReadThreads; i++) 
				CreateThread(NULL, 0, reader, &readers[i], 0, NULL);

			if (notSilence) {
				Sleep(1500);
				cout << "��������� ����� �" << iep + 1 << endl;
			}
			Sleep(1000);
			queue->clear();
		}

		cout << "+===============================      FINISH      ================================+" << endl;
		cout << "| ���������: " << countReadThreads << endl
			 << "| ���������: " << countWriteThreads << endl
			 << "|      ����: " << epochs << endl;
		if (countWriteThreads != 0) cout << "| ������� ����� PUSH: " << getMidTime(writeTimers, countWriteThreads) << " ms" << endl;
		if (countReadThreads != 0) cout << "| ������� ����� POP: " << getMidTime(readTimers, countReadThreads) << " ms" << endl;
		showLine();
		free(writeTimers);
		free(readTimers);
		free(writers);
		free(readers);
	}

	// ��������� ���������� ��� ������������ MSQueue
	void startTestByParams() {
		showLine();
		do {
			cout << "����� ���������� ������� �� ������ ���� ������ 3-� " << endl;
			countWriteThreads = getConfig("������� ���������� ������� '���������' (0-3)",
				"��������� ����� �� ����� � ��������� [0, 3]",
				0, 3, 1);
			countReadThreads = getConfig("������� ���������� ������� '���������' (0-3)",
				"��������� ����� �� ����� � ��������� [0, 3]",
				0, 3, 1);
		} while (countWriteThreads + countReadThreads > 3);

		epochs = getConfig("������� ���������� ���� [1, 1000] (������� ��� ����� �������� ����, ��� ��������� ������� �����������)",
			"��������� ����� �� ����� � ��������� [1, 1000]",
			1, 1000, 1);

		cout << "������ � ������ ������ ��������? (0-���) = ";
		cin >> notSilence;
		showLine();
		testByParams();
	}

	// �������������� ������������, ������� ���� ��������� ����������
	void autoTest() {
		notSilence = false;
		epochs = 50;
		for (unsigned i_writers = 1; i_writers <=3; i_writers++)
			for (unsigned i_readers = 0; i_readers + i_writers <= 3; i_readers++) {
				countWriteThreads = i_writers;
				countReadThreads = i_readers;
				testByParams();
			}
	}

public:
	// ����������� - ������ ������������
	MSQueueTests() {
		try {
			showLine();
			cout << "| �������� ����� ������������: 1-��������������, �����-������������� = ";
			short testMode;
			cin >> testMode;
			if (testMode != 1)
				startTestByParams();
			else
				autoTest();
		}
		catch (const exception & error) {
			cout << error.what() << endl;
		}
	}
};


#endif