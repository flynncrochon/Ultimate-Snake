#include "web_platform.hpp"
#include <emscripten.h>

namespace {
EM_JS(int, hidden, (), {return document.hidden ? 1 : 0;});
EM_JS(int, viewportSize, (), {
    const width=Math.min(16384,Math.max(800,window.innerWidth|0));
    const height=Math.min(16384,Math.max(500,window.innerHeight|0));
    const packed=(width<<16)|height,canvas=Module['canvas'];
    if(Module['canvasViewport']===packed&&canvas.width===width&&canvas.height===height)return 0;
    Module['canvasViewport']=packed;return packed;
});
EM_JS(void, queueSave, (), {if(Module['queueSave'])Module['queueSave']();});
EM_JS(void, fatalError, (const char* message), {if(Module['fatalError'])Module['fatalError'](UTF8ToString(message));});
}
namespace snake::web {
bool pageHidden(){return hidden()!=0;}
int pendingViewportSize(){return viewportSize();}
void flushSaves(){queueSave();}
void showError(const char* message){fatalError(message);}
}
