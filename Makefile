CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -pthread -std=c++17
APP_NAME = secure_copy

all: $(APP_NAME)

$(APP_NAME): secure_copy.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test: $(APP_NAME)
	@rm -rf test_dir disk.img result.txt
	@mkdir -p test_dir/sub_dir
	@echo "Secret1" > test_dir/file1.txt
	@echo "Secret2" > test_dir/sub_dir/file2.txt
	./$(APP_NAME) -add -key "mykey" -image disk.img test_dir/
	./$(APP_NAME) -list -image disk.img
	./$(APP_NAME) -get -image disk.img -key "mykey" -out result.txt test_dir/sub_dir/file2.txt
	@cat result.txt

clean:
	rm -rf $(APP_NAME) test_dir disk.img result.txt