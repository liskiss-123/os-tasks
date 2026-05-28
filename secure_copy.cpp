#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <queue>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <random>

namespace fs = std::filesystem;

#pragma pack(push, 1)
struct FileRecordHeader {
    uint32_t file_size;
    uint32_t name_length;
    uint8_t salt[16];
};
#pragma pack(pop)

class RC4 {
private:
    uint8_t S[256];
    int i, j;

public:
    RC4(const std::string& master_key, const uint8_t salt[16]) {
        std::vector<uint8_t> key(master_key.begin(), master_key.end());
        key.insert(key.end(), salt, salt + 16);

        for (int k = 0; k < 256; k++) S[k] = k;
        i = j = 0;
        int k_len = key.size();
        for (int k = 0, j_tmp = 0; k < 256; k++) {
            j_tmp = (j_tmp + S[k] + key[k % k_len]) % 256;
            std::swap(S[k], S[j_tmp]);
        }
    }

    void process(uint8_t* data, size_t len) {
        for (size_t k = 0; k < len; k++) {
            i = (i + 1) % 256;
            j = (j + S[i]) % 256;
            std::swap(S[i], S[j]);
            data[k] ^= S[(S[i] + S[j]) % 256];
        }
    }
};

struct AddTask {
    std::string abs_path;
    std::string img_path;
};

std::queue<AddTask> task_queue;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t img_file_mutex = PTHREAD_MUTEX_INITIALIZER;

std::string global_key;
std::string global_img_path;

void generate_salt(uint8_t* salt) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    for (int i = 0; i < 16; i++) salt[i] = dis(gen);
}

void* add_worker(void*) {
    while (true) {
        AddTask task;
        pthread_mutex_lock(&queue_mutex);
        if (task_queue.empty()) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }
        task = task_queue.front();
        task_queue.pop();
        pthread_mutex_unlock(&queue_mutex);

        uint32_t file_size = fs::file_size(task.abs_path);
        uint32_t name_len = task.img_path.length();
        
        FileRecordHeader header;
        header.file_size = file_size;
        header.name_length = name_len;
        generate_salt(header.salt);

        size_t total_record_size = sizeof(FileRecordHeader) + name_len + file_size;

        pthread_mutex_lock(&img_file_mutex);
        int img_fd = open(global_img_path.c_str(), O_RDWR | O_CREAT, 0644);
        off_t my_offset = lseek(img_fd, 0, SEEK_END);
        ftruncate(img_fd, my_offset + total_record_size);
        close(img_fd);
        pthread_mutex_unlock(&img_file_mutex);

        img_fd = open(global_img_path.c_str(), O_WRONLY);
        
        pwrite(img_fd, &header, sizeof(header), my_offset);
        my_offset += sizeof(header);
        
        pwrite(img_fd, task.img_path.c_str(), name_len, my_offset);
        my_offset += name_len;

        int in_fd = open(task.abs_path.c_str(), O_RDONLY);
        RC4 rc4(global_key, header.salt);
        
        uint8_t buffer[8192];
        while (true) {
            ssize_t bytes = read(in_fd, buffer, sizeof(buffer));
            if (bytes <= 0) break;
            rc4.process(buffer, bytes);
            pwrite(img_fd, buffer, bytes, my_offset);
            my_offset += bytes;
        }
        
        close(in_fd);
        close(img_fd);
    }
    return nullptr;
}

void do_add(int argc, char* argv[]) {
    for (int i = 6; i < argc; i++) {
        std::string target = argv[i];
        if (!fs::exists(target)) continue;

        if (fs::is_directory(target)) {
            for (const auto& entry : fs::recursive_directory_iterator(target)) {
                if (entry.is_regular_file()) {
                    task_queue.push({entry.path().string(), entry.path().string()});
                }
            }
        } else {
            task_queue.push({target, target});
        }
    }

    int num_threads = std::min((int)task_queue.size(), 5);
    pthread_t workers[5];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&workers[i], nullptr, add_worker, nullptr);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(workers[i], nullptr);
    }
    std::cout << "Успешно добавлено в образ.\n";
}

void do_list() {
    int fd = open(global_img_path.c_str(), O_RDONLY);
    if (fd < 0) return;

    FileRecordHeader header;
    std::cout << "=== Содержимое образа ===\n";
    while (read(fd, &header, sizeof(header)) == sizeof(header)) {
        std::string name(header.name_length, '\0');
        read(fd, &name[0], header.name_length);
        
        std::cout << name << " (" << header.file_size << " байт)\n";
        
        lseek(fd, header.file_size, SEEK_CUR);
    }
    close(fd);
}

void do_get(const std::string& out_file, const std::string& target_name) {
    int fd = open(global_img_path.c_str(), O_RDONLY);
    if (fd < 0) return;

    // Очищаем искомую строку от возможных Windows-символов
    std::string clean_target = target_name;
    while (!clean_target.empty() && (clean_target.back() == '\r' || clean_target.back() == '\n')) {
        clean_target.pop_back();
    }

    FileRecordHeader header;
    while (read(fd, &header, sizeof(header)) == sizeof(header)) {
        std::string name(header.name_length, '\0');
        read(fd, &name[0], header.name_length);

        // Очищаем найденную строку
        std::string clean_name = name;
        while (!clean_name.empty() && (clean_name.back() == '\r' || clean_name.back() == '\n')) {
            clean_name.pop_back();
        }

        if (clean_name == clean_target) {
            int out_fd = open(out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            RC4 rc4(global_key, header.salt);
            uint8_t buffer[8192];
            uint32_t remaining = header.file_size;

            while (remaining > 0) {
                uint32_t to_read = std::min(remaining, (uint32_t)sizeof(buffer));
                read(fd, buffer, to_read);
                rc4.process(buffer, to_read);
                write(out_fd, buffer, to_read);
                remaining -= to_read;
            }
            close(out_fd);
            close(fd);
            std::cout << "Файл извлечен.\n";
            return;
        } else {
            lseek(fd, header.file_size, SEEK_CUR);
        }
    }
    close(fd);
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    std::string mode = argv[1];

    if (mode == "-add" && argc >= 6) {
        global_key = argv[3];
        global_img_path = argv[5];
        do_add(argc, argv);
    } 
    else if (mode == "-list" && argc == 4) {
        global_img_path = argv[3];
        do_list();
    } 
    else if (mode == "-get" && argc >= 8) {
        global_img_path = argv[3];
        global_key = argv[5];
        std::string out_file = argv[7];
        std::string target_file = argv[8];
        do_get(out_file, target_file);
    }

    return 0;
}