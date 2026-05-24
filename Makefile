CXX = g++
# Обязательные флаги, которые требует преподаватель [cite: 49]
CXXFLAGS = -Wall -Wextra -pedantic -fPIC 
LDFLAGS = -shared
LIB_NAME = libcaesar.so

# Цель 1: собирает саму библиотеку 
all: $(LIB_NAME) 

$(LIB_NAME): caesar.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

# Цель 2: копирует файл в системную папку линукса 
install: $(LIB_NAME) 
	cp $(LIB_NAME) /usr/local/lib/
	ldconfig

# Цель 3: автоматически проверяет, работает ли код [cite: 16]
test: all 
	@echo "Создаем файл..."
	@echo "Hello, OS!" > input.txt
	@echo "Шифруем..."
	python3 test.py ./$(LIB_NAME) K input.txt encrypted.bin
	@echo "Дешифруем..."
	python3 test.py ./$(LIB_NAME) K encrypted.bin decrypted.txt
	@echo "Результат (должен совпасть с изначальным):"
	cat decrypted.txt

clean:
	rm -f $(LIB_NAME) input.txt encrypted.bin decrypted.txt