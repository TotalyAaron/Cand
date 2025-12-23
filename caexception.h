#pragma once
#include <exception>
#include <string>
class CAndException : public std::exception
{
private:
    std::string type;
    std::string message;
    char* concatenate_strings(char *strings[], int count) const
    {
        size_t total_length = 0;
        for (int i = 0; i < count; i++)
        {
            total_length += strlen(strings[i]);
        }
        // Add 1 for the null-terminator character
        total_length += 1;
        char *destination = (char *)malloc(total_length * sizeof(char));
        if (destination == NULL)
        {
            // Handle memory allocation failure
            return NULL;
        }
        // Copy all data into the buffer using a single or sequential operations
        strcpy(destination, strings[0]);
        // Append subsequent strings efficiently
        for (int i = 1; i < count; i++)
        {
            strcat(destination, strings[i]);
        }
        return destination;
    }

public:
    CAndException(const std::string msg) : message(msg), type("Base") {}
    CAndException(const std::string msg, const std::string type) : message(msg), type(type) {}
    const char *what() const noexcept override
    {
        char* buffer = strdup(type.c_str());
        char* buffer1 = strdup(message.c_str());
        char* strings[4] = {"CAndException.", buffer, ": ", buffer1};
        char* msg = concatenate_strings(strings, 4);
        free(buffer);
        free(buffer1);
        return msg;
    }
    const char *nwhat()
    {
        return message.c_str();
    }
};