#include "ACR1281UReader.h"

ACR1281UReader::ACR1281UReader(PCSC& pcsc) : Reader(pcsc) {}
ACR1281UReader::ACR1281UReader(PCSC& pcsc, BYTE blockSize) : Reader(pcsc, blockSize) {}
ACR1281UReader::ACR1281UReader(ACR1281UReader&&) noexcept = default;
ACR1281UReader::~ACR1281UReader() = default;
ACR1281UReader& ACR1281UReader::operator=(ACR1281UReader&&) noexcept = default;
