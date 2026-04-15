#pragma once
// Skillbar management is part of SkillMgr — skillbar access is tightly coupled
// with skill execution.
//
// Key functions in SkillMgr.h / SkillMgr.cpp:
//   GetSkillbarArrayBase()  — Traverses game structures to the skill array
//   UseSkill()              — Player skill execution via shellcode ring buffer
//   UseHeroSkill()          — Hero skill execution
//   LoadSkillTemplate()     — Loads a skill template code
//
// Shellcode uses a 16-slot ring buffer (32 bytes each) to avoid repeated
// VirtualAlloc on the game thread.
#include <gwa3/managers/SkillMgr.h>
