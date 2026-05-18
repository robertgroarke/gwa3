// Shared step runner for the advanced workflow integration suite.

static bool RunAdvancedWorkflowStep(const char* abortLabel, bool (*step)()) {
    step();
    return !AbortWorkflowIfRuntimeFailed(abortLabel);
}
