#ifndef PCSC_UTILS_GENERIC_EXCEPTIONS_H
#define PCSC_UTILS_GENERIC_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace pcsc {

	class Error : public std::runtime_error {
	public:
		explicit Error(const std::string& what): std::runtime_error(what) {}
		explicit Error(const char* what) : std::runtime_error(what) {}
		Error(const std::runtime_error& other) : std::runtime_error(other) {}
		Error(const Error& other) : std::runtime_error(other) {}
	};

	class CipherError : public Error {
	public:
		explicit CipherError(const std::string& what): Error(what) {}
		explicit CipherError(const char* what) : Error(what) {}
		CipherError(const Error& other) : Error(other) {}
		CipherError(const CipherError& other) : Error(other) {}
	};

	class ReaderError : public Error {
	public:
		explicit ReaderError(const std::string& what) : Error(what) {}
		explicit ReaderError(const char* what) : Error(what) {}
		ReaderError(const Error& other) : Error(other) {}
		ReaderError(const ReaderError& other) : Error(other) {}
	};

	class AuthFailedError : public ReaderError {
	public:
		explicit AuthFailedError(const std::string& what) : ReaderError(what) {}
		explicit AuthFailedError(const char* what) : ReaderError(what) {}
		AuthFailedError(const ReaderError& other) : ReaderError(other) {}
		AuthFailedError(const AuthFailedError& other) : ReaderError(other) {}
	};

	class LoadKeyFailedError : public ReaderError {
	public:
		explicit LoadKeyFailedError(const std::string& what) : ReaderError(what) {}
		explicit LoadKeyFailedError(const char* what) : ReaderError(what) {}
		LoadKeyFailedError(const ReaderError& other) : ReaderError(other) {}
		LoadKeyFailedError(const AuthFailedError& other) : ReaderError(other) {}
	};

	class ReaderReadError : public ReaderError {
	public:
		explicit ReaderReadError(const std::string& what) : ReaderError(what) {}
		explicit ReaderReadError(const char* what) : ReaderError(what) {}
		ReaderReadError(const ReaderError& other) : ReaderError(other) {}
		ReaderReadError(const ReaderReadError& other) : ReaderError(other) {}
	};

	class ReaderWriteError : public ReaderError {
	public:
		explicit ReaderWriteError(const std::string& what) : ReaderError(what) {}
		explicit ReaderWriteError(const char* what) : ReaderError(what) {}
		ReaderWriteError(const ReaderError& other) : ReaderError(other) {}
		ReaderWriteError(const ReaderWriteError& other) : ReaderError(other) {}
	};

} // namespace pcsc
#endif // PCSC_UTILS_GENERIC_EXCEPTIONS_H