#include "SmartCardApp.h"

#define BUILD_MAIN_APP

#ifdef BUILD_MAIN_APP
int main() {
    SmartCardApp app;
    return app.run();
}
#endif