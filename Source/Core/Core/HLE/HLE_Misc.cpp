// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/Host.h"
#include "Core/HLE/HLE_Misc.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCCache.h"

namespace HLE_Misc
{

static std::string args;

// HLE_HOOK_START trace hook on HSD_Randi (GALE01 0x80380580). Logs the RNG seed BEFORE the draw, the
// requested range (r3), and the caller LR. HOOK_START runs this hook and THEN the original HSD_Randi,
// so the game RNG stream is unaffected. Gated by the MSL_RNG_TRACE env var so it is fully inert for
// normal runs and ordinary engine-dump probes. HSD_Rand/HSD_Randf draws are not hooked but are
// recoverable as seed gaps between consecutive logged Randi calls (the LCG advances one step/draw).
// Used only by the Needle post-hit RNG forensics; trace lines go to stderr.
void HLE_HSD_RandiTrace()
{
	static const bool s_enabled = std::getenv("MSL_RNG_TRACE") != nullptr;
	if (!s_enabled)
		return;
	const u32 seed_before = PowerPC::HostRead_U32(0x804D5F90);
	const u32 max_val = GPR(3);
	const u32 caller = LR;
	std::fprintf(stderr, "RNGTRACE seed=%08x max=%u lr=%08x\n", seed_before, max_val, caller);
}

// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction()
{
	NPC = LR;
}

// If you want a function to panic, you can rename it PanicAlert :p
// Don't know if this is worth keeping.
void HLEPanicAlert()
{
	PanicAlert("HLE: PanicAlert %08x", LR);
	NPC = LR;
}

void HBReload()
{
	// There isn't much we can do. Just stop cleanly.
	CPU::Break();
	Host_Message(WM_USER_STOP);
}

void HLEGeckoCodehandler()
{
	// Work around the codehandler not properly invalidating the icache, but
	// only the first few frames.
	// (Project M uses a conditional to only apply patches after something has
	// been read into memory, or such, so we do the first 5 frames.  More
	// robust alternative would be to actually detect memory writes, but that
	// would be even uglier.)
	u32 magic = 0xd01f1bad;
	u32 existing = PowerPC::HostRead_U32(0x80001800);
	if (existing - magic == 5)
	{
		return;
	}
	else if (existing - magic > 5)
	{
		existing = magic;
	}
	PowerPC::HostWrite_U32(existing + 1, 0x80001800);
	PowerPC::ppcState.iCache.Reset();
}

}
