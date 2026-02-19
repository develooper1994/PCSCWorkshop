#include "PCSC.h"

#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
    PCSC app;
    return app.run();
}
#endif