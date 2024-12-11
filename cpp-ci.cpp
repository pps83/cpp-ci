#include "VsSolution.h"
#include "util.h"

int main()
{
    HrTimer t;
    VsSolution sln;
    sln.loadSlnFile("../backtest-engine/backtest-engine.sln");
    LOG("VsSolution load time:%s", t.elapsedTimeString().c_str());
}
