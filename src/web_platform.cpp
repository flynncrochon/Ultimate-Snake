#include "web_platform.hpp"
#include <emscripten.h>

namespace {
EM_JS(int, hidden, (), {return document.hidden ? 1 : 0;});
EM_JS(void, queueSave, (), {if(Module['queueSave'])Module['queueSave']();});
EM_JS(void, fatalError, (const char* message), {if(Module['fatalError'])Module['fatalError'](UTF8ToString(message));});
}
namespace snake::web {
bool pageHidden(){return hidden()!=0;}
void flushSaves(){queueSave();}
void showError(const char* message){fatalError(message);}
}
