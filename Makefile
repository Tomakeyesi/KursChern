CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -DSERVER_TESTING
LDFLAGS = -lssl -lcrypto -lpthread
TEST_LDFLAGS = -lUnitTest++ -lssl -lcrypto -lpthread

SOURCES = main.cpp server.cpp
TARGET = server
TEST_SOURCE = test.cpp
TEST_TARGET = test_server

# Сборка основного сервера
$(TARGET): $(SOURCES) server.h
	$(CXX) $(SOURCES) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)

# Сборка тестов с UnitTest++
$(TEST_TARGET): $(TEST_SOURCE) server.cpp server.h
	@echo "Создание тестовых файлов..."
	@echo "user:P@ssW0rd" > test_auth_db.txt
	@echo "alice:password456" >> test_auth_db.txt
	@echo "bob:secret789" >> test_auth_db.txt
	@touch empty_test.txt
	@echo "user1pass1" > invalid_format.txt
	@echo "user2:pass2" >> invalid_format.txt
	@echo ":pass3" >> invalid_format.txt
	@echo "user4:" >> invalid_format.txt
	@echo "user5:pass5" >> invalid_format.txt
	$(CXX) $(TEST_SOURCE) server.cpp -o $(TEST_TARGET) $(CXXFLAGS) $(TEST_LDFLAGS)

# Генерация документации Doxygen
doxygen:
	@echo "Генерация документации Doxygen..."
	@doxygen Doxyfile 2>/dev/null || echo "Установите Doxygen для генерации документации"

# Запуск модульных тестов
test: $(TEST_TARGET)
	@echo "=== ЗАПУСК МОДУЛЬНЫХ ТЕСТОВ С UNITTEST++ ==="
	./$(TEST_TARGET)

# Запуск функциональных тестов
functional_test: $(TARGET)
	@echo "=== ЗАПУСК ФУНКЦИОНАЛЬНЫХ ТЕСТОВ ==="
	@chmod +x test_functional.sh 2>/dev/null || true
	@./test_functional.sh

# Очистка
clean:
	rm -f $(TARGET) $(TEST_TARGET)
	rm -f test_auth_db.txt empty_test.txt invalid_format.txt
	rm -f *.log
	rm -rf log
	rm -rf html latex

.PHONY: clean test functional_test doxygen
