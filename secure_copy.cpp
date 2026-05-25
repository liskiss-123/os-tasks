#include <iostream>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <queue>
#include <time.h>
#include <iomanip>
#include <string>

#define WORKERS_COUNT 4

extern "C" {
    void set_key(char key);
    void caesar(void* src, void* dst, int len);
}

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

struct Task {
    std::string in_file;
    std::string out_file;
    double duration;
};

std::queue<Task*> task_queue;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void process_file(Task* task) {
    double start_time = get_time();

    int fd_in = open(task->in_file.c_str(), O_RDONLY);
    int fd_out = open(task->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    if (fd_in >= 0 && fd_out >= 0) {
        char buffer[8192];
        while (true) {
            ssize_t bytes = read(fd_in, buffer, sizeof(buffer));
            if (bytes <= 0) break;
            
            caesar(buffer, buffer, bytes);
            
            ssize_t w = write(fd_out, buffer, bytes);
            (void)w;
        }
    }
    
    if (fd_in >= 0) close(fd_in);
    if (fd_out >= 0) close(fd_out);

    task->duration = get_time() - start_time;
}

void* worker_thread(void*) {
    while (true) {
        Task* my_task = nullptr;

        pthread_mutex_lock(&queue_mutex);
        if (!task_queue.empty()) {
            my_task = task_queue.front();
            task_queue.pop();
        }
        pthread_mutex_unlock(&queue_mutex);

        if (my_task == nullptr) break;

        process_file(my_task);
    }
    return nullptr;
}

double run_parallel(std::vector<Task>& tasks) {
    double start_total = get_time();

    for (auto& task : tasks) {
        task_queue.push(&task);
    }

    pthread_t workers[WORKERS_COUNT];
    int active_workers = std::min((int)tasks.size(), WORKERS_COUNT);

    for (int i = 0; i < active_workers; i++) {
        pthread_create(&workers[i], nullptr, worker_thread, nullptr);
    }

    for (int i = 0; i < active_workers; i++) {
        pthread_join(workers[i], nullptr);
    }

    return get_time() - start_total;
}

double run_sequential(std::vector<Task>& tasks) {
    double start_total = get_time();
    for (auto& task : tasks) {
        process_file(&task);
    }
    return get_time() - start_total;
}

void print_stats(const std::string& mode, double total_time, const std::vector<Task>& tasks) {
    double sum_time = 0;
    for (const auto& t : tasks) sum_time += t.duration;
    double avg_time = sum_time / tasks.size();

    std::cout << "\n=== Статистика (" << mode << ") ===\n";
    std::cout << "Обработано файлов: " << tasks.size() << "\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Общее время работы: " << total_time << " сек\n";
    std::cout << "Среднее время на файл: " << avg_time << " сек\n";
    std::cout << "=================================\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Использование: ./secure_copy [--mode=sequential|parallel] <ключ> <in1> <out1> ...\n";
        return 1;
    }

    std::string mode = "auto";
    int start_arg = 1;

    if (std::string(argv[1]).find("--mode=") == 0) {
        mode = std::string(argv[1]).substr(7);
        start_arg = 2;
    }

    set_key(argv[start_arg][0]);
    start_arg++;

    std::vector<Task> tasks;
    for (int i = start_arg; i < argc; i += 2) {
        if (i + 1 < argc) {
            tasks.push_back({argv[i], argv[i+1], 0.0});
        }
    }

    int file_count = tasks.size();

    if (mode == "auto") {
        std::cout << "Автоматический выбор режима. Количество файлов: " << file_count << ".\n";
        
        if (file_count < 5) {
            std::cout << "Эвристика: выбрана ПОСЛЕДОВАТЕЛЬНАЯ обработка (< 5 файлов).\n";
            double t_seq = run_sequential(tasks);
            print_stats("Sequential", t_seq, tasks);
        } else {
            std::cout << "Эвристика: выбрана ПАРАЛЛЕЛЬНАЯ обработка (>= 5 файлов).\n";
            std::cout << "[Для сравнения запускаем оба режима...]\n";
            
            double t_seq = run_sequential(tasks);
            double t_par = run_parallel(tasks);
            
            print_stats("Sequential (Альтернатива)", t_seq, tasks);
            print_stats("Parallel (Выбрано)", t_par, tasks);
            
            std::cout << "\nРАЗНИЦА: Параллельный режим быстрее на " << (t_seq - t_par) << " сек!\n";
        }
    } 
    else if (mode == "sequential") {
        double t = run_sequential(tasks);
        print_stats("Sequential", t, tasks);
    } 
    else if (mode == "parallel") {
        double t = run_parallel(tasks);
        print_stats("Parallel", t, tasks);
    }

    return 0;
}