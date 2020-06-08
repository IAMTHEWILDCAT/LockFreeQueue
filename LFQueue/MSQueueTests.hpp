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
	// Класс, играющий роль потока номер number, читающий maxIter раз данные из очереди queue
	MSQueue<int>* queue; // Указатель на неблокирующую очередь MSQueue типа int
	unsigned maxIter;	 // Количество вызовов метода pop()
	unsigned number;	 // Номер потока
	bool notSilence;	 // Режим выполнения (С выводом/Тихий)
	double* time;		 // Указатель на переменную, куда нужно записывать время выполнения метода pop()
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
	// Класс, играющий роль потока номер number, записывающий maxIter раз данные в очередь queue
	MSQueue<int>* queue; // Указатель на неблокирующую очередь MSQueue типа int
	unsigned maxIter;	 // Количество вызовов метода push()
	unsigned number;	 // Номер потока
	bool notSilence;	 // Режим выполнения (С выводом/Тихий)
	double* time;		 // Указатель на переменную, куда нужно записывать время выполнения метода push()
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

// Обертка WINAPI для класса Reader, позволяющая создавать экземпляр класса в отдельном потоке
DWORD WINAPI reader(LPVOID a) {
	Reader reader = *(Reader*)a;
	reader();
	ExitThread(0);
}

// Обертка WINAPI для класса Writer, позволяющая создавать экземпляр класса в отдельном потоке
DWORD WINAPI writer(LPVOID a) {
	Writer writer = *(Writer*)a;
	writer();
	ExitThread(0);
}

class MSQueueTests {
	// Класс, предназначенный для проведения теста и оценки работы неблокирующей очереди MSQueue в нескольких настраиваемых пользователем режимах
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

	// Тестирование MSQueue по заданным в обьекте параметрам
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
				cout << "Завершена Эпоха №" << iep + 1 << endl;
			}
			Sleep(1000);
			queue->clear();
		}

		cout << "+===============================      FINISH      ================================+" << endl;
		cout << "| Читателей: " << countReadThreads << endl
			 << "| Писателей: " << countWriteThreads << endl
			 << "|      Эпох: " << epochs << endl;
		if (countWriteThreads != 0) cout << "| Среднее время PUSH: " << getMidTime(writeTimers, countWriteThreads) << " ms" << endl;
		if (countReadThreads != 0) cout << "| Среднее время POP: " << getMidTime(readTimers, countReadThreads) << " ms" << endl;
		showLine();
		free(writeTimers);
		free(readTimers);
		free(writers);
		free(readers);
	}

	// Настройка параметров для тестирования MSQueue
	void startTestByParams() {
		showLine();
		do {
			cout << "Общее количество потоков не должно быть больше 3-х " << endl;
			countWriteThreads = getConfig("Введите количество потоков 'Писателей' (0-3)",
				"Выбранное число не лежит в дмапазоне [0, 3]",
				0, 3, 1);
			countReadThreads = getConfig("Введите количество потоков 'Читателей' (0-3)",
				"Выбранное число не лежит в дмапазоне [0, 3]",
				0, 3, 1);
		} while (countWriteThreads + countReadThreads > 3);

		epochs = getConfig("Введите количество эпох [1, 1000] (сколько раз будет проведен тест, для получения средних показателей)",
			"Выбранное число не лежит в диапазоне [1, 1000]",
			1, 1000, 1);

		cout << "Запуск в режиме вывода действий? (0-Нет) = ";
		cin >> notSilence;
		showLine();
		testByParams();
	}

	// Автоматическое тестирование, перебор всех возможных параметров
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
	// Конструктор - начало тестирования
	MSQueueTests() {
		try {
			showLine();
			cout << "| Выберите режим тестирования: 1-Автоматическое, Иначе-настраиваемое = ";
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