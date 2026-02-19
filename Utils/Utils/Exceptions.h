#pragma once

#include <stdexcept>
#include <string>

namespace pcsc {

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& what)
        : std::runtime_error(what) {}
};

class CipherError : public Error {
public:
    explicit CipherError(const std::string& what)
        : Error(what) {}
};

class ReaderError : public Error {
public:
    explicit ReaderError(const std::string& what)
        : Error(what) {}
};

} // namespace pcsc
