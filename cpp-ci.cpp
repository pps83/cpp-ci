#include "VsSolution.h"
#include "VsToolsets.h"
#include "util.h"

int main()
{
    HrTimer t0;
    VsSolution sln;
    sln.loadSlnFile("../backtest-engine/backtest-engine.sln");
    LOG("VsSolution load time:%s", t0.elapsedTimeString().c_str());

    HrTimer t1;
    VsToolsets vsToolsets;
    vsToolsets.testVsToolsets();
    LOG("VsToolsets test time:%s", t1.elapsedTimeString().c_str());
}
