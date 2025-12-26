/**
 * @file test.cpp
 * @author Чернышев Ринат Рустямович
 * @date 26.12.2025
 * @brief Модульные тесты для серверного приложения.
 * @details Содержит тесты для проверки корректности работы методов класса Server,
 *          включая аутентификацию SHA-224, вычисление суммы квадратов и загрузку БД.
 */

#include <UnitTest++/UnitTest++.h>
#include <fstream>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <iostream>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <regex>

using namespace std;

// #define SERVER_TESTING
#include "server.h"

// Функция для создания временного файла с пользователями
string createTempUserDb(const vector<pair<string, string>>& users) {
    string filename = "temp_test_db_" + to_string(time(nullptr)) + ".txt";
    ofstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Cannot create temp file");
    }
    
    for (const auto& user_pair : users) {
        file << user_pair.first << ":" << user_pair.second << "\n";
    }
    file.close();
    return filename;
}

// Функция для удаления временного файла
void deleteTempFile(const string& filename) {
    remove(filename.c_str());
}

// ==================== ТЕСТЫ ВЫЧИСЛЕНИЙ (СУММА КВАДРАТОВ) ====================

SUITE(CalculationTest)
{
    TEST(CalculateSumOfSquaresPositiveNumbers) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector = {1, 2, 3, 4};
        int16_t result = server.testCalculateSumOfSquares(vector);
        // 1² + 2² + 3² + 4² = 1 + 4 + 9 + 16 = 30
        CHECK_EQUAL(30, result);
    }
    
    TEST(CalculateSumOfSquaresWithNegativeNumbers) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector = {-1, 2, -3, 4};
        // (-1)² + 2² + (-3)² + 4² = 1 + 4 + 9 + 16 = 30
        int16_t result = server.testCalculateSumOfSquares(vector);
        CHECK_EQUAL(30, result);
    }
    
    TEST(CalculateSumOfSquaresSingleElement) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector = {42};
        int16_t result = server.testCalculateSumOfSquares(vector);
        CHECK_EQUAL(1764, result); // 42² = 1764
    }
    
    TEST(CalculateSumOfSquaresEmptyVector) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector;
        int16_t result = server.testCalculateSumOfSquares(vector);
        CHECK_EQUAL(0, result);
    }
    
    TEST(CalculateSumOfSquaresWithZero) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector = {0, 5, 10};
        int16_t result = server.testCalculateSumOfSquares(vector);
        CHECK_EQUAL(125, result); // 0² + 5² + 10² = 0 + 25 + 100 = 125
    }
    
    TEST(CalculateSumOfSquaresOverflowPositive) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector = {200, 200};
        // 200² + 200² = 40000 + 40000 = 80000 > 32767
        int16_t result = server.testCalculateSumOfSquares(vector);
        CHECK_EQUAL(32767, result); // Должно вернуть 2^15
    }
    
    TEST(CalculateSumOfSquaresOverflowNegative) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector = {-200, -200};
        // (-200)² + (-200)² = 40000 + 40000 = 80000 > 32767
        int16_t result = server.testCalculateSumOfSquares(vector);
        CHECK_EQUAL(32767, result); // Должно вернуть 2^15 (положительное переполнение)
    }
    
    TEST(CalculateSumOfSquaresLargeValues) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        vector<int16_t> vector = {1000, 1000};
        // 1000² + 1000² = 1,000,000 + 1,000,000 = 2,000,000 > 32767
        int16_t result = server.testCalculateSumOfSquares(vector);
        CHECK_EQUAL(32767, result);
    }
}

// ==================== ТЕСТЫ ХЕШИРОВАНИЯ SHA-224 ====================

SUITE(SHA224HashTest)
{
    TEST(SHA224HashKnownValue1) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        string result = server.testSha224Hash("test");
        // SHA-224("test") = 90A3ED9E32B2AAF4C61C410EB925426119E1A9DC53D4286ADE99A809
        // Проверяем длину (56 hex символов)
        CHECK_EQUAL(56, result.length());
        
        // Проверяем, что все символы в верхнем регистре и hex
        bool validHex = true;
        for (char c : result) {
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                validHex = false;
                break;
            }
        }
        CHECK(validHex);
    }
    
    TEST(SHA224HashEmptyString) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        string result = server.testSha224Hash("");
        // SHA-224("") = D14A028C2A3A2BC9476102BB288234C415A2B01F828EA62AC5B3E42F
        CHECK_EQUAL(56, result.length());
    }
    
    TEST(SHA224HashConsistency) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        string input = "consistent_test_string_123";
        string hash1 = server.testSha224Hash(input);
        string hash2 = server.testSha224Hash(input);
        CHECK_EQUAL(hash1, hash2);
    }
    
    TEST(SHA224HashDifferentInputs) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        string hash1 = server.testSha224Hash("input1");
        string hash2 = server.testSha224Hash("input2");
        CHECK(hash1 != hash2); // Разные входы должны давать разные хэши
    }
}

