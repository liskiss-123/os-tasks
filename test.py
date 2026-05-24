import sys
import ctypes

def main():
    # Проверяем, что нам передали 4 аргумента [cite: 19]
    if len(sys.argv) != 5:
        print("Использование: python3 test.py <библиотека> <ключ> <входной_файл> <выходной_файл>")
        sys.exit(1)

    lib_path = sys.argv[1] # [cite: 20]
    key_char = sys.argv[2] # [cite: 21]
    in_file = sys.argv[3]  # [cite: 22]
    out_file = sys.argv[4] # [cite: 23]

    # Загружаем нашу скомпилированную плюсовую библиотеку 
    lib = ctypes.CDLL(lib_path)
    
    # Объясняем Питону, какие типы данных принимают наши функции из C++
    lib.set_key.argtypes = [ctypes.c_char]
    lib.caesar.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]

    # 1. Устанавливаем ключ
    lib.set_key(key_char.encode('utf-8')[:1])

    # 2. Читаем исходный файл
    with open(in_file, 'rb') as f:
        data = f.read()

    # 3. Создаем буфер данных (чтобы шифровать прямо в нем [cite: 32])
    buffer = ctypes.create_string_buffer(data)
    
    # 4. Вызываем функцию шифрования
    lib.caesar(buffer, buffer, len(data))

    # 5. Сохраняем результат
    with open(out_file, 'wb') as f:
        f.write(buffer.raw)

if __name__ == "__main__":
    main()