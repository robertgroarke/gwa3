struct TekksDebugTailStep {
    float x;
    float y;
    float threshold;
    DWORD timeoutMs;
    const char* label;
};

struct TekksDebugPathStep {
    float x;
    float y;
    float threshold;
    DWORD timeoutMs;
    const char* label;
    bool required;
};
