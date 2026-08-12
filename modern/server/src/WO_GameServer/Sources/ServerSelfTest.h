// [PORT] New file. See ServerSelfTest.cpp for what this is and why it exists.
//
// Milestone C's server criterion, split into stages that run one at a time and report
// which subsystem failed, without needing a MasterServer, a SupervisorServer or a
// backend to be up.

#pragma once

struct SelfTestOptions
{
    const char* stage;      // "config" | "world" | "level" | "all"
    const char* levelDir;   // --level=<dir>, required by the "level" stage
    int         ticks;      // --ticks=<n>, default 100
};

// Returns true if --selftest=<stage> was present, in which case main() should run the
// stages and exit rather than continuing into the normal boot. Fills `out` regardless.
bool SelfTest_ParseArgs(int argc, char* argv[], SelfTestOptions* out);

// Runs the requested stage(s). Returns a process exit code: 0 pass, 1 a stage failed,
// 2 the stage name was not recognised.
int SelfTest_Run(const SelfTestOptions& opt);
