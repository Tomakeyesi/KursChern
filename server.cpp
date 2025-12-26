/**
 * @file server.cpp
 * @author Чернышев Ринат Рустямович
 * @date 26.12.2025
 * @brief Реализация методов класса Server для обработки клиентских подключений.
 * @details Содержит реализацию сетевого сервера с аутентификацией пользователей
 *          по протоколу SHA-224, обработкой векторных данных и логированием событий.
 */

#include "server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <cstdlib>

/**
 * @brief Конструктор класса Server.
 * @param port Порт для прослушивания подключений.
 * @param userDbPath Путь к файлу базы данных пользователей.
 * @param logPath Путь к файлу журнала сервера.
 */
Server::Server(int port, const std::string& userDbPath, const std::string& logPath)
    : port(port), userDbPath(userDbPath), logPath(logPath) {}

/**
 * @brief Загружает базу данных пользователей из файла.
 * @details Читает файл построчно, парсит строки формата "логин:пароль"
 *          и сохраняет данные во внутреннем контейнере users.
 */
void Server::loadUserDatabase() {
    std::ifstream file(userDbPath);
    if (!file.is_open()) {
        logError("Cannot open user database file: " + userDbPath, true);
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find(':');
        if (pos != std::string::npos && pos > 0 && pos < line.length() - 1) {
            std::string login = line.substr(0, pos);
            std::string password = line.substr(pos + 1);
            if (!login.empty() && !password.empty()) {
                users[login] = password;
            }
        }
    }
    file.close();
}

/**
 * @brief Записывает сообщение об ошибке в файл журнала.
 * @param message Текст сообщения об ошибке.
 * @param isCritical Флаг критичности ошибки.
 */
void Server::logError(const std::string& message, bool isCritical) {
    std::ofstream logFile(logPath, std::ios::app);
    if (!logFile.is_open()) {
        return;
    }
    
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    
    logFile << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S") << " | "
            << (isCritical ? "CRITICAL" : "NON-CRITICAL") << " | "
            << message << std::endl;
    
    logFile.close();
}

/**
 * @brief Вычисляет SHA-224 хэш для входной строки.
 * @param input Входная строка для хэширования.
 * @return SHA-224 хэш строки в шестнадцатеричном формате (верхний регистр, 56 символов).
 */
std::string Server::sha224Hash(const std::string& input) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) {
        return "";
    }
    
    if (EVP_DigestInit_ex(context, EVP_sha224(), nullptr) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }
    
    if (EVP_DigestUpdate(context, input.c_str(), input.length()) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }
    
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLength;
    if (EVP_DigestFinal_ex(context, digest, &digestLength) != 1) {
        EVP_MD_CTX_free(context);
        return "";
    }
    
    EVP_MD_CTX_free(context);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLength; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(digest[i]);
    }
    
    std::string result = ss.str();
    for (auto& c : result) {
        c = std::toupper(c);
    }
    return result;
}

/**
 * @brief Генерирует случайную соль для аутентификации.
 * @return Соль в виде строки из 16 шестнадцатеричных символов (64 бита).
 */
std::string Server::generateSalt() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    uint64_t saltValue = dis(gen);
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << saltValue;
    
    std::string salt = ss.str();
    for (auto& c : salt) {
        c = std::toupper(c);
    }
    return salt;
}

/**
 * @brief Аутентифицирует клиента по протоколу SHA-224 с солью.
 * @param clientSocket Дескриптор сокета клиента.
 * @return true если аутентификация успешна, false в противном случае.
 */
bool Server::authenticate(int clientSocket) {
    char buffer[256];
    
    // Шаг 2: Клиент передает свой идентификатор LOGIN
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        logError("No data received from client for login", false);
        return false;
    }
    buffer[bytesRead] = '\0';
    std::string login(buffer);
    
    // Шаг 3: Проверяем идентификацию
    auto userIt = users.find(login);
    if (userIt == users.end()) {
        // 3б. Ошибка идентификации - отправляем ERR и разрываем соединение
        send(clientSocket, "ERR", 3, 0);
        logError("Identification failed for login: " + login, false);
        return false;
    }
    
    // 3а. Успешная идентификация - отправляем соль (16 hex символов)
    std::string salt = generateSalt();
    if (send(clientSocket, salt.c_str(), 16, 0) != 16) {
        logError("Failed to send salt to client", false);
        return false;
    }
    
    // Шаг 4: Клиент передает HASH(SALT || PASSWORD)
    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        logError("No hash received from client", false);
        return false;
    }
    buffer[bytesRead] = '\0';
    std::string receivedHash(buffer);
    
    // Шаг 5: Проверяем аутентификацию
    std::string password = userIt->second;
    std::string computedHash = sha224Hash(salt + password);
    
    // Приводим к верхнему регистру для сравнения
    for (auto& c : receivedHash) c = std::toupper(c);
    
    if (computedHash == receivedHash) {
        // 5а. Успешная аутентификация
        send(clientSocket, "OK", 2, 0);
        logError("Authentication successful for login: " + login, false);
        return true;
    } else {
        // 5б. Ошибка аутентификации - отправляем ERR и разрываем соединение
        send(clientSocket, "ERR", 3, 0);
        logError("Authentication failed for login: " + login, false);
        return false;
    }
}

/**
 * @brief Вычисляет сумму квадратов элементов вектора с проверкой переполнения.
 * @param vector Вектор 16-битных целых чисел.
 * @return Сумма квадратов элементов вектора.
 * @details При переполнении вверх возвращает 32767 (2^15),
 *          при переполнении вниз возвращает -32768 (-2^15).
 */
int16_t Server::calculateSumOfSquares(const std::vector<int16_t>& vector) {
    if (vector.empty()) {
        return 0;
    }
    
    // Используем int64_t для предотвращения переполнения
    int64_t sum = 0;
    
    for (int16_t value : vector) {
        int64_t square = static_cast<int64_t>(value) * static_cast<int64_t>(value);
        sum += square;
        
        // Проверка переполнения
        if (sum > 32767) {
            return 32767;
        }
        if (sum < -32768) {
            return -32768;
        }
    }
    
    return static_cast<int16_t>(sum);
}

/**
 * @brief Вспомогательная функция для точного чтения из сокета.
 * @param socket Дескриптор сокета.
 * @param buffer Буфер для данных.
 * @param size Количество байт для чтения.
 * @return true если все байты прочитаны, false при ошибке.
 */
bool Server::readExact(int socket, void* buffer, size_t size) {
    uint8_t* buf = reinterpret_cast<uint8_t*>(buffer);
    size_t totalRead = 0;
    
    while (totalRead < size) {
        ssize_t bytesRead = recv(socket, buf + totalRead, size - totalRead, 0);
        if (bytesRead <= 0) {
            return false;
        }
        totalRead += bytesRead;
    }
    return true;
}

/**
 * @brief Обрабатывает передачу векторов от аутентифицированного клиента.
 * @param clientSocket Дескриптор сокета клиента.
 * @details Ожидает данные в двоичном формате согласно ТЗ.
 */
void Server::processVectors(int clientSocket) {
    std::cout << "DEBUG: Starting vector processing" << std::endl;
    
    // Шаг 6: Читаем количество векторов
    uint32_t numVectors;
    ssize_t bytesRead = recv(clientSocket, &numVectors, sizeof(numVectors), 0);
    
    std::cout << "DEBUG: Read " << bytesRead << " bytes for numVectors" << std::endl;
    
    if (bytesRead != sizeof(numVectors)) {
        std::cout << "DEBUG: Failed to read numVectors. Expected " << sizeof(numVectors) 
                  << ", got " << bytesRead << std::endl;
        logError("Failed to read number of vectors", false);
        return;
    }
    
    // КЛИЕНТ ОТПРАВЛЯЕТ В LITTLE-ENDIAN - оставляем как есть
    std::cout << "DEBUG: Number of vectors: " << numVectors << std::endl;
    
    // Обрабатываем каждый вектор и сразу отправляем результат
    for (uint32_t i = 0; i < numVectors; ++i) {
        std::cout << "DEBUG: Processing vector " << i + 1 << std::endl;
        
        // Шаг 7: Читаем размер вектора
        uint32_t vectorSize;
        bytesRead = recv(clientSocket, &vectorSize, sizeof(vectorSize), 0);
        
        std::cout << "DEBUG: Read " << bytesRead << " bytes for vectorSize" << std::endl;
        
        if (bytesRead != sizeof(vectorSize)) {
            std::cout << "DEBUG: Failed to read vectorSize" << std::endl;
            logError("Failed to read vector size", false);
            return;
        }
        
        // КЛИЕНТ ОТПРАВЛЯЕТ В LITTLE-ENDIAN - оставляем как есть
        std::cout << "DEBUG: Vector " << i + 1 << " size: " << vectorSize << std::endl;
        
        // Шаг 8: Читаем данные вектора
        std::vector<int16_t> vector(vectorSize);
        size_t totalBytesToRead = vectorSize * sizeof(int16_t);
        
        // Читаем частями если нужно
        uint8_t* buffer = reinterpret_cast<uint8_t*>(vector.data());
        size_t totalRead = 0;
        
        while (totalRead < totalBytesToRead) {
            bytesRead = recv(clientSocket, buffer + totalRead, totalBytesToRead - totalRead, 0);
            if (bytesRead <= 0) {
                std::cout << "DEBUG: Failed to read vector data" << std::endl;
                logError("Failed to read vector data", false);
                return;
            }
            totalRead += bytesRead;
        }
        
        std::cout << "DEBUG: Read " << totalRead << " bytes of vector data" << std::endl;
        
        // КЛИЕНТ ОТПРАВЛЯЕТ В LITTLE-ENDIAN - оставляем как есть
        if (!vector.empty()) {
            std::cout << "DEBUG: First value: " << vector[0] << std::endl;
        }
        
        // Вычисляем сумму квадратов
        int16_t result = calculateSumOfSquares(vector);
        std::cout << "DEBUG: Sum of squares for vector " << i + 1 << ": " << result << std::endl;
        
        // Шаг 9: Отправляем результат СРАЗУ в LITTLE-ENDIAN
        ssize_t bytesSent = send(clientSocket, &result, sizeof(result), 0);
        
        std::cout << "DEBUG: Sent " << bytesSent << " bytes for result: " << result << std::endl;
        
        if (bytesSent != sizeof(result)) {
            std::cout << "DEBUG: Failed to send result" << std::endl;
            logError("Failed to send result for vector " + std::to_string(i + 1), false);
            return;
        }
        
        std::cout << "DEBUG: Result for vector " << i + 1 << " sent successfully" << std::endl;
    }
    
    std::cout << "DEBUG: All " << numVectors << " vectors processed successfully" << std::endl;
}

/**
 * @brief Обрабатывает подключение одного клиента.
 * @param clientSocket Дескриптор сокета клиента.
 */
void Server::handleClient(int clientSocket) {
    std::cout << "New client connection" << std::endl;
    logError("New client connection established", false);
    
    if (!authenticate(clientSocket)) {
        logError("Authentication failed, closing connection", false);
        close(clientSocket);
        return;
    }
    
    std::cout << "Client authenticated successfully" << std::endl;
    logError("Client authenticated successfully", false);
    
    processVectors(clientSocket);
    close(clientSocket);
    logError("Client connection closed", false);
}

/**
 * @brief Запускает основной цикл работы сервера.
 * @return true если сервер успешно запущен, false при критической ошибке.
 */
bool Server::start() {
    // Проверяем возможность записи в лог-файл
    std::ofstream testLog(logPath, std::ios::app);
    if (!testLog.is_open()) {
        std::cerr << "ERROR: Cannot open log file: " << logPath << std::endl;
        // Пробуем альтернативный путь
        logPath = "./server_fallback.log";
        std::cerr << "Trying fallback: " << logPath << std::endl;
        testLog.open(logPath, std::ios::app);
        if (!testLog.is_open()) {
            std::cerr << "ERROR: Cannot open fallback log file" << std::endl;
            return false;
        }
    }
    testLog.close();
    
    logError("=== Server starting ===", false);
    
    // Загружаем базу пользователей
    loadUserDatabase();
    logError("User database loaded, users: " + std::to_string(users.size()), false);
    
    // Создаем директорию для логов если её нет
    system("mkdir -p log 2>/dev/null");
    
    // Инициализация OpenSSL
    OpenSSL_add_all_digests();
    
    // Создаем сокет
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        logError("Cannot create socket", true);
        return false;
    }
    
    // Устанавливаем опции сокета
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logError("Cannot set socket options", true);
        close(serverSocket);
        return false;
    }
    
    // Настраиваем адрес сервера
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    // Привязываем сокет к адресу
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        logError("Cannot bind socket to port " + std::to_string(port), true);
        close(serverSocket);
        return false;
    }
    
    // Начинаем прослушивание
    if (listen(serverSocket, 10) < 0) {
        logError("Cannot listen on socket", true);
        close(serverSocket);
        return false;
    }
    
    std::cout << "Server started on port " << port << std::endl;
    std::cout << "User database: " << userDbPath << std::endl;
    std::cout << "Log file: " << logPath << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    logError("Server started successfully on port " + std::to_string(port), false);
    
    // Основной цикл обработки подключений
    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            logError("Cannot accept client connection", false);
            continue;
        }
        
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::cout << "Client connected from " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;
        
        // Обрабатываем клиента в текущем потоке (однопоточный режим по ТЗ)
        handleClient(clientSocket);
    }
    
    close(serverSocket);
    return true;
}