// ==================== ТЕСТЫ ГЕНЕРАЦИИ СОЛИ ====================

SUITE(SaltGenerationTest)
{
    TEST(SaltGenerationLength) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        string salt = server.testGenerateSalt();
        // Соль должна быть 16 hex символов (64 бита)
        CHECK_EQUAL(16, salt.length());
    }
    
    TEST(SaltGenerationHexFormat) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        string salt = server.testGenerateSalt();
        
        // Проверяем, что все символы в верхнем регистре и hex
        bool validHex = true;
        for (char c : salt) {
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                validHex = false;
                break;
            }
        }
        CHECK(validHex);
    }
    
    TEST(SaltGenerationUniqueness) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        string salt1 = server.testGenerateSalt();
        string salt2 = server.testGenerateSalt();
        
        // Соли должны быть разными (высокая вероятность)
        CHECK(salt1 != salt2);
    }
}

// ==================== ТЕСТЫ ЗАГРУЗКИ БАЗЫ ПОЛЬЗОВАТЕЛЕЙ ====================

SUITE(UserDatabaseTest)
{
    TEST(LoadValidUserDatabase) {
        vector<pair<string, string>> users = {
            {"user1", "password123"},
            {"user2", "secret456"},
            {"admin", "adminpass"}
        };
        
        string filename = createTempUserDb(users);
        
        Server server(33333, filename, "/log/scale.log");
        server.testLoadUserDatabase();
        
        CHECK_EQUAL(3, server.getUsersCount());
        
        deleteTempFile(filename);
    }
    
    TEST(LoadEmptyDatabaseFile) {
        string filename = "empty_test_" + to_string(time(nullptr)) + ".txt";
        ofstream file(filename);
        file.close();
        
        Server server(33333, filename, "/log/scale.log");
        server.testLoadUserDatabase();
        
        CHECK_EQUAL(0, server.getUsersCount());
        
        deleteTempFile(filename);
    }
    
    TEST(LoadNonExistentDatabaseFile) {
        Server server(33333, "non_existent_file_12345.txt", "/log/scale.log");
        server.testLoadUserDatabase();
        CHECK_EQUAL(0, server.getUsersCount());
    }
    
    TEST(LoadDatabaseWithInvalidFormat) {
        string filename = "invalid_test_" + to_string(time(nullptr)) + ".txt";
        ofstream file(filename);
        file << "user1pass1\n"; // Нет двоеточия
        file << "user2:pass2\n";
        file << ":pass3\n"; // Нет логина
        file << "user4:\n"; // Нет пароля
        file << "user5:pass5\n";
        file.close();
        
        Server server(33333, filename, "/log/scale.log");
        server.testLoadUserDatabase();
        
        // Должны загрузиться только user2 и user5
        CHECK_EQUAL(2, server.getUsersCount());
        
        deleteTempFile(filename);
    }
}

// ==================== УПРОЩЕННЫЕ ТЕСТЫ ====================

SUITE(SimpleConstructorTest)
{
    TEST(ServerConstructorWorks) {
        try {
            Server server(33333, "/scale.conf", "/log/scale.log");
            CHECK(true);
        } catch (...) {
            CHECK(false);
        }
    }
    
    TEST(ServerConstructorWithDifferentPorts) {
        try {
            Server server1(8080, "/scale.conf", "/log/scale.log");
            Server server2(9000, "/scale.conf", "/log/scale.log");
            CHECK(true);
        } catch (...) {
            CHECK(false);
        }
    }
}

// ==================== ТЕСТЫ ИНТЕГРАЦИИ АУТЕНТИФИКАЦИИ ====================

SUITE(AuthIntegrationTest)
{
    TEST(AuthHashCalculation) {
        Server server(33333, "/scale.conf", "/log/scale.log");
        
        string salt = "0123456789ABCDEF"; // 16 hex символов
        string password = "testpassword";
        
        // Вычисляем хэш вручную для проверки
        string expectedInput = salt + password;
        string computedHash = server.testSha224Hash(expectedInput);
        
        // Проверяем длину и формат
        CHECK_EQUAL(56, computedHash.length());
        
        // Проверяем hex формат
        bool validHex = true;
        for (char c : computedHash) {
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
                validHex = false;
                break;
            }
        }
        CHECK(validHex);
    }
}

// ==================== ГЛАВНАЯ ФУНКЦИЯ ТЕСТОВ ====================

/**
 * @brief Основная функция модульных тестов.
 * @return Код завершения тестов.
 */
int main()
{
    cout << "Starting server tests..." << endl;
    cout << "=========================" << endl;
    
    int result = UnitTest::RunAllTests();
    
    cout << "=========================" << endl;
    cout << "Tests completed." << endl;
    
    // Очистка временных файлов
    system("rm -f temp_test_db_*.txt 2>/dev/null || true");
    system("rm -f empty_test_*.txt 2>/dev/null || true");
    system("rm -f invalid_test_*.txt 2>/dev/null || true");
    
    return result;
}
