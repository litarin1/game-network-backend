#pragma once
#include <assert.h>

#include <functional>

// 128 chars + separator+terminator
// might be overridden
#define MAX_MESSAGE_LENGTH 130
#define MAX_REQUEST_LENGTH MAX_MESSAGE_LENGTH

/** @brief Functor that converts enum of command to value and pass it to Sender
 * @tparam CommandsEnum enum of network command codes
 * @tparam buf_size size of send buffer
 * @tparam Separator operator("first", "second") with Separator '|' and
 * Terminator ';' produces "first|second;")
 * @tparam Terminator operator("...data...") with Terminator ';' and Separator
 * '|' produces "...data...|;")
 * @tparam Escaper escapes Separator, Terminator and Escaper symbols from the
 * message (e.g. "|';DROP TABLE USERS" -> "\|'\;DROP TABLE USERS|;")
 * @code
 * class Server {
 *     enum Commands {VERSION, ORDER_PIZZA};
 *     void _send_message(const char* message){
 *         send(fd, message, 0);
 *     }
 *
 * public:
 *     CommandSender<Commands> send_message{_send_message};
 * }
 * @endcode
 */
template <typename CommandsEnum, size_t buf_size = MAX_MESSAGE_LENGTH,
          const char Separator = '|', const char Terminator = ';',
          const char Escaper = '\\'>
class CommandSender {
    char buff[buf_size];
    size_t i = 0;  // buff[0] is the command from CommandsEnum
    /// @brief sender function
    std::function<void(char* message, size_t n)> sender;

    /// @brief writes value to buff
    /// @tparam T
    /// @param value
    /// @internal @details base function. clips data
    template <typename T>
    constexpr void write_buffer(const T value) {
        for (const char* p = reinterpret_cast<const char*>(&value);
             p < reinterpret_cast<const char*>(&value) + sizeof(value) / 4 &&
             i < sizeof(buff)-2;
             i++, p++) {
            buff[i] = *p;
        }
        assert(!(i > sizeof(buff)-2) && "should not happen");
        // buff[i] = '\r';  // maximum possible i here is sizeof(buff)-2 (128)
        // buff[++i] = '\n';
        buff[i++] = Separator;
    }
    constexpr void write_buffer(char* string) {
        // strncpy(&buff[i+1], null_terminated_string, sizeof(buff) - i);
        // size_t n = strnlen(string, sizeof(buff) - i - 1 - 2);  // -1-        buff[i] = Separator;
        // memcpy(&buff[i + 1], string, n);
        size_t n = strncpy_with_escaping(&buff[i+1], string, sizeof(buff) - i - 2);
        // i = sizeof(buff) - n;
        i += n;
        assert(!(i > sizeof(buff)-2) && "should not happen");
        buff[++i] = Separator;
        i++;
    }

    /// @param len dest length
    /// @return length
    static constexpr size_t strncpy_with_escaping(char* dest, const char* src, const size_t len){
        const char _forbidden_symbols[] = {Separator, Terminator, Escaper};
        size_t i = 0, offset = 0;
        while (i+offset < len && src[i] != '\0'){
            for (const char c : _forbidden_symbols){
                if (src[i] == c){
                    dest[i+offset] = Escaper;
                    offset++;
                    break;
                }
            }
            fflush(stdout);
            dest[i+offset] = src[i];
            i++;
        }
        return i+offset;
    }
    /// @internal
    /// @brief pass value
    /// @details recursive function
    /// @tparam T single value (used for recursion)
    /// @tparam Left left values
    template <typename T, typename... Left>
    constexpr void write_buffer(const T value, const Left... left) {
        write_buffer(value);
        write_buffer(left...);  // recursive call
    }

public:
    // send command and (optional) values
    // should not be more than MAX_MESSAGE_LENGTH bytes
    template <typename... T>
    void operator()(CommandsEnum command, T... values) {
        i = 0;
        bzero(buff, sizeof(buff));
        write_buffer(command, values...);
        buff[i] = Terminator;
        i++;
        sender(buff, i);
    }

    /// @brief constructor
    /// @param sender function that will be called when operator() invoked
    CommandSender(std::function<void(char* message, size_t n)> sender)
        : sender(sender) {}
};

