#include <cstdint>

// Это "магическая" строчка, которая заставляет C++ не ломать имена функций,
// чтобы внешние программы могли их найти[cite: 50].
extern "C" {
    
    // Переменная для хранения ключа прячется внутри библиотеки[cite: 54].
    static char current_key = 0; 

    // Функция 1: Запоминает ключ[cite: 9].
    void set_key(char key) { 
        current_key = key;
    }

    // Функция 2: Само шифрование[cite: 10].
    void caesar(void* src, void* dst, int len) { 
        char* s = static_cast<char*>(src);
        char* d = static_cast<char*>(dst);
        
        // Проходимся по каждому символу и применяем операцию XOR [cite: 6]
        for (int i = 0; i < len; i++) {
            d[i] = s[i] ^ current_key; 
        }
    }
}