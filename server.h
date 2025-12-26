/**
 * @file server.h
 * @author Чернышев Ринат Рустямович
 * @date 26.12.2025
 * @brief Заголовочный файл класса Server.
 * @details Объявление класса сервера для обработки сетевых подключений,
 *          аутентификации клиентов по протоколу SHA-224 и вычисления суммы квадратов векторов.
 */

#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

/**
 * @brief Класс сервера для обработки клиентских подключений.
 * @details Обеспечивает сетевую коммуникацию, аутентификацию пользователей
 *          по протоколу SHA-224 с генерацией соли на сервере, прием и обработку
 *          векторных данных в двоичном формате, а также логирование.
 */
class Server {
public:
    /**
     * @brief Конструктор сервера.
     * @param port Порт для прослушивания подключений.
     * @param userDbPath Путь к файлу базы данных пользователей.
     * @param logPath Путь к файлу журнала сервера.
     */
    Server(int port, const std::string& userDbPath, const std::string& logPath);
    
    /**
     * @brief Запускает сервер и начинает прослушивание порта.
     * @return true если сервер успешно запущен, false при ошибке.
     */
    bool start();

private:
    int port;                                       ///< Порт сервера
    std::string userDbPath;                         ///< Путь к базе пользователей
    std::string logPath;                            ///< Путь к файлу журнала
    std::unordered_map<std::string, std::string> users; ///< Кэш пользователей (логин:пароль)
    
    /**
     * @brief Записывает сообщение об ошибке в журнал.
     * @param message Текст сообщения об ошибке.
     * @param isCritical Флаг критичности ошибки (true для критических).
     */
    void logError(const std::string& message, bool isCritical);
    
    /**
     * @brief Загружает базу данных пользователей из файла.
     * @details Формат файла: каждая строка содержит "логин:пароль".
     */
    void loadUserDatabase();
    
    /**
     * @brief Вычисляет SHA-224 хэш строки.
     * @param input Входная строка для хэширования.
     * @return SHA-224 хэш в шестнадцатеричном формате (56 символов, верхний регистр).
     */
    std::string sha224Hash(const std::string& input);
    
    /**
     * @brief Генерирует случайную соль для аутентификации.
     * @return Соль в виде строки из 16 шестнадцатеричных символов (64 бита).
     */
    std::string generateSalt();
    
    /**
     * @brief Обрабатывает подключение клиента.
     * @param clientSocket Дескриптор сокета клиента для обмена данными.
     */
    void handleClient(int clientSocket);
    
    /**
     * @brief Аутентифицирует клиента по протоколу SHA-224.
     * @param clientSocket Дескриптор сокета клиента.
     * @return true если аутентификация успешна.
     * @details Протокол:
     *          1. Клиент отправляет логин
     *          2. Сервер генерирует и отправляет соль (16 hex символов)
     *          3. Клиент отправляет HASH(SALT || PASSWORD)
     *          4. Сервер проверяет хэш
     */
    bool authenticate(int clientSocket);
    
    /**
     * @brief Обрабатывает передачу векторов от аутентифицированного клиента.
     * @param clientSocket Дескриптор сокета клиента для обмена данными.
     * @details Ожидает данные в двоичном формате:
     *          - количество векторов (uint32_t)
     *          - для каждого вектора:
     *            * размер (uint32_t)
     *            * данные (int16_t[])
     *          Отправляет результаты в двоичном формате:
     *          - количество результатов (uint32_t)
     *          - результаты (int16_t[])
     */
    void processVectors(int clientSocket);
    
    /**
     * @brief Вычисляет сумму квадратов элементов вектора.
     * @param vector Вектор 16-битных целых чисел для обработки.
     * @return Сумма квадратов элементов.
     * @details При переполнении вверх возвращает 32767 (2^15),
     *          при переполнении вниз возвращает -32768 (-2^15).
     */
    int16_t calculateSumOfSquares(const std::vector<int16_t>& vector);
    
    /**
     * @brief Читает точное количество байт из сокета.
     * @param socket Дескриптор сокета для чтения.
     * @param buffer Буфер для принятых данных.
     * @param size Количество байт для чтения.
     * @return true если все байты прочитаны, false при ошибке.
     */
    bool readExact(int socket, void* buffer, size_t size);
    
    #ifdef SERVER_TESTING
    public:
        /**
         * @brief Возвращает количество загруженных пользователей.
         * @return Количество пользователей в базе.
         */
        size_t getUsersCount() const { return users.size(); }
        
        /**
         * @brief Тестовый метод для вычисления суммы квадратов вектора.
         * @param vector Вектор для обработки.
         * @return Сумма квадратов элементов вектора.
         */
        int16_t testCalculateSumOfSquares(const std::vector<int16_t>& vector) {
            return calculateSumOfSquares(vector);
        }
        
        /**
         * @brief Тестовый метод для вычисления SHA-224 хэша.
         * @param input Строка для хэширования.
         * @return SHA-224 хэш строки.
         */
        std::string testSha224Hash(const std::string& input) {
            return sha224Hash(input);
        }
        
        /**
         * @brief Тестовый метод для генерации соли.
         * @return Сгенерированная соль.
         */
        std::string testGenerateSalt() {
            return generateSalt();
        }
        
        /**
         * @brief Тестовый метод для загрузки базы данных пользователей.
         */
        void testLoadUserDatabase() {
            loadUserDatabase();
        }
    #endif
};

#endif // SERVER_H
