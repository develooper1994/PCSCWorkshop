#include "DesfireSecureMessaging.h"
#include "CngBlockCipher.h"
#include <stdexcept>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
// CMAC Calculation + IV Update
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireSecureMessaging::calculateCMAC(DesfireSession& session,
                                             const BYTEV& data) {
    if (session.sessionKey.empty())
        throw std::runtime_error("SecureMessaging: no session key");

    // CMAC with current session key
    BYTEV fullMAC = CngBlockCipher::cmacAES(session.sessionKey, data);

    // IV = full CMAC (for next operation's IV chaining)
    session.iv = fullMAC;

    return fullMAC;
}

BYTEV DesfireSecureMessaging::truncateCMAC(const BYTEV& fullCMAC) {
    // Odd-indexed bytes: [1],[3],[5],[7],[9],[11],[13],[15]
    BYTEV trunc;
    trunc.reserve(8);
    for (size_t i = 1; i < fullCMAC.size() && trunc.size() < 8; i += 2) {
        trunc.push_back(fullCMAC[i]);
    }
    return trunc;
}

// ════════════════════════════════════════════════════════════════════════════════
// Command Wrapping
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireSecureMessaging::wrapCommand(DesfireSession& session,
                                           const BYTEV& cmdHeader,
                                           const BYTEV& cmdData,
                                           DesfireCommMode commMode) {
    // CMAC input: INS byte + cmdData
    // (DESFire EV1: CMAC is calculated over the native command byte + data)
    BYTEV cmacInput;
    if (!cmdHeader.empty() && cmdHeader.size() >= 2) {
        cmacInput.push_back(cmdHeader[1]);  // INS byte
    }
    cmacInput.insert(cmacInput.end(), cmdData.begin(), cmdData.end());

    BYTEV fullMAC = calculateCMAC(session, cmacInput);

    if (commMode == DesfireCommMode::MAC) {
        // Append 8-byte truncated CMAC to command data
        BYTEV tMAC = truncateCMAC(fullMAC);
        BYTEV wrappedData = cmdData;
        wrappedData.insert(wrappedData.end(), tMAC.begin(), tMAC.end());

        // Re-wrap the APDU with extended data
        if (cmdHeader.size() >= 2) {
            return DesfireCommands::wrapCommand(cmdHeader[1], wrappedData);
        }
    }

    // Plain mode or no header: just update IV, return original command
    if (cmdHeader.size() >= 2) {
        return DesfireCommands::wrapCommand(cmdHeader[1], cmdData);
    }
    return cmdHeader;
}

// ════════════════════════════════════════════════════════════════════════════════
// Response Unwrapping
// ════════════════════════════════════════════════════════════════════════════════

BYTEV DesfireSecureMessaging::unwrapResponse(DesfireSession& session,
                                              const BYTEV& responseData,
                                              BYTE statusCode,
                                              DesfireCommMode commMode) {
    if (commMode == DesfireCommMode::MAC && responseData.size() >= 8) {
        // Last 8 bytes = truncated CMAC from card
        size_t dataLen = responseData.size() - 8;
        BYTEV pureData(responseData.begin(), responseData.begin() + dataLen);
        BYTEV receivedMAC(responseData.begin() + dataLen, responseData.end());

        // CMAC input: data + status code
        BYTEV cmacInput = pureData;
        cmacInput.push_back(statusCode);

        BYTEV fullMAC = calculateCMAC(session, cmacInput);
        BYTEV expectedMAC = truncateCMAC(fullMAC);

        if (receivedMAC != expectedMAC) {
            throw std::runtime_error("SecureMessaging: CMAC verification failed");
        }

        return pureData;
    }

    // Plain mode: calculate CMAC for IV update but don't verify
    if (!responseData.empty()) {
        BYTEV cmacInput = responseData;
        cmacInput.push_back(statusCode);
        calculateCMAC(session, cmacInput);
    }

    return responseData;
}
