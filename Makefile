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
	@echo "Создаем тестовый файл 10 МБ..."
	dd if=/dev/urandom of=input_10mb.bin bs=1M count=10
	@echo "Шифруем 10 МБ через secure_copy..."
	./$(APP_NAME) input_10mb.bin encrypted_10mb.bin K
	@echo "Дешифруем обратно..."
	./$(APP_NAME) encrypted_10mb.bin decrypted_10mb.bin K
	@echo "Проверка завершена. Если ошибок нет, файлы идентичны."

clean:
	rm -f $(LIB_NAME) $(APP_NAME) input.txt encrypted.bin decrypted.txt input_10mb.bin encrypted_10mb.bin decrypted_10mb.bin