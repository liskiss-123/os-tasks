CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -fPIC -pthread
LDFLAGS = -shared
LIB_NAME = libcaesar.so
APP_NAME = secure_copy

all: $(LIB_NAME) $(APP_NAME)

$(LIB_NAME): caesar.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

$(APP_NAME): secure_copy.cpp $(LIB_NAME)
	$(CXX) $(CXXFLAGS) -o $@ $< -L. -lcaesar -Wl,-rpath=.

install: $(LIB_NAME)
	cp $(LIB_NAME) /usr/local/lib/
	ldconfig

test: all
	@echo "Очищаем старые логи..."
	rm -f copy_log.txt
	@echo "Создаем два тестовых файла..."
	@echo "Первый файл" > file1.txt
	@echo "Второй файл" > file2.txt
	@echo "Запускаем многопоточную обработку..."
	./$(APP_NAME) K file1.txt out1.bin file2.txt out2.bin
	@echo "Дешифруем обратно..."
	./$(APP_NAME) K out1.bin dec1.txt out2.bin dec2.txt
	@echo "Смотрим лог-файл (copy_log.txt):"
	cat copy_log.txt

clean:
	rm -f $(LIB_NAME) $(APP_NAME) *.txt *.bin copy_log.txt