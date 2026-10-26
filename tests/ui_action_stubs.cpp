// Link stubs for UI builds that pull in src/ui/actions.cpp but don't exercise
// the map or power paths (test_actions.cpp defines its own counting versions).
void openNavigation() {}
const char* powerOff(bool) { return nullptr; }
