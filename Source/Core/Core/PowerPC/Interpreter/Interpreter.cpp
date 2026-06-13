// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <assert.h>
#include <cinttypes>
#include <cstdlib>
#include <fstream>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/Host.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"

#ifdef USE_GDBSTUB
#include "Core/PowerPC/GDBStub.h"
#endif

namespace
{
u32 last_pc;

constexpr u32 MSL_FRAME_INDEX_PTR = 0x804D6CF4;
constexpr u32 MSL_ITEM_COMMON_DATA_PTR = 0x804D6D28;
constexpr u32 MSL_ITEM_COMMON_UNK_DEGREES_OFF = 0xE0;
constexpr u32 MSL_PLAYER_SLOTS = 0x80453080;
constexpr u32 MSL_STATIC_PLAYER_SIZE = 0xE90;
constexpr u32 MSL_GOBJ_USER_DATA_OFF = 0x2C;
constexpr u32 MSL_ITEM_KIND_OFF = 0x10;
constexpr u32 MSL_ITEM_STATE_OFF = 0x24;
constexpr u32 MSL_ITEM_GROUND_OR_AIR_OFF = 0xC0;
constexpr u32 MSL_ITEM_OWNER_OFF = 0x518;
constexpr u32 MSL_ITEM_POS_OFF = 0x4C;
constexpr u32 MSL_ITEM_VEL_OFF = 0x40;
constexpr u32 MSL_ITEM_LIFETIME_OFF = 0xD44;
constexpr u32 MSL_ITEM_HITBOX0_OFF = 0x5D4;
constexpr u32 MSL_ITEM_HITBOX_STRIDE = 0x13C;
constexpr u32 MSL_ITEM_XC34_DAMAGE_DEALT_OFF = 0xC34;
constexpr u32 MSL_ITEM_XC48_CLANK_DAMAGE_OFF = 0xC48;
constexpr u32 MSL_ITEM_XC4C_REFLECT_DAMAGE_OFF = 0xC4C;
constexpr u32 MSL_ITEM_XC50_SHIELD_DAMAGE_OFF = 0xC50;
constexpr u32 MSL_ITEM_XC54_SHIELD_ANGLE_OFF = 0xC54;
constexpr u32 MSL_ITEM_XC58_SHIELD_NORMAL_OFF = 0xC58;
constexpr u32 MSL_ITEM_XC64_REFLECT_GOBJ_OFF = 0xC64;
constexpr u32 MSL_ITEM_XC6C_REFLECT_DAMAGE_MUL_OFF = 0xC6C;
constexpr u32 MSL_ITEM_XC70_REFLECT_SPEED_MUL_OFF = 0xC70;
constexpr u32 MSL_ITEM_XC74_REFLECT_INSTANCE_ID_OFF = 0xC74;
constexpr u32 MSL_ITEM_XC8C_REFLECT_XDA8_SHORT_OFF = 0xC8C;
constexpr u32 MSL_ITEM_XCA8_CALLBACK_DAMAGE_OFF = 0xCA8;
constexpr u32 MSL_ITEM_XCB8_OUT_DAMAGE_DIR_OFF = 0xCB8;
constexpr u32 MSL_ITEM_XCBC_HITLAG_OFF = 0xCBC;
constexpr u32 MSL_ITEM_XCC0_HITLAG_MIN_OFF = 0xCC0;
constexpr u32 MSL_ITEM_XD60_DESTROY_TYPE_OFF = 0xD60;
constexpr u32 MSL_ITEM_XDCC_FLAGS_OFF = 0xDCC;
constexpr u32 MSL_ITEM_XDA8_SHORT_OFF = 0xDA8;
constexpr u32 MSL_ITEM_XDC8_WORD_OFF = 0xDC8;
constexpr u32 MSL_ITEM_XDCE_FLAGS_OFF = 0xDCE;
constexpr u32 MSL_ITEM_XD88_ATTACK_ID_OFF = 0xD88;
constexpr u32 MSL_ITEM_XD8C_ATTACK_INSTANCE_OFF = 0xD8C;
constexpr u32 MSL_ITEM_FOXLASER_SCALE_OFF = 0xDD4;
constexpr u32 MSL_ITEM_FOXLASER_ANGLE_OFF = 0xDD8;
constexpr u32 MSL_ITEM_FOXLASER_SPEED_OFF = 0xDDC;
constexpr u32 MSL_ITEM_FOXLASER_POS_OFF = 0xDE0;
constexpr u32 MSL_FIGHTER_STATE_FLAGS_2218_OFF = 0x2218;
constexpr u32 MSL_FIGHTER_STATE_FLAGS_221B_OFF = 0x221B;
constexpr u32 MSL_FIGHTER_SHIELD_UNK1_OFF = 0x19B8;
constexpr u32 MSL_FIGHTER_REFLECT_DIR_OFF = 0x1A2C;
constexpr u32 MSL_FIGHTER_REFLECT_MAX_DAMAGE_OFF = 0x1A30;
constexpr u32 MSL_FIGHTER_REFLECT_DAMAGE_MUL_OFF = 0x1A34;
constexpr u32 MSL_FIGHTER_REFLECT_SPEED_MUL_OFF = 0x1A38;
constexpr u32 MSL_FIGHTER_VICTIM_GOBJ_OFF = 0x1A58;
constexpr u32 MSL_FIGHTER_X1A70_OFF = 0x1A70;
constexpr u32 MSL_FIGHTER_X2174_OFF = 0x2174;
constexpr u32 MSL_CAPSULE_DAMAGE_OFF = 0x0C;
constexpr u32 MSL_CAPSULE_ELEMENT_OFF = 0x30;
constexpr u32 MSL_CAPSULE_SHIELD_DAMAGE_OFF = 0x34;
constexpr u32 MSL_CAPSULE_FLAGS40_OFF = 0x40;
constexpr u32 MSL_CAPSULE_FLAGS41_OFF = 0x41;
constexpr u32 MSL_CAPSULE_FLAGS42_OFF = 0x42;
constexpr u32 MSL_CAPSULE_HURT_COLL_POS_OFF = 0x64;
constexpr u32 MSL_CAPSULE_COLL_DISTANCE_OFF = 0x70;
constexpr u32 MSL_HITBOX_VICTIMS1_CURSOR_OFF = 0x44;
constexpr u32 MSL_HITBOX_VICTIMS1_BASE_OFF = 0x74;
constexpr u32 MSL_HITBOX_VICTIMS2_CURSOR_OFF = 0x45;
constexpr u32 MSL_HITBOX_VICTIMS2_BASE_OFF = 0xD4;
constexpr u32 MSL_HITBOX_VICTIM_STRIDE = 0x8;
constexpr u32 MSL_HITBOX_OWNER_OFF = 0x134;
constexpr u32 MSL_IT_KIND_FOX_LASER = 54;
constexpr u32 MSL_IT_KIND_FALCO_LASER = 55;

constexpr u32 MSL_FN_IT_8029C6CC = 0x8029C6CC;
constexpr u32 MSL_FN_IT_8029C4D4 = 0x8029C4D4;
constexpr u32 MSL_FN_IT_8026FAC4 = 0x8026FAC4;
constexpr u32 MSL_FN_IT_80272460 = 0x80272460;
constexpr u32 MSL_FN_ITEM_8026A294 = 0x8026A294;
constexpr u32 MSL_FN_ITEM_8026A8EC = 0x8026A8EC;
constexpr u32 MSL_FN_FTCOLL_80077464 = 0x80077464;
constexpr u32 MSL_FN_FTCOLL_80077688 = 0x80077688;
constexpr u32 MSL_FN_ITEM_80269DC8 = 0x80269DC8;
constexpr u32 MSL_FN_ITEM_80269F14 = 0x80269F14;
constexpr u32 MSL_FN_ITFOXLASER_LOGIC94_SHIELDBOUNCED = 0x8029CC54;
constexpr u32 MSL_FN_ITFOXLASER_LOGIC94_HITSHIELD = 0x8029CCF0;
constexpr u32 MSL_FTCOLL_8007699C = 0x8007699C;
constexpr u32 MSL_FTCOLL_80076ED8 = 0x80076ED8;
constexpr u32 MSL_LBCOLL_80007BCC = 0x80007BCC;
constexpr u32 MSL_LBCOLL_80007ECC = 0x80007ECC;
constexpr u32 MSL_LBCOLL_8000805C = 0x8000805C;
constexpr u32 MSL_FTCO_800DE508 = 0x800DE508;
constexpr u32 MSL_FTCO_80091A4C = 0x80091A4C;
constexpr u32 MSL_FTCO_800923B4 = 0x800923B4;
constexpr u32 MSL_FTCO_800939B4 = 0x800939B4;
constexpr u32 MSL_FIGHTER_ACTION_STATE_OFF = 0x10;
constexpr u32 MSL_FIGHTER_ACTION_FRAME_OFF = 0x894;
constexpr u32 MSL_FIGHTER_ATTACK_ID_OFF = 0x2068;
constexpr u32 MSL_FIGHTER_ATTACK_INSTANCE_OFF = 0x206C;
constexpr u32 MSL_FIGHTER_X2070_OFF = 0x2070;
constexpr u32 MSL_FIGHTER_X2088_OFF = 0x2088;
constexpr u32 MSL_FIGHTER_INPUT_X650_OFF = 0x650;
constexpr u32 MSL_FIGHTER_INPUT_X654_OFF = 0x654;
constexpr u32 MSL_FIGHTER_INPUT_HELD_OFF = 0x65C;
constexpr u32 MSL_FIGHTER_INPUT_PREV_HELD_OFF = 0x660;
constexpr u32 MSL_FIGHTER_INPUT_PRESSED_OFF = 0x668;
constexpr u32 MSL_FIGHTER_INPUT_RELEASED_OFF = 0x66C;
constexpr u32 MSL_FIGHTER_X672_INPUT_TIMER_OFF = 0x672;
constexpr u32 MSL_PLATTACK_COUNTER_ADDR = 0x804D6480;
constexpr u32 MSL_FN_PLATTACK_80037B08 = 0x80037B08;
constexpr u32 MSL_FN_FIGHTER_CHANGE_MOTION_STATE = 0x800693AC;
constexpr u32 MSL_FN_FIGHTER_GRAB_PROC = 0x8006CA5C;
constexpr u32 MSL_FN_FT_800895E0 = 0x800895E0;
constexpr u32 MSL_FN_FT_80089824 = 0x80089824;
constexpr u32 MSL_FN_FTCO_DAMAGE_ON_EVERY_HITLAG = 0x8008E4F0;
constexpr u32 MSL_FIGHTER_POS_X_OFF = 0xB0;
constexpr u32 MSL_FIGHTER_POS_Y_OFF = 0xB4;
constexpr u32 MSL_FIGHTER_INPUT_LSTICK_X_OFF = 0x620;
constexpr u32 MSL_FIGHTER_INPUT_LSTICK_Y_OFF = 0x624;
constexpr u32 MSL_FIGHTER_INPUT_LSTICK1_X_OFF = 0x628;
constexpr u32 MSL_FIGHTER_INPUT_LSTICK1_Y_OFF = 0x62C;
constexpr u32 MSL_FIGHTER_X670_TIMER_OFF = 0x670;
constexpr u32 MSL_FIGHTER_X671_TIMER_OFF = 0x671;
constexpr u32 MSL_FIGHTER_STATE_FLAGS_221A_OFF = 0x221A;
constexpr u32 MSL_FIGHTER_STATE_FLAGS_221D_OFF = 0x221D;
constexpr u32 MSL_FIGHTER_HITLAG_FRAMES_OFF = 0x195C;
constexpr u32 MSL_FIGHTER_FTDATA_OFF = 0x10C;
constexpr u32 MSL_FIGHTER_PARTS_OFF = 0x5E8;
constexpr u32 MSL_FTDATA_X8_OFF = 0x08;
constexpr u32 MSL_FTDATA_X8_SHIELD_PART_OFF = 0x11;
constexpr u32 MSL_FIGHTER_PART_STRIDE = 0x10;
constexpr u32 MSL_FIGHTER_PART_JOBJ_OFF = 0x00;
constexpr u32 MSL_JOBJ_FLAGS_OFF = 0x14;
constexpr u32 MSL_JOBJ_ROTATE_OFF = 0x1C;
constexpr u32 MSL_JOBJ_SCALE_OFF = 0x2C;
constexpr u32 MSL_JOBJ_TRANSLATE_OFF = 0x38;
constexpr u32 MSL_JOBJ_MTX_OFF = 0x44;
constexpr u32 MSL_STALE_TABLE_OFF = 0xBC;

struct ThrowLaserEventCall
{
	u32 fn_pc;
	u32 lr;
	u32 arg3;
	u32 arg4;
	u32 arg5;
	u32 arg6;
	u32 arg7;
};

ThrowLaserEventCall g_throw_laser_event_stack[64];
u32 g_throw_laser_event_depth = 0;

struct LaserShieldReflectEventCall
{
	u32 fn_pc;
	u32 lr;
	u32 arg3;
	u32 arg4;
	u32 arg5;
	u32 arg6;
	u32 arg7;
	double fpr1;
};

LaserShieldReflectEventCall g_laser_shield_reflect_event_stack[64];
u32 g_laser_shield_reflect_event_depth = 0;

struct CollisionProbeCall
{
	u32 fn_pc;
	u32 lr;
	u32 attacker_fp;
	u32 hit;
	u32 defender_fp;
	u32 hurt;
	u32 shield_hit;
	u32 matrix;
};

CollisionProbeCall g_collision_probe_stack[32];
u32 g_collision_probe_depth = 0;

struct ThrowAttachProbeCall
{
	u32 lr;
	u32 gobj;
	u32 fighter_fp;
};

ThrowAttachProbeCall g_throw_attach_probe_stack[32];
u32 g_throw_attach_probe_depth = 0;

struct InstanceProbeCall
{
	u32 fn_pc;
	u32 lr;
	u32 arg3;
	u32 arg4;
	u32 arg5;
	u32 arg6;
	u16 pre_iid;
	u32 pre_x2070;
	u16 pre_counter;
};

InstanceProbeCall g_instance_probe_stack[64];
u32 g_instance_probe_depth = 0;

struct DamageSdiProbeCall
{
	u32 lr;
	u32 gobj;
	u32 fighter_fp;
	u32 pre_pos_x_bits;
	u32 pre_pos_y_bits;
	u8 pre_x670;
	u8 pre_x671;
};

DamageSdiProbeCall g_damage_sdi_probe_stack[64];
u32 g_damage_sdi_probe_depth = 0;

u32 ReadEventU32(u32 addr)
{
	if (addr == 0)
		return 0;
	return Memory::Read_U32(addr);
}

u16 ReadEventU16(u32 addr)
{
	if (addr == 0)
		return 0;
	return Memory::Read_U16(addr);
}

u8 ReadEventU8(u32 addr)
{
	if (addr == 0)
		return 0;
	return Memory::Read_U8(addr);
}

s32 ReadEventS32(u32 addr)
{
	return static_cast<s32>(ReadEventU32(addr));
}

float ReadEventF32(u32 addr)
{
	union
	{
		u32 bits;
		float f;
	} value;
	value.bits = ReadEventU32(addr);
	return value.f;
}

void DumpEventVecBits(std::ofstream& out, const char* name, u32 addr)
{
	out << ",\"" << name << "\":[" << ReadEventU32(addr + 0x0) << ","
	    << ReadEventU32(addr + 0x4) << "," << ReadEventU32(addr + 0x8) << "]";
}

void DumpCollisionMtxBits(std::ofstream& out, const char* name, u32 addr)
{
	out << ",\"" << name << "\":[";
	for (u32 i = 0; i < 12; i++)
	{
		if (i)
			out << ",";
		out << ReadEventU32(addr + i * 4);
	}
	out << "]";
}

void DumpCollisionFObjChain(std::ofstream& out, const char* name, u32 fobj)
{
	out << ",\"" << name << "\":[";
	for (u32 i = 0; fobj != 0 && i < 8; i++)
	{
		if (i)
			out << ",";
		out << "{\"ptr\":" << fobj;
		out << ",\"next\":" << ReadEventU32(fobj + 0x00);
		out << ",\"ad\":" << ReadEventU32(fobj + 0x04);
		out << ",\"ad_head\":" << ReadEventU32(fobj + 0x08);
		out << ",\"length\":" << ReadEventU32(fobj + 0x0C);
		out << ",\"flags\":" << static_cast<u32>(ReadEventU8(fobj + 0x10));
		out << ",\"op\":" << static_cast<u32>(ReadEventU8(fobj + 0x11));
		out << ",\"op_intrp\":" << static_cast<u32>(ReadEventU8(fobj + 0x12));
		out << ",\"obj_type\":" << static_cast<u32>(ReadEventU8(fobj + 0x13));
		out << ",\"frac_value\":" << static_cast<u32>(ReadEventU8(fobj + 0x14));
		out << ",\"frac_slope\":" << static_cast<u32>(ReadEventU8(fobj + 0x15));
		out << ",\"nb_pack\":" << ReadEventU16(fobj + 0x16);
		out << ",\"startframe\":" << static_cast<s32>(static_cast<s16>(ReadEventU16(fobj + 0x18)));
		out << ",\"fterm\":" << ReadEventU16(fobj + 0x1A);
		out << ",\"time_bits\":" << ReadEventU32(fobj + 0x1C);
		out << ",\"p0_bits\":" << ReadEventU32(fobj + 0x20);
		out << ",\"p1_bits\":" << ReadEventU32(fobj + 0x24);
		out << ",\"d0_bits\":" << ReadEventU32(fobj + 0x28);
		out << ",\"d1_bits\":" << ReadEventU32(fobj + 0x2C);
		out << "}";
		fobj = ReadEventU32(fobj + 0x00);
	}
	out << "]";
}

void DumpCollisionJObj(std::ofstream& out, const char* name, u32 jobj)
{
	out << ",\"" << name << "\":{\"ptr\":" << jobj;
	if (jobj != 0)
	{
		const u32 aobj = ReadEventU32(jobj + 0x7C);
		out << ",\"parent\":" << ReadEventU32(jobj + 0x0C);
		out << ",\"child\":" << ReadEventU32(jobj + 0x10);
		out << ",\"flags\":" << ReadEventU32(jobj + 0x14);
		DumpEventVecBits(out, "rotate_xyz_bits", jobj + 0x1C);
		out << ",\"rotate_w_bits\":" << ReadEventU32(jobj + 0x28);
		DumpEventVecBits(out, "scale_bits", jobj + 0x2C);
		DumpEventVecBits(out, "translate_bits", jobj + 0x38);
		DumpCollisionMtxBits(out, "mtx_bits", jobj + 0x44);
		out << ",\"aobj\":{\"ptr\":" << aobj;
		if (aobj != 0)
		{
			out << ",\"flags\":" << ReadEventU32(aobj + 0x00);
			out << ",\"curr_frame_bits\":" << ReadEventU32(aobj + 0x04);
			out << ",\"rewind_frame_bits\":" << ReadEventU32(aobj + 0x08);
			out << ",\"end_frame_bits\":" << ReadEventU32(aobj + 0x0C);
			out << ",\"framerate_bits\":" << ReadEventU32(aobj + 0x10);
			out << ",\"fobj\":" << ReadEventU32(aobj + 0x14);
			out << ",\"hsd_obj\":" << ReadEventU32(aobj + 0x18);
			DumpCollisionFObjChain(out, "fobjs", ReadEventU32(aobj + 0x14));
		}
		out << "}";
	}
	out << "}";
}

void DumpCollisionPart(std::ofstream& out, const char* name, u32 parts, u32 idx)
{
	const u32 part = parts + idx * 0x10;
	out << "\"" << name << "\":{\"idx\":" << idx;
	if (parts != 0)
	{
		const u32 joint = ReadEventU32(part + 0x0);
		const u32 jobj2 = ReadEventU32(part + 0x4);
		out << ",\"joint\":" << joint;
		out << ",\"jobj2\":" << jobj2;
		out << ",\"flags8\":" << static_cast<u32>(ReadEventU8(part + 0x8));
		out << ",\"flags9\":" << static_cast<u32>(ReadEventU8(part + 0x9));
		out << ",\"flagsC\":" << static_cast<u32>(ReadEventU8(part + 0xC));
		DumpCollisionJObj(out, "joint_jobj", joint);
		DumpCollisionJObj(out, "jobj2_jobj", jobj2);
	}
	out << "}";
}

void DumpCollisionFighter(std::ofstream& out, const char* name, u32 fp)
{
	out << ",\"" << name << "\":{\"ptr\":" << fp;
	if (fp != 0)
	{
		out << ",\"player_id\":" << static_cast<u32>(ReadEventU8(fp + 0x0C));
		out << ",\"motion_id\":" << ReadEventU32(fp + 0x10);
		out << ",\"anim_id\":" << ReadEventU32(fp + 0x14);
		out << ",\"facing_bits\":" << ReadEventU32(fp + 0x2C);
		DumpEventVecBits(out, "scale_bits", fp + 0x34);
		DumpEventVecBits(out, "cur_pos_bits", fp + 0xB0);
		DumpEventVecBits(out, "prev_pos_bits", fp + 0xBC);
		out << ",\"ground_or_air\":" << ReadEventU32(fp + 0xE0);
		out << ",\"cur_anim_frame_bits\":" << ReadEventU32(fp + 0x894);
		out << ",\"x898_unk_bits\":" << ReadEventU32(fp + 0x898);
		out << ",\"frame_speed_mul_bits\":" << ReadEventU32(fp + 0x89C);
		out << ",\"x8a4_anim_blend_frames_bits\":" << ReadEventU32(fp + 0x8A4);
		out << ",\"x8a8_anim_frame_bits\":" << ReadEventU32(fp + 0x8A8);
		out << ",\"x590_figatree\":" << ReadEventU32(fp + 0x590);
		out << ",\"x594_s32\":" << ReadEventU32(fp + 0x594);
		out << ",\"x598_figatree\":" << ReadEventU32(fp + 0x598);
		out << ",\"x8ac_anim_skeleton\":" << ReadEventU32(fp + 0x8AC);
		out << ",\"parts_ptr\":" << ReadEventU32(fp + 0x5E8);
		out << ",\"dynamics_num\":" << ReadEventU32(fp + 0x3E0);
		out << ",\"victim_gobj\":" << ReadEventU32(fp + MSL_FIGHTER_VICTIM_GOBJ_OFF);
		DumpEventVecBits(out, "x1A70_bits", fp + MSL_FIGHTER_X1A70_OFF);
		DumpEventVecBits(out, "x2174_bits", fp + MSL_FIGHTER_X2174_OFF);
		out << ",\"hit_status_x1988\":" << ReadEventU32(fp + 0x1988);
		out << ",\"colanim_hit_status_x198c\":" << ReadEventU32(fp + 0x198C);
		out << ",\"state_flags_2218\":" << static_cast<u32>(ReadEventU8(fp + 0x2218));
		out << ",\"state_flags_221a\":" << static_cast<u32>(ReadEventU8(fp + 0x221A));
		out << ",\"state_flags_221b\":" << static_cast<u32>(ReadEventU8(fp + 0x221B));
		out << ",\"state_flags_221c\":" << static_cast<u32>(ReadEventU8(fp + 0x221C));
		out << ",\"state_flags_221f\":" << static_cast<u32>(ReadEventU8(fp + 0x221F));
		const u32 parts = ReadEventU32(fp + 0x5E8);
		out << ",\"parts_sample\":{";
		DumpCollisionPart(out, "p0", parts, 0);
		out << ",";
		DumpCollisionPart(out, "p1", parts, 1);
		out << ",";
		DumpCollisionPart(out, "p2", parts, 2);
		out << ",";
		DumpCollisionPart(out, "p3", parts, 3);
		out << ",";
		DumpCollisionPart(out, "p4", parts, 4);
		out << ",";
		DumpCollisionPart(out, "p17", parts, 17);
		out << ",";
		DumpCollisionPart(out, "p18", parts, 18);
		out << ",";
		DumpCollisionPart(out, "p19", parts, 19);
		out << ",";
		DumpCollisionPart(out, "p38", parts, 38);
		out << ",";
		DumpCollisionPart(out, "p39", parts, 39);
		out << ",";
		DumpCollisionPart(out, "p40", parts, 40);
		out << ",";
		DumpCollisionPart(out, "p49", parts, 49);
		out << ",";
		DumpCollisionPart(out, "p50", parts, 50);
		out << ",";
		DumpCollisionPart(out, "p51", parts, 51);
		out << ",";
		DumpCollisionPart(out, "p52", parts, 52);
		out << ",";
		DumpCollisionPart(out, "p53", parts, 53);
		out << "}";
	}
	out << "}";
}

void DumpAttachPartBrief(std::ofstream& out, const char* name, u32 parts, u32 idx)
{
	out << ",\"" << name << "\":{\"idx\":" << idx;
	if (parts != 0)
	{
		const u32 joint = ReadEventU32(parts + idx * 0x10);
		out << ",\"joint\":" << joint;
		if (joint != 0)
		{
			DumpEventVecBits(out, "translate_bits", joint + 0x38);
			DumpCollisionMtxBits(out, "mtx_bits", joint + 0x44);
		}
	}
	out << "}";
}

void DumpThrowAttachFighterBrief(std::ofstream& out, const char* name, u32 fp)
{
	out << ",\"" << name << "\":{\"ptr\":" << fp;
	if (fp != 0)
	{
		out << ",\"player_id\":" << static_cast<u32>(ReadEventU8(fp + 0x0C));
		out << ",\"motion_id\":" << ReadEventU32(fp + 0x10);
		out << ",\"anim_id\":" << ReadEventU32(fp + 0x14);
		out << ",\"facing_bits\":" << ReadEventU32(fp + 0x2C);
		DumpEventVecBits(out, "cur_pos_bits", fp + 0xB0);
		DumpEventVecBits(out, "prev_pos_bits", fp + 0xBC);
		out << ",\"ground_or_air\":" << ReadEventU32(fp + 0xE0);
		out << ",\"cur_anim_frame_bits\":" << ReadEventU32(fp + 0x894);
		out << ",\"frame_speed_mul_bits\":" << ReadEventU32(fp + 0x89C);
		out << ",\"victim_gobj\":" << ReadEventU32(fp + MSL_FIGHTER_VICTIM_GOBJ_OFF);
		DumpEventVecBits(out, "x1A70_bits", fp + MSL_FIGHTER_X1A70_OFF);
		DumpEventVecBits(out, "x2174_bits", fp + MSL_FIGHTER_X2174_OFF);
		const u32 parts = ReadEventU32(fp + 0x5E8);
		out << ",\"parts_brief\":{";
		out << "\"parts_ptr\":" << parts;
		DumpAttachPartBrief(out, "p2_xrotn", parts, 2);
		DumpAttachPartBrief(out, "p52_transn2_enum", parts, 52);
		DumpAttachPartBrief(out, "p71_capture_anchor", parts, 71);
		out << "}";
	}
	out << "}";
}

void DumpCollisionHitCapsule(std::ofstream& out, const char* name, u32 hit)
{
	out << ",\"" << name << "\":{\"ptr\":" << hit;
	if (hit != 0)
	{
		out << ",\"state\":" << ReadEventU32(hit + 0x00);
		out << ",\"group\":" << ReadEventU32(hit + 0x04);
		out << ",\"damage_int\":" << ReadEventU32(hit + 0x08);
		out << ",\"damage_bits\":" << ReadEventU32(hit + 0x0C);
		DumpEventVecBits(out, "offset_bits", hit + 0x10);
		out << ",\"scale_bits\":" << ReadEventU32(hit + 0x1C);
		out << ",\"angle\":" << ReadEventU32(hit + 0x20);
		out << ",\"element\":" << ReadEventU32(hit + 0x30);
		out << ",\"flags\":[";
		for (u32 i = 0; i < 8; i++)
		{
			if (i)
				out << ",";
			out << static_cast<u32>(ReadEventU8(hit + 0x40 + i));
		}
		out << "]";
		out << ",\"bone_ptr\":" << ReadEventU32(hit + 0x48);
		DumpEventVecBits(out, "x4c_bits", hit + 0x4C);
		DumpEventVecBits(out, "x58_bits", hit + 0x58);
		DumpEventVecBits(out, "hurt_coll_pos_bits", hit + 0x64);
		out << ",\"coll_distance_bits\":" << ReadEventU32(hit + 0x70);
	}
	out << "}";
}

void DumpCollisionHurtCapsule(std::ofstream& out, const char* name, u32 hurt)
{
	out << ",\"" << name << "\":{\"ptr\":" << hurt;
	if (hurt != 0)
	{
		out << ",\"state\":" << ReadEventU32(hurt + 0x00);
		DumpEventVecBits(out, "a_offset_bits", hurt + 0x04);
		DumpEventVecBits(out, "b_offset_bits", hurt + 0x10);
		out << ",\"scale_bits\":" << ReadEventU32(hurt + 0x1C);
		out << ",\"bone_ptr\":" << ReadEventU32(hurt + 0x20);
		out << ",\"flags\":" << static_cast<u32>(ReadEventU8(hurt + 0x24));
		DumpEventVecBits(out, "a_pos_bits", hurt + 0x28);
		DumpEventVecBits(out, "b_pos_bits", hurt + 0x34);
		out << ",\"bone_idx\":" << ReadEventU32(hurt + 0x40);
		out << ",\"height\":" << ReadEventU32(hurt + 0x44);
		out << ",\"is_grabbable\":" << static_cast<u32>(ReadEventU8(hurt + 0x48));
		const u32 bone = ReadEventU32(hurt + 0x20);
		DumpCollisionJObj(out, "bone_jobj", bone);
		DumpCollisionJObj(out, "bone_parent_jobj", ReadEventU32(bone + 0x0C));
	}
	out << "}";
}

void DumpCollisionHitResult(std::ofstream& out, const char* name, u32 hit_result)
{
	out << ",\"" << name << "\":{\"ptr\":" << hit_result;
	if (hit_result != 0)
	{
		const u32 bone = ReadEventU32(hit_result + 0x00);
		out << ",\"bone\":" << bone;
		out << ",\"skip_update_pos_byte\":" << static_cast<u32>(ReadEventU8(hit_result + 0x04));
		DumpEventVecBits(out, "pos_bits", hit_result + 0x08);
		DumpEventVecBits(out, "offset_bits", hit_result + 0x14);
		out << ",\"size_bits\":" << ReadEventU32(hit_result + 0x20);
		DumpCollisionJObj(out, "bone_jobj", bone);
	}
	out << "}";
}

u32 FighterGobjForPort(int port)
{
	u32 base = MSL_PLAYER_SLOTS + static_cast<u32>(port - 1) * MSL_STATIC_PLAYER_SIZE;
	u8 transformed = ReadEventU8(base + 0x0C);
	return ReadEventU32(base + 0xB0 + static_cast<u32>(transformed) * 4);
}

int OwnerPortFromGobj(u32 gobj)
{
	for (int port = 1; port <= 4; port++)
	{
		if (gobj != 0 && FighterGobjForPort(port) == gobj)
			return port;
	}
	return -1;
}

u32 FighterDataForPort(int port)
{
	const u32 gobj = FighterGobjForPort(port);
	if (gobj == 0)
		return 0;
	return ReadEventU32(gobj + MSL_GOBJ_USER_DATA_OFF);
}

u32 FighterGobjFromData(u32 fighter_data)
{
	for (int port = 1; port <= 4; port++)
	{
		const u32 gobj = FighterGobjForPort(port);
		if (gobj != 0 && ReadEventU32(gobj + MSL_GOBJ_USER_DATA_OFF) == fighter_data)
			return gobj;
	}
	return 0;
}

u32 FighterDataFromGobj(u32 gobj)
{
	if (gobj == 0)
		return 0;
	return ReadEventU32(gobj + MSL_GOBJ_USER_DATA_OFF);
}

void DumpStaleTable(std::ofstream& out, const char* name, int player_id)
{
	out << ",\"" << name << "\":{";
	if (player_id < 0 || player_id >= 6)
	{
		out << "\"available\":0}";
		return;
	}

	const u32 base = MSL_PLAYER_SLOTS + static_cast<u32>(player_id) * MSL_STATIC_PLAYER_SIZE +
	                 MSL_STALE_TABLE_OFF;
	out << "\"available\":1";
	out << ",\"base\":" << base;
	out << ",\"current_index\":" << ReadEventU32(base);
	out << ",\"move_id\":[";
	for (u32 i = 0; i < 10; i++)
	{
		if (i)
			out << ",";
		out << ReadEventU16(base + 0x4 + i * 4);
	}
	out << "],\"attack_instance\":[";
	for (u32 i = 0; i < 10; i++)
	{
		if (i)
			out << ",";
		out << ReadEventU16(base + 0x6 + i * 4);
	}
	out << "]}";
}

int FighterPortFromData(u32 fighter_data)
{
	for (int port = 1; port <= 4; port++)
	{
		if (fighter_data != 0 && FighterDataForPort(port) == fighter_data)
			return port;
	}
	return -1;
}

const char* InstanceProbeFnName(u32 pc)
{
	switch (pc)
	{
	case MSL_FN_PLATTACK_80037B08:
		return "plAttack_80037B08";
	case MSL_FN_FIGHTER_CHANGE_MOTION_STATE:
		return "Fighter_ChangeMotionState";
	case MSL_FN_FIGHTER_GRAB_PROC:
		return "Fighter_UnkProcessGrab_8006CA5C";
	case MSL_FN_FT_800895E0:
		return "ft_800895E0";
	case MSL_FN_FT_80089824:
		return "ft_80089824";
	default:
		return "unknown";
	}
}

u32 InstanceProbeFighterFromCall(u32 pc, u32 arg3)
{
	if (pc == MSL_FN_FT_800895E0)
		return arg3;
	if (pc == MSL_FN_PLATTACK_80037B08)
		return 0;
	return FighterDataFromGobj(arg3);
}

void DumpInstanceProbeFighter(std::ofstream& out, const char* name, u32 fp)
{
	out << ",\"" << name << "\":{\"ptr\":" << fp;
	if (fp != 0)
	{
		out << ",\"port\":" << FighterPortFromData(fp);
		out << ",\"action\":" << ReadEventU32(fp + MSL_FIGHTER_ACTION_STATE_OFF);
		out << ",\"action_frame_bits\":" << ReadEventU32(fp + MSL_FIGHTER_ACTION_FRAME_OFF);
		out << ",\"x2070\":" << ReadEventU32(fp + MSL_FIGHTER_X2070_OFF);
		out << ",\"x2073\":" << static_cast<u32>(ReadEventU8(fp + MSL_FIGHTER_X2070_OFF + 3));
		out << ",\"x2088\":" << ReadEventU16(fp + MSL_FIGHTER_X2088_OFF);
	}
	out << "}";
}

void EmitInstanceProbeEvent(std::ofstream& out, const char* phase, const InstanceProbeCall& call,
                            u32 return_r3)
{
	const u32 fp = InstanceProbeFighterFromCall(call.fn_pc, call.arg3);
	out << "{\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
	out << ",\"phase\":\"" << phase << "\"";
	out << ",\"fn\":\"" << InstanceProbeFnName(call.fn_pc) << "\"";
	out << ",\"pc\":" << call.fn_pc;
	out << ",\"lr\":" << call.lr;
	out << ",\"return_r3\":" << return_r3;
	out << ",\"args\":[" << call.arg3 << "," << call.arg4 << "," << call.arg5 << ","
	    << call.arg6 << "]";
	out << ",\"pre_iid\":" << call.pre_iid;
	out << ",\"pre_x2070\":" << call.pre_x2070;
	out << ",\"pre_counter\":" << call.pre_counter;
	out << ",\"counter\":" << ReadEventU16(MSL_PLATTACK_COUNTER_ADDR);
	DumpInstanceProbeFighter(out, "fighter", fp);
	out << "}\n";
}

void MaybeCaptureInstanceProbe(u32 pc)
{
	static bool initialized = false;
	static bool enabled = false;
	static s32 frame_start = -2147483647;
	static s32 frame_end = 2147483647;
	static std::ofstream out;
	if (!initialized)
	{
		initialized = true;
		const char* path = std::getenv("MSL_INSTANCE_PROBE_PATH");
		if (path != nullptr && path[0] != '\0')
		{
			out.open(path, std::ios::out | std::ios::app);
			enabled = out.good();
		}
		const char* start = std::getenv("MSL_INSTANCE_PROBE_FRAME_START");
		if (start != nullptr && start[0] != '\0')
			frame_start = std::atoi(start);
		const char* end = std::getenv("MSL_INSTANCE_PROBE_FRAME_END");
		if (end != nullptr && end[0] != '\0')
			frame_end = std::atoi(end);
	}
	if (!enabled)
		return;

	const s32 frame = static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
	if (frame < frame_start || frame > frame_end)
		return;

	if (g_instance_probe_depth > 0 && pc == g_instance_probe_stack[g_instance_probe_depth - 1].lr)
	{
		const InstanceProbeCall call = g_instance_probe_stack[g_instance_probe_depth - 1];
		g_instance_probe_depth--;
		EmitInstanceProbeEvent(out, "return", call, PowerPC::ppcState.gpr[3]);
	}

	if (pc != MSL_FN_PLATTACK_80037B08 && pc != MSL_FN_FIGHTER_CHANGE_MOTION_STATE &&
	    pc != MSL_FN_FIGHTER_GRAB_PROC && pc != MSL_FN_FT_800895E0 &&
	    pc != MSL_FN_FT_80089824)
		return;

	if (g_instance_probe_depth <
	    sizeof(g_instance_probe_stack) / sizeof(g_instance_probe_stack[0]))
	{
		InstanceProbeCall& call = g_instance_probe_stack[g_instance_probe_depth++];
		call.fn_pc = pc;
		call.lr = LR;
		call.arg3 = PowerPC::ppcState.gpr[3];
		call.arg4 = PowerPC::ppcState.gpr[4];
		call.arg5 = PowerPC::ppcState.gpr[5];
		call.arg6 = PowerPC::ppcState.gpr[6];
		const u32 fp = InstanceProbeFighterFromCall(pc, call.arg3);
		call.pre_iid = fp ? ReadEventU16(fp + MSL_FIGHTER_X2088_OFF) : 0;
		call.pre_x2070 = fp ? ReadEventU32(fp + MSL_FIGHTER_X2070_OFF) : 0;
		call.pre_counter = ReadEventU16(MSL_PLATTACK_COUNTER_ADDR);
		EmitInstanceProbeEvent(out, "entry", call, 0);
	}
}

bool IsThrowLaserKind(u32 kind)
{
	return kind == MSL_IT_KIND_FOX_LASER || kind == MSL_IT_KIND_FALCO_LASER;
}

const char* ThrowLaserFnName(u32 pc)
{
	switch (pc)
	{
	case MSL_FN_IT_8029C6CC:
		return "it_8029C6CC";
	case MSL_FN_IT_8029C4D4:
		return "it_8029C4D4";
	case MSL_FN_IT_8026FAC4:
		return "it_8026FAC4";
	case MSL_FN_IT_80272460:
		return "it_80272460";
	case MSL_FN_ITEM_8026A294:
		return "Item_8026A294";
	case MSL_FN_ITEM_8026A8EC:
		return "Item_8026A8EC";
	default:
		return "unknown";
	}
}

const char* ThrowLaserEventName(u32 pc)
{
	switch (pc)
	{
	case MSL_FN_IT_8029C6CC:
		return "spawn";
	case MSL_FN_IT_8029C4D4:
		return "laser_collision";
	case MSL_FN_IT_8026FAC4:
		return "body_hitlist";
	case MSL_FN_IT_80272460:
		return "damage_scale";
	case MSL_FN_ITEM_8026A294:
		return "give_damage";
	case MSL_FN_ITEM_8026A8EC:
		return "destroy";
	default:
		return "unknown";
	}
}

void DumpEventVec(std::ofstream& out, const char* name, u32 addr)
{
	out << ",\"" << name << "\":[";
	out << ReadEventU32(addr + 0x0) << "," << ReadEventU32(addr + 0x4) << ","
	    << ReadEventU32(addr + 0x8) << "]";
}

void DumpEventVictimList(std::ofstream& out, const char* name, u32 base)
{
	out << ",\"" << name << "_ptr\":[";
	for (u32 i = 0; i < 12; i++)
	{
		if (i)
			out << ",";
		out << ReadEventU32(base + i * MSL_HITBOX_VICTIM_STRIDE);
	}
	out << "],\"" << name << "_cooldown\":[";
	for (u32 i = 0; i < 12; i++)
	{
		if (i)
			out << ",";
		out << ReadEventU32(base + i * MSL_HITBOX_VICTIM_STRIDE + 0x4);
	}
	out << "],\"" << name << "_active_slots\":[";
	bool first = true;
	for (u32 i = 0; i < 12; i++)
	{
		if (ReadEventU32(base + i * MSL_HITBOX_VICTIM_STRIDE) == 0)
			continue;
		if (!first)
			out << ",";
		first = false;
		out << i;
	}
	out << "]";
}

void DumpEventHitbox(std::ofstream& out, const char* name, u32 item_data, u32 hit)
{
	out << ",\"" << name << "\":{";
	out << "\"ptr\":" << hit;
	if (hit != 0)
	{
		int index = -1;
		if (item_data != 0)
		{
			for (int i = 0; i < 4; i++)
			{
				if (hit == item_data + MSL_ITEM_HITBOX0_OFF +
				               static_cast<u32>(i) * MSL_ITEM_HITBOX_STRIDE)
				{
					index = i;
					break;
				}
			}
		}
		out << ",\"index\":" << index;
		out << ",\"state\":" << ReadEventU32(hit + 0x00);
		out << ",\"group\":" << ReadEventU32(hit + 0x04);
		out << ",\"damage_int\":" << ReadEventU32(hit + 0x08);
		out << ",\"damage_bits\":" << ReadEventU32(hit + 0x0C);
		DumpEventVec(out, "offset_bits", hit + 0x10);
		out << ",\"scale_bits\":" << ReadEventU32(hit + 0x1C);
		out << ",\"angle\":" << ReadEventU32(hit + 0x20);
		out << ",\"element\":" << ReadEventU32(hit + 0x30);
		out << ",\"victims1_cursor\":" << static_cast<u32>(ReadEventU8(hit + MSL_HITBOX_VICTIMS1_CURSOR_OFF));
		out << ",\"victims2_cursor\":" << static_cast<u32>(ReadEventU8(hit + MSL_HITBOX_VICTIMS2_CURSOR_OFF));
		out << ",\"owner_gobj\":" << ReadEventU32(hit + MSL_HITBOX_OWNER_OFF);
		DumpEventVictimList(out, "victims1", hit + MSL_HITBOX_VICTIMS1_BASE_OFF);
		DumpEventVictimList(out, "victims2", hit + MSL_HITBOX_VICTIMS2_BASE_OFF);
	}
	out << "}";
}

void DumpEventItem(std::ofstream& out, const char* name, u32 item_gobj, u32 item_data)
{
	out << ",\"" << name << "\":{";
	out << "\"gobj\":" << item_gobj << ",\"ptr\":" << item_data;
	if (item_data != 0)
	{
		const u32 owner_gobj = ReadEventU32(item_data + MSL_ITEM_OWNER_OFF);
		const int owner_port = OwnerPortFromGobj(owner_gobj);
		const int owner_player_id = owner_port > 0 ? owner_port - 1 : -1;
		const u32 owner_fp = FighterDataFromGobj(owner_gobj);
		const u32 common_data = ReadEventU32(MSL_ITEM_COMMON_DATA_PTR);
		const float unk_degrees = ReadEventF32(common_data + MSL_ITEM_COMMON_UNK_DEGREES_OFF);
		const float shield_bounce_threshold = (90.0f + unk_degrees) * 0.017453292519943295f;
		const u8 xdce_flags = ReadEventU8(item_data + MSL_ITEM_XDCE_FLAGS_OFF);
		const bool xdce_b5 = (xdce_flags & 0x04) != 0;
		const bool xdce_b4 = (xdce_flags & 0x08) != 0;
		const u32 ground_or_air = ReadEventU32(item_data + MSL_ITEM_GROUND_OR_AIR_OFF);
		const float xC54 = ReadEventF32(item_data + MSL_ITEM_XC54_SHIELD_ANGLE_OFF);
		const bool shield_bounce_predicate =
		    xdce_b5 && ground_or_air == 1 && (xdce_b4 || xC54 < shield_bounce_threshold);
		out << ",\"kind\":" << (ReadEventU32(item_data + MSL_ITEM_KIND_OFF) & 0xFFFF);
		out << ",\"state\":" << (ReadEventU32(item_data + MSL_ITEM_STATE_OFF) & 0xFFFF);
		out << ",\"ground_or_air\":" << ground_or_air;
		out << ",\"owner_gobj\":" << owner_gobj;
		out << ",\"owner_port\":" << owner_port;
		out << ",\"xD88_attack_id\":" << ReadEventU32(item_data + MSL_ITEM_XD88_ATTACK_ID_OFF);
		out << ",\"xD8C_attack_instance\":" << ReadEventU16(item_data + MSL_ITEM_XD8C_ATTACK_INSTANCE_OFF);
		out << ",\"owner_fighter_attack_id\":"
		    << (owner_fp != 0 ? ReadEventU32(owner_fp + MSL_FIGHTER_ATTACK_ID_OFF) : 0);
		out << ",\"owner_fighter_attack_instance\":"
		    << (owner_fp != 0 ? ReadEventU16(owner_fp + MSL_FIGHTER_ATTACK_INSTANCE_OFF) : 0);
		DumpStaleTable(out, "owner_stale_table", owner_player_id);
		out << ",\"xDA8_short\":" << ReadEventU16(item_data + MSL_ITEM_XDA8_SHORT_OFF);
		out << ",\"xC34_damage_dealt\":" << ReadEventS32(item_data + MSL_ITEM_XC34_DAMAGE_DEALT_OFF);
		out << ",\"xC48_clank_damage\":" << ReadEventS32(item_data + MSL_ITEM_XC48_CLANK_DAMAGE_OFF);
		out << ",\"xC4C_reflect_damage\":" << ReadEventS32(item_data + MSL_ITEM_XC4C_REFLECT_DAMAGE_OFF);
		out << ",\"xC50_shield_damage\":" << ReadEventS32(item_data + MSL_ITEM_XC50_SHIELD_DAMAGE_OFF);
		out << ",\"xC54_bits\":" << ReadEventU32(item_data + MSL_ITEM_XC54_SHIELD_ANGLE_OFF);
		DumpEventVec(out, "xC58_bits", item_data + MSL_ITEM_XC58_SHIELD_NORMAL_OFF);
		out << ",\"xC64_reflect_gobj\":" << ReadEventU32(item_data + MSL_ITEM_XC64_REFLECT_GOBJ_OFF);
		out << ",\"xC64_reflect_port\":"
		    << OwnerPortFromGobj(ReadEventU32(item_data + MSL_ITEM_XC64_REFLECT_GOBJ_OFF));
		out << ",\"xC6C_damage_mul_bits\":" << ReadEventU32(item_data + MSL_ITEM_XC6C_REFLECT_DAMAGE_MUL_OFF);
		out << ",\"xC70_speed_mul_bits\":" << ReadEventU32(item_data + MSL_ITEM_XC70_REFLECT_SPEED_MUL_OFF);
		out << ",\"xC74_reflect_instance_id\":" << ReadEventU32(item_data + MSL_ITEM_XC74_REFLECT_INSTANCE_ID_OFF);
		out << ",\"xC8C_reflect_xDA8_short\":" << ReadEventU16(item_data + MSL_ITEM_XC8C_REFLECT_XDA8_SHORT_OFF);
		out << ",\"xCA8_callback_damage\":" << ReadEventS32(item_data + MSL_ITEM_XCA8_CALLBACK_DAMAGE_OFF);
		out << ",\"xCB8_out_damage_dir_bits\":" << ReadEventU32(item_data + MSL_ITEM_XCB8_OUT_DAMAGE_DIR_OFF);
		out << ",\"xCBC_hitlag_bits\":" << ReadEventU32(item_data + MSL_ITEM_XCBC_HITLAG_OFF);
		out << ",\"xCC0_hitlag_min_bits\":" << ReadEventU32(item_data + MSL_ITEM_XCC0_HITLAG_MIN_OFF);
		out << ",\"destroy_type\":" << ReadEventU32(item_data + MSL_ITEM_XD60_DESTROY_TYPE_OFF);
		out << ",\"xDCC_flags\":" << static_cast<u32>(ReadEventU8(item_data + MSL_ITEM_XDCC_FLAGS_OFF));
		out << ",\"xDC8_word\":" << ReadEventU32(item_data + MSL_ITEM_XDC8_WORD_OFF);
		out << ",\"xDCE_flags\":" << static_cast<u32>(xdce_flags);
		out << ",\"xDCE_b5\":" << (xdce_b5 ? 1 : 0);
		out << ",\"xDCE_b4\":" << (xdce_b4 ? 1 : 0);
		out << ",\"item_common_unk_degrees\":" << unk_degrees;
		out << ",\"shield_bounce_threshold\":" << shield_bounce_threshold;
		out << ",\"shield_bounce_predicate\":" << (shield_bounce_predicate ? 1 : 0);
		out << ",\"lifetime_bits\":" << ReadEventU32(item_data + MSL_ITEM_LIFETIME_OFF);
		DumpEventVec(out, "pos_bits", item_data + MSL_ITEM_POS_OFF);
		DumpEventVec(out, "vel_bits", item_data + MSL_ITEM_VEL_OFF);
		out << ",\"laser_scale_bits\":" << ReadEventU32(item_data + MSL_ITEM_FOXLASER_SCALE_OFF);
		out << ",\"laser_angle_bits\":" << ReadEventU32(item_data + MSL_ITEM_FOXLASER_ANGLE_OFF);
		out << ",\"laser_speed_bits\":" << ReadEventU32(item_data + MSL_ITEM_FOXLASER_SPEED_OFF);
		DumpEventVec(out, "laser_pos_bits", item_data + MSL_ITEM_FOXLASER_POS_OFF);
		out << ",\"hitboxes\":[";
		for (u32 i = 0; i < 4; i++)
		{
			if (i)
				out << ",";
			out << "{";
			u32 hit = item_data + MSL_ITEM_HITBOX0_OFF + i * MSL_ITEM_HITBOX_STRIDE;
			out << "\"index\":" << i;
			out << ",\"state\":" << ReadEventU32(hit + 0x00);
			out << ",\"group\":" << ReadEventU32(hit + 0x04);
			out << ",\"damage_int\":" << ReadEventU32(hit + 0x08);
			out << ",\"damage_bits\":" << ReadEventU32(hit + 0x0C);
			DumpEventVecBits(out, "offset_bits", hit + 0x10);
			out << ",\"scale_bits\":" << ReadEventU32(hit + 0x1C);
			out << ",\"angle\":" << ReadEventU32(hit + 0x20);
			out << ",\"element\":" << ReadEventU32(hit + 0x30);
			out << ",\"flags\":[";
			for (u32 j = 0; j < 8; j++)
			{
				if (j)
					out << ",";
				out << static_cast<u32>(ReadEventU8(hit + 0x40 + j));
			}
			out << "]";
			out << ",\"bone_ptr\":" << ReadEventU32(hit + 0x48);
			DumpEventVecBits(out, "x4c_bits", hit + 0x4C);
			DumpEventVecBits(out, "x58_bits", hit + 0x58);
			DumpEventVecBits(out, "hurt_coll_pos_bits", hit + 0x64);
			out << ",\"coll_distance_bits\":" << ReadEventU32(hit + 0x70);
			out << ",\"victims1_cursor\":" << static_cast<u32>(ReadEventU8(hit + MSL_HITBOX_VICTIMS1_CURSOR_OFF));
			DumpEventVictimList(out, "victims1", hit + MSL_HITBOX_VICTIMS1_BASE_OFF);
			out << "}";
		}
		out << "]";
	}
	out << "}";
}

void DumpEventShieldJObj(std::ofstream& out, const char* name, u32 fighter_data);

void DumpEventFighter(std::ofstream& out, const char* name, u32 fighter_data)
{
	out << ",\"" << name << "\":{";
	out << "\"ptr\":" << fighter_data;
	if (fighter_data != 0)
	{
		const u32 fighter_gobj = FighterGobjFromData(fighter_data);
		out << ",\"gobj\":" << fighter_gobj;
		out << ",\"port\":" << FighterPortFromData(fighter_data);
		out << ",\"action\":" << (ReadEventU32(fighter_data + MSL_FIGHTER_ACTION_STATE_OFF) & 0xFFFF);
		out << ",\"action_frame_bits\":" << ReadEventU32(fighter_data + MSL_FIGHTER_ACTION_FRAME_OFF);
		out << ",\"state_flags_2218\":" << static_cast<u32>(ReadEventU8(fighter_data + MSL_FIGHTER_STATE_FLAGS_2218_OFF));
		out << ",\"state_flags_221B\":" << static_cast<u32>(ReadEventU8(fighter_data + MSL_FIGHTER_STATE_FLAGS_221B_OFF));
		out << ",\"shield_unk1_bits\":" << ReadEventU32(fighter_data + MSL_FIGHTER_SHIELD_UNK1_OFF);
		out << ",\"reflect_dir_bits\":" << ReadEventU32(fighter_data + MSL_FIGHTER_REFLECT_DIR_OFF);
		out << ",\"reflect_max_damage\":" << ReadEventS32(fighter_data + MSL_FIGHTER_REFLECT_MAX_DAMAGE_OFF);
		out << ",\"reflect_damage_mul_bits\":" << ReadEventU32(fighter_data + MSL_FIGHTER_REFLECT_DAMAGE_MUL_OFF);
		out << ",\"reflect_speed_mul_bits\":" << ReadEventU32(fighter_data + MSL_FIGHTER_REFLECT_SPEED_MUL_OFF);
		DumpEventShieldJObj(out, "shield_jobj", fighter_data);
	}
	out << "}";
}

void DumpEventShieldJObj(std::ofstream& out, const char* name, u32 fighter_data)
{
	out << ",\"" << name << "\":{";
	out << "\"fighter\":" << fighter_data;
	if (fighter_data != 0)
	{
		const u32 ftdata = ReadEventU32(fighter_data + MSL_FIGHTER_FTDATA_OFF);
		const u32 parts = ReadEventU32(fighter_data + MSL_FIGHTER_PARTS_OFF);
		u32 shield_part = 0xFFFFFFFF;
		u32 jobj = 0;
		if (ftdata != 0)
		{
			const u32 x8 = ReadEventU32(ftdata + MSL_FTDATA_X8_OFF);
			if (x8 != 0)
				shield_part = static_cast<u32>(ReadEventU8(x8 + MSL_FTDATA_X8_SHIELD_PART_OFF));
		}
		if (parts != 0 && shield_part != 0xFFFFFFFF)
			jobj = ReadEventU32(parts + shield_part * MSL_FIGHTER_PART_STRIDE +
			                    MSL_FIGHTER_PART_JOBJ_OFF);
		out << ",\"ftdata\":" << ftdata;
		out << ",\"parts\":" << parts;
		out << ",\"shield_part\":" << shield_part;
		out << ",\"jobj\":" << jobj;
		if (jobj != 0)
		{
			out << ",\"flags\":" << ReadEventU32(jobj + MSL_JOBJ_FLAGS_OFF);
			DumpEventVec(out, "rotate_bits", jobj + MSL_JOBJ_ROTATE_OFF);
			DumpEventVec(out, "scale_bits", jobj + MSL_JOBJ_SCALE_OFF);
			DumpEventVec(out, "translate_bits", jobj + MSL_JOBJ_TRANSLATE_OFF);
			out << ",\"mtx_bits\":[";
			for (u32 i = 0; i < 12; i++)
			{
				if (i != 0)
					out << ",";
				out << ReadEventU32(jobj + MSL_JOBJ_MTX_OFF + i * 4);
			}
			out << "]";
		}
	}
	out << "}";
}

void DumpEventCapsule(std::ofstream& out, const char* name, u32 capsule)
{
	out << ",\"" << name << "\":{";
	out << "\"ptr\":" << capsule;
	if (capsule != 0)
	{
		out << ",\"state\":" << ReadEventU32(capsule + 0x00);
		out << ",\"group\":" << ReadEventU32(capsule + 0x04);
		out << ",\"damage_bits\":" << ReadEventU32(capsule + MSL_CAPSULE_DAMAGE_OFF);
		out << ",\"element\":" << ReadEventU32(capsule + MSL_CAPSULE_ELEMENT_OFF);
		out << ",\"shield_damage\":" << ReadEventS32(capsule + MSL_CAPSULE_SHIELD_DAMAGE_OFF);
		out << ",\"flags40\":" << static_cast<u32>(ReadEventU16(capsule + MSL_CAPSULE_FLAGS40_OFF));
		out << ",\"flags41\":" << static_cast<u32>(ReadEventU8(capsule + MSL_CAPSULE_FLAGS41_OFF));
		out << ",\"flags42\":" << static_cast<u32>(ReadEventU8(capsule + MSL_CAPSULE_FLAGS42_OFF));
		out << ",\"coll_distance_bits\":" << ReadEventU32(capsule + MSL_CAPSULE_COLL_DISTANCE_OFF);
		DumpEventVec(out, "hurt_coll_pos_bits", capsule + MSL_CAPSULE_HURT_COLL_POS_OFF);
	}
	out << "}";
}

const char* LaserShieldReflectFnName(u32 pc)
{
	switch (pc)
	{
	case MSL_FN_FTCOLL_80077464:
		return "ftColl_80077464";
	case MSL_FN_FTCOLL_80077688:
		return "ftColl_80077688";
	case MSL_FN_ITEM_80269DC8:
		return "Item_80269DC8";
	case MSL_FN_ITEM_80269F14:
		return "Item_80269F14";
	case MSL_FN_ITFOXLASER_LOGIC94_SHIELDBOUNCED:
		return "itFoxLaser_Logic94_ShieldBounced";
	case MSL_FN_ITFOXLASER_LOGIC94_HITSHIELD:
		return "itFoxLaser_Logic94_HitShield";
	case MSL_FN_ITEM_8026A8EC:
		return "Item_8026A8EC";
	default:
		return "unknown";
	}
}

const char* LaserShieldReflectEventName(u32 pc)
{
	switch (pc)
	{
	case MSL_FN_FTCOLL_80077464:
		return "reflect_collision";
	case MSL_FN_FTCOLL_80077688:
		return "shield_collision";
	case MSL_FN_ITEM_80269DC8:
		return "shield_branch";
	case MSL_FN_ITEM_80269F14:
		return "reflect_owner_transfer";
	case MSL_FN_ITFOXLASER_LOGIC94_SHIELDBOUNCED:
		return "shield_bounced";
	case MSL_FN_ITFOXLASER_LOGIC94_HITSHIELD:
		return "hit_shield";
	case MSL_FN_ITEM_8026A8EC:
		return "destroy";
	default:
		return "unknown";
	}
}

void EmitLaserShieldReflectEvent(std::ofstream& out, const char* phase, u32 fn_pc, u32 item_gobj,
                                 u32 item_data, u32 fighter_data, u32 capsule,
                                 u32 raw_a, u32 raw_b, u32 raw_c, u32 raw_d, double raw_fpr1,
                                 bool has_return, u32 return_value)
{
	if (item_data == 0)
		return;
	const u32 kind = ReadEventU32(item_data + MSL_ITEM_KIND_OFF) & 0xFFFF;
	if (!IsThrowLaserKind(kind))
		return;
	out << "{\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
	out << ",\"phase\":\"" << phase << "\"";
	out << ",\"event\":\"" << LaserShieldReflectEventName(fn_pc) << "\"";
	out << ",\"fn\":\"" << LaserShieldReflectFnName(fn_pc) << "\"";
	out << ",\"pc\":" << fn_pc;
	out << ",\"lr\":" << LR;
	out << ",\"raw_args\":[" << raw_a << "," << raw_b << "," << raw_c << "," << raw_d << "]";
	out << ",\"raw_fpr1\":" << raw_fpr1;
	if (has_return)
		out << ",\"return_value\":" << return_value;
	if (fn_pc == MSL_FN_ITEM_80269DC8 || fn_pc == MSL_FN_ITEM_80269F14)
		out << ",\"outcome\":\"" << ((has_return && return_value != 0) ? "destroy" : "keepalive")
		    << "\"";
	DumpEventItem(out, "item", item_gobj, item_data);
	if (fighter_data != 0)
		DumpEventFighter(out, "fighter", fighter_data);
	if (capsule != 0)
		DumpEventCapsule(out, "capsule", capsule);
	if (fn_pc == MSL_FN_FTCOLL_80077688 && raw_d != 0)
		DumpEventVec(out, "shield_coll_pos_bits", raw_d);
	out << "}\n";
}

void MaybeCaptureLaserShieldReflectEvents(u32 pc)
{
	static bool initialized = false;
	static bool enabled = false;
	static std::ofstream out;
	if (!initialized)
	{
		initialized = true;
		const char* path = std::getenv("MSL_LASER_SHIELD_REFLECT_EVENT_PROBE_PATH");
		if (path != nullptr && path[0] != '\0')
		{
			out.open(path, std::ios::out | std::ios::app);
			enabled = out.good();
		}
	}
	if (!enabled)
		return;

	if (pc == MSL_FTCO_80091A4C || pc == MSL_FTCO_800923B4 || pc == MSL_FTCO_800939B4)
	{
		u32 gobj = PowerPC::ppcState.gpr[3];
		u32 fighter = ReadEventU32(gobj + MSL_GOBJ_USER_DATA_OFF);
		if (fighter != 0)
		{
			const char* fn = (pc == MSL_FTCO_80091A4C)
			                     ? "ftCo_80091A4C"
			                     : ((pc == MSL_FTCO_800923B4) ? "ftCo_800923B4" : "ftCo_800939B4");
			out << "{\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
			out << ",\"phase\":\"entry\"";
			out << ",\"event\":\"guard_input\"";
			out << ",\"fn\":\"" << fn << "\"";
			out << ",\"pc\":" << pc;
			out << ",\"lr\":" << LR;
			out << ",\"fighter\":{";
			out << "\"ptr\":" << fighter;
			out << ",\"gobj\":" << gobj;
			out << ",\"action\":" << (ReadEventU32(fighter + MSL_FIGHTER_ACTION_STATE_OFF) & 0xFFFF);
			out << ",\"action_frame_bits\":" << ReadEventU32(fighter + MSL_FIGHTER_ACTION_FRAME_OFF);
			out << ",\"x650_bits\":" << ReadEventU32(fighter + MSL_FIGHTER_INPUT_X650_OFF);
			out << ",\"x654_bits\":" << ReadEventU32(fighter + MSL_FIGHTER_INPUT_X654_OFF);
			out << ",\"held_inputs\":" << ReadEventU32(fighter + MSL_FIGHTER_INPUT_HELD_OFF);
			out << ",\"prev_held_inputs\":" << ReadEventU32(fighter + MSL_FIGHTER_INPUT_PREV_HELD_OFF);
			out << ",\"x668_pressed\":" << ReadEventU32(fighter + MSL_FIGHTER_INPUT_PRESSED_OFF);
			out << ",\"x66c_released\":" << ReadEventU32(fighter + MSL_FIGHTER_INPUT_RELEASED_OFF);
			out << ",\"x672\":" << static_cast<u32>(ReadEventU8(fighter + MSL_FIGHTER_X672_INPUT_TIMER_OFF));
			out << ",\"state_flags_2218\":"
			    << static_cast<u32>(ReadEventU8(fighter + MSL_FIGHTER_STATE_FLAGS_2218_OFF));
			out << ",\"state_flags_221B\":"
			    << static_cast<u32>(ReadEventU8(fighter + MSL_FIGHTER_STATE_FLAGS_221B_OFF));
			DumpEventShieldJObj(out, "shield_jobj", fighter);
			out << "}}\n";
		}
	}

	if (g_laser_shield_reflect_event_depth > 0 &&
	    pc == g_laser_shield_reflect_event_stack[g_laser_shield_reflect_event_depth - 1].lr)
	{
		const LaserShieldReflectEventCall call =
		    g_laser_shield_reflect_event_stack[g_laser_shield_reflect_event_depth - 1];
		g_laser_shield_reflect_event_depth--;
		u32 item_gobj = 0;
		u32 item_data = 0;
		u32 fighter_data = 0;
		u32 capsule = 0;
		if (call.fn_pc == MSL_FN_FTCOLL_80077464 || call.fn_pc == MSL_FN_FTCOLL_80077688)
		{
			item_data = call.arg3;
			fighter_data = call.arg5;
			capsule = call.arg4;
		}
		else if (call.fn_pc == MSL_FN_ITEM_80269DC8 || call.fn_pc == MSL_FN_ITEM_80269F14 ||
		         call.fn_pc == MSL_FN_ITFOXLASER_LOGIC94_SHIELDBOUNCED ||
		         call.fn_pc == MSL_FN_ITFOXLASER_LOGIC94_HITSHIELD ||
		         call.fn_pc == MSL_FN_ITEM_8026A8EC)
		{
			item_gobj = call.arg3;
			item_data = ReadEventU32(item_gobj + MSL_GOBJ_USER_DATA_OFF);
		}
		if (item_data != 0)
		{
			EmitLaserShieldReflectEvent(out, "return", call.fn_pc, item_gobj, item_data,
			                            fighter_data, capsule, call.arg3, call.arg4, call.arg5,
			                            call.arg6, call.fpr1, true, PowerPC::ppcState.gpr[3]);
		}
	}

	if (pc != MSL_FN_FTCOLL_80077464 && pc != MSL_FN_FTCOLL_80077688 &&
	    pc != MSL_FN_ITEM_80269DC8 && pc != MSL_FN_ITEM_80269F14 &&
	    pc != MSL_FN_ITFOXLASER_LOGIC94_SHIELDBOUNCED &&
	    pc != MSL_FN_ITFOXLASER_LOGIC94_HITSHIELD && pc != MSL_FN_ITEM_8026A8EC)
		return;

	if (g_laser_shield_reflect_event_depth <
	    sizeof(g_laser_shield_reflect_event_stack) / sizeof(g_laser_shield_reflect_event_stack[0]))
	{
		LaserShieldReflectEventCall& call =
		    g_laser_shield_reflect_event_stack[g_laser_shield_reflect_event_depth++];
		call.fn_pc = pc;
		call.lr = LR;
		call.arg3 = PowerPC::ppcState.gpr[3];
		call.arg4 = PowerPC::ppcState.gpr[4];
		call.arg5 = PowerPC::ppcState.gpr[5];
		call.arg6 = PowerPC::ppcState.gpr[6];
		call.arg7 = PowerPC::ppcState.gpr[7];
		call.fpr1 = PowerPC::ppcState.ps[1][0];
	}

	u32 item_gobj = 0;
	u32 item_data = 0;
	u32 fighter_data = 0;
	u32 capsule = 0;
	if (pc == MSL_FN_FTCOLL_80077464 || pc == MSL_FN_FTCOLL_80077688)
	{
		item_data = PowerPC::ppcState.gpr[3];
		fighter_data = PowerPC::ppcState.gpr[5];
		capsule = PowerPC::ppcState.gpr[4];
	}
	else
	{
		item_gobj = PowerPC::ppcState.gpr[3];
		item_data = ReadEventU32(item_gobj + MSL_GOBJ_USER_DATA_OFF);
	}
	if (item_data == 0)
		return;
	EmitLaserShieldReflectEvent(out, "entry", pc, item_gobj, item_data, fighter_data, capsule,
	                            PowerPC::ppcState.gpr[3], PowerPC::ppcState.gpr[4],
	                            PowerPC::ppcState.gpr[5], PowerPC::ppcState.gpr[6],
	                            PowerPC::ppcState.ps[1][0], false, 0);
}

void EmitThrowLaserEvent(std::ofstream& out, const char* phase, u32 fn_pc, u32 item_gobj,
                         u32 item_data, u32 hitbox, u32 raw_a, u32 raw_b, u32 raw_c)
{
	if (item_data == 0)
		return;
	const u32 kind = ReadEventU32(item_data + MSL_ITEM_KIND_OFF) & 0xFFFF;
	if (!IsThrowLaserKind(kind))
		return;
	out << "{\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
	out << ",\"phase\":\"" << phase << "\"";
	out << ",\"event\":\"" << ThrowLaserEventName(fn_pc) << "\"";
	out << ",\"fn\":\"" << ThrowLaserFnName(fn_pc) << "\"";
	out << ",\"pc\":" << fn_pc;
	out << ",\"lr\":" << LR;
	out << ",\"raw_args\":[" << raw_a << "," << raw_b << "," << raw_c << "]";
	DumpEventItem(out, "item", item_gobj, item_data);
	if (hitbox != 0)
		DumpEventHitbox(out, "hitbox", item_data, hitbox);
	out << "}\n";
}

void EmitThrowLaserSpawnRequest(std::ofstream& out)
{
	const u32 item_kind = PowerPC::ppcState.gpr[5] & 0xFFFFu;
	if (!IsThrowLaserKind(item_kind))
		return;
	out << "{\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
	out << ",\"phase\":\"entry\"";
	out << ",\"event\":\"spawn_request\"";
	out << ",\"fn\":\"it_8029C6CC\"";
	out << ",\"pc\":" << MSL_FN_IT_8029C6CC;
	out << ",\"owner_gobj\":" << PowerPC::ppcState.gpr[3];
	out << ",\"owner_port\":" << OwnerPortFromGobj(PowerPC::ppcState.gpr[3]);
	out << ",\"item_kind\":" << item_kind;
	out << ",\"spawn_pos_ptr\":" << PowerPC::ppcState.gpr[4];
	DumpEventVec(out, "spawn_pos_bits", PowerPC::ppcState.gpr[4]);
	out << ",\"angle_fpr1\":" << PowerPC::ppcState.ps[1][0];
	out << ",\"speed_fpr2\":" << PowerPC::ppcState.ps[2][0];
	out << "}\n";
}

void MaybeCaptureThrowLaserEvents(u32 pc)
{
	static bool initialized = false;
	static bool enabled = false;
	static std::ofstream out;
	if (!initialized)
	{
		initialized = true;
		const char* path = std::getenv("MSL_THROW_LASER_EVENT_PROBE_PATH");
		if (path != nullptr && path[0] != '\0')
		{
			out.open(path, std::ios::out | std::ios::app);
			enabled = out.good();
		}
	}
	if (!enabled)
		return;

	if (g_throw_laser_event_depth > 0 &&
	    pc == g_throw_laser_event_stack[g_throw_laser_event_depth - 1].lr)
	{
		const ThrowLaserEventCall call =
		    g_throw_laser_event_stack[g_throw_laser_event_depth - 1];
		g_throw_laser_event_depth--;
		if (call.fn_pc == MSL_FN_IT_8029C6CC)
		{
			u32 item_gobj = PowerPC::ppcState.gpr[3];
			u32 item_data = ReadEventU32(item_gobj + MSL_GOBJ_USER_DATA_OFF);
			EmitThrowLaserEvent(out, "return", call.fn_pc, item_gobj, item_data, 0, call.arg3,
			                    call.arg4, PowerPC::ppcState.gpr[3]);
		}
		else if (call.fn_pc == MSL_FN_IT_8029C4D4 || call.fn_pc == MSL_FN_ITEM_8026A294)
		{
			u32 item_gobj = call.arg3;
			u32 item_data = ReadEventU32(item_gobj + MSL_GOBJ_USER_DATA_OFF);
			EmitThrowLaserEvent(out, "return", call.fn_pc, item_gobj, item_data, 0, call.arg3,
			                    call.arg4, PowerPC::ppcState.gpr[3]);
		}
		else if (call.fn_pc == MSL_FN_IT_8026FAC4)
		{
			EmitThrowLaserEvent(out, "return", call.fn_pc, 0, call.arg3, call.arg4, call.arg5,
			                    call.arg6, PowerPC::ppcState.gpr[3]);
		}
		else if (call.fn_pc == MSL_FN_IT_80272460)
		{
			u32 item_gobj = call.arg5;
			u32 item_data = ReadEventU32(item_gobj + MSL_GOBJ_USER_DATA_OFF);
			EmitThrowLaserEvent(out, "return", call.fn_pc, item_gobj, item_data, call.arg3,
			                    call.arg3, call.arg4, PowerPC::ppcState.gpr[3]);
		}
	}

	if (pc != MSL_FN_IT_8029C6CC && pc != MSL_FN_IT_8029C4D4 && pc != MSL_FN_IT_8026FAC4 &&
	    pc != MSL_FN_IT_80272460 && pc != MSL_FN_ITEM_8026A294 && pc != MSL_FN_ITEM_8026A8EC)
		return;

	if (g_throw_laser_event_depth < sizeof(g_throw_laser_event_stack) / sizeof(g_throw_laser_event_stack[0]))
	{
		ThrowLaserEventCall& call = g_throw_laser_event_stack[g_throw_laser_event_depth++];
		call.fn_pc = pc;
		call.lr = LR;
		call.arg3 = PowerPC::ppcState.gpr[3];
		call.arg4 = PowerPC::ppcState.gpr[4];
		call.arg5 = PowerPC::ppcState.gpr[5];
		call.arg6 = PowerPC::ppcState.gpr[6];
		call.arg7 = PowerPC::ppcState.gpr[7];
	}

	if (pc == MSL_FN_IT_8029C4D4 || pc == MSL_FN_ITEM_8026A294 || pc == MSL_FN_ITEM_8026A8EC)
	{
		u32 item_gobj = PowerPC::ppcState.gpr[3];
		u32 item_data = ReadEventU32(item_gobj + MSL_GOBJ_USER_DATA_OFF);
		EmitThrowLaserEvent(out, "entry", pc, item_gobj, item_data, 0, PowerPC::ppcState.gpr[3],
		                    PowerPC::ppcState.gpr[4], PowerPC::ppcState.gpr[5]);
	}
	else if (pc == MSL_FN_IT_8026FAC4)
	{
		EmitThrowLaserEvent(out, "entry", pc, 0, PowerPC::ppcState.gpr[3], PowerPC::ppcState.gpr[4],
		                    PowerPC::ppcState.gpr[5], PowerPC::ppcState.gpr[6],
		                    PowerPC::ppcState.gpr[7]);
	}
	else if (pc == MSL_FN_IT_80272460)
	{
		u32 item_gobj = PowerPC::ppcState.gpr[5];
		u32 item_data = ReadEventU32(item_gobj + MSL_GOBJ_USER_DATA_OFF);
		EmitThrowLaserEvent(out, "entry", pc, item_gobj, item_data, PowerPC::ppcState.gpr[3],
		                    PowerPC::ppcState.gpr[3], PowerPC::ppcState.gpr[4],
		                    PowerPC::ppcState.gpr[5]);
	}
	else if (pc == MSL_FN_IT_8029C6CC)
	{
		EmitThrowLaserSpawnRequest(out);
	}
}

// DamageFall IASA event probe: traces ftCo_DamageFall_IASA gate inputs (lstick, lstick1,
// x670) and ftCo_Fall_Enter callers. Enable with MSL_DAMAGEFALL_PROBE_PATH (+ optional
// MSL_DAMAGEFALL_PROBE_FRAME_START/END); requires interpreter mode.
static u32 DamageFallProbeReadU32(u32 addr)
{
	if (addr == 0)
		return 0;
	return Memory::Read_U32(addr);
}

void MaybeCaptureDamageFallIasaProbe(u32 pc)
{
	constexpr u32 MSL_FTCO_DAMAGEFALL_IASA = 0x80090828;
	constexpr u32 MSL_FTCO_FALL_ENTER = 0x800CC730;
	constexpr u32 MSL_FIGHTER_SPAGHETTI = 0x8006AD10;
	if (pc != MSL_FTCO_DAMAGEFALL_IASA && pc != MSL_FTCO_FALL_ENTER && pc != MSL_FIGHTER_SPAGHETTI)
		return;
	static bool initialized = false;
	static bool enabled = false;
	static s32 frame_start = -2147483647;
	static s32 frame_end = 2147483647;
	static std::ofstream out;
	if (!initialized)
	{
		initialized = true;
		const char* path = std::getenv("MSL_DAMAGEFALL_PROBE_PATH");
		if (path != nullptr && path[0] != '\0')
		{
			out.open(path, std::ios::out | std::ios::app);
			enabled = out.good();
		}
		const char* start = std::getenv("MSL_DAMAGEFALL_PROBE_FRAME_START");
		if (start != nullptr && start[0] != '\0')
			frame_start = std::atoi(start);
		const char* end = std::getenv("MSL_DAMAGEFALL_PROBE_FRAME_END");
		if (end != nullptr && end[0] != '\0')
			frame_end = std::atoi(end);
	}
	if (!enabled)
		return;
	const s32 frame = static_cast<s32>(DamageFallProbeReadU32(MSL_FRAME_INDEX_PTR));
	if (frame < frame_start || frame > frame_end)
		return;
	const u32 gobj = PowerPC::ppcState.gpr[3];
	const u32 fp = DamageFallProbeReadU32(gobj + 0x2C);
	const char* fn = pc == MSL_FTCO_DAMAGEFALL_IASA ? "DamageFall_IASA"
	                 : pc == MSL_FTCO_FALL_ENTER    ? "Fall_Enter"
	                                                : "Spaghetti";
	out << "{\"fn\":\"" << fn << "\",\"frame\":" << frame << ",\"lr\":" << PowerPC::ppcState.spr[8]
	    << ",\"fp\":" << fp;
	if (fp != 0)
	{
		out << ",\"action\":" << DamageFallProbeReadU32(fp + 0x10)
		    << ",\"lstick_x_bits\":" << DamageFallProbeReadU32(fp + 0x620)
		    << ",\"lstick1_x_bits\":" << DamageFallProbeReadU32(fp + 0x628)
		    << ",\"x670\":" << (DamageFallProbeReadU32(fp + 0x670) >> 24);
	}
	out << "}\n";
}

void MaybeCaptureCollisionProbe(u32 pc)
{
	static bool initialized = false;
	static bool enabled = false;
	static s32 frame_start = -2147483647;
	static s32 frame_end = 2147483647;
	static std::ofstream out;
	if (!initialized)
	{
		initialized = true;
		const char* path = std::getenv("MSL_COLLISION_PROBE_PATH");
		if (path != nullptr && path[0] != '\0')
		{
			out.open(path, std::ios::out | std::ios::app);
			enabled = out.good();
		}
		const char* start = std::getenv("MSL_COLLISION_PROBE_FRAME_START");
		if (start != nullptr && start[0] != '\0')
			frame_start = std::atoi(start);
		const char* end = std::getenv("MSL_COLLISION_PROBE_FRAME_END");
		if (end != nullptr && end[0] != '\0')
			frame_end = std::atoi(end);
	}
	if (!enabled)
		return;

	const s32 frame = static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
	if (frame < frame_start || frame > frame_end)
		return;

	if (g_collision_probe_depth > 0 && pc == g_collision_probe_stack[g_collision_probe_depth - 1].lr)
	{
		const CollisionProbeCall call = g_collision_probe_stack[g_collision_probe_depth - 1];
		g_collision_probe_depth--;
		const char* fn = call.fn_pc == MSL_LBCOLL_80007BCC    ? "lbColl_80007BCC"
		                 : call.fn_pc == MSL_LBCOLL_80007ECC  ? "lbColl_80007ECC"
		                 : call.fn_pc == MSL_LBCOLL_8000805C  ? "lbColl_8000805C"
		                 : call.fn_pc == MSL_FTCOLL_8007699C ? "ftColl_8007699C"
		                                                     : "ftColl_80076ED8";
		out << "{\"pc\":" << pc << ",\"phase\":\"return\",\"fn\":\"" << fn << "\""
		    << ",\"frame\":" << frame
		    << ",\"lr\":" << call.lr << ",\"return_r3\":" << PowerPC::ppcState.gpr[3];
		if (call.fn_pc == MSL_LBCOLL_80007BCC)
		{
			DumpCollisionHitCapsule(out, "hit", call.hit);
			DumpCollisionHitResult(out, "shield_hit", call.shield_hit);
			out << ",\"matrix_ptr\":" << call.matrix;
			if (call.matrix != 0)
				DumpCollisionMtxBits(out, "matrix_bits", call.matrix);
		}
		else if (call.fn_pc == MSL_LBCOLL_80007ECC)
		{
			DumpCollisionHitCapsule(out, "hit", call.hit);
			DumpCollisionHurtCapsule(out, "hurt", call.hurt);
			out << ",\"matrix_ptr\":" << call.matrix;
			if (call.matrix != 0)
				DumpCollisionMtxBits(out, "matrix_bits", call.matrix);
		}
		else if (call.fn_pc == MSL_LBCOLL_8000805C)
		{
			DumpCollisionHitCapsule(out, "hit", call.hit);
			DumpCollisionHurtCapsule(out, "hurt", call.hurt);
			out << ",\"matrix_ptr\":" << call.matrix;
			if (call.matrix != 0)
				DumpCollisionMtxBits(out, "matrix_bits", call.matrix);
		}
		else if (call.fn_pc == MSL_FTCOLL_8007699C)
		{
			DumpCollisionFighter(out, "fighter0", call.attacker_fp);
			DumpCollisionFighter(out, "fighter1", call.defender_fp);
			DumpCollisionHitCapsule(out, "hit0", call.hit);
			DumpCollisionHitCapsule(out, "hit1", call.hurt);
		}
		else
		{
			DumpCollisionFighter(out, "attacker_fighter", call.attacker_fp);
			DumpCollisionFighter(out, "defender_fighter", call.defender_fp);
			DumpCollisionHitCapsule(out, "hit", call.hit);
			DumpCollisionHurtCapsule(out, "hurt", call.hurt);
		}
		out << "}\n";
	}

	if (pc != MSL_FTCOLL_80076ED8 && pc != MSL_LBCOLL_80007BCC &&
	    pc != MSL_LBCOLL_80007ECC &&
	    pc != MSL_LBCOLL_8000805C && pc != MSL_FTCOLL_8007699C)
		return;

	if (g_collision_probe_depth < sizeof(g_collision_probe_stack) / sizeof(g_collision_probe_stack[0]))
	{
		CollisionProbeCall& call = g_collision_probe_stack[g_collision_probe_depth++];
		call.fn_pc = pc;
		call.lr = LR;
		if (pc == MSL_LBCOLL_80007BCC)
		{
			call.attacker_fp = 0;
			call.hit = PowerPC::ppcState.gpr[3];
			call.defender_fp = 0;
			call.hurt = 0;
			call.shield_hit = PowerPC::ppcState.gpr[4];
			call.matrix = PowerPC::ppcState.gpr[5];
		}
		else if (pc == MSL_LBCOLL_80007ECC)
		{
			call.attacker_fp = 0;
			call.hit = PowerPC::ppcState.gpr[3];
			call.defender_fp = 0;
			call.hurt = PowerPC::ppcState.gpr[4];
			call.shield_hit = 0;
			call.matrix = PowerPC::ppcState.gpr[5];
		}
		else if (pc == MSL_LBCOLL_8000805C)
		{
			call.attacker_fp = 0;
			call.hit = PowerPC::ppcState.gpr[3];
			call.defender_fp = 0;
			call.hurt = PowerPC::ppcState.gpr[4];
			call.shield_hit = 0;
			call.matrix = PowerPC::ppcState.gpr[5];
		}
		else if (pc == MSL_FTCOLL_8007699C)
		{
			call.attacker_fp = PowerPC::ppcState.gpr[3];
			call.hit = PowerPC::ppcState.gpr[4];
			call.defender_fp = PowerPC::ppcState.gpr[5];
			call.hurt = PowerPC::ppcState.gpr[6];
			call.shield_hit = 0;
			call.matrix = 0;
		}
		else
		{
			call.attacker_fp = PowerPC::ppcState.gpr[3];
			call.hit = PowerPC::ppcState.gpr[4];
			call.defender_fp = PowerPC::ppcState.gpr[5];
			call.hurt = PowerPC::ppcState.gpr[6];
			call.shield_hit = 0;
			call.matrix = 0;
		}
	}

	const char* fn = pc == MSL_LBCOLL_80007BCC    ? "lbColl_80007BCC"
	                 : pc == MSL_LBCOLL_80007ECC  ? "lbColl_80007ECC"
	                 : pc == MSL_LBCOLL_8000805C  ? "lbColl_8000805C"
	                 : pc == MSL_FTCOLL_8007699C ? "ftColl_8007699C"
	                                             : "ftColl_80076ED8";
	out << "{\"pc\":" << pc << ",\"phase\":\"entry\",\"fn\":\"" << fn << "\""
	    << ",\"frame\":" << frame
	    << ",\"lr\":" << LR;
	if (pc == MSL_LBCOLL_80007BCC)
	{
		out << ",\"arg3_var_r22\":" << PowerPC::ppcState.gpr[6];
		out << ",\"fpr_bits\":[" << PowerPC::ppcState.ps[1][0] << ","
		    << PowerPC::ppcState.ps[2][0] << "," << PowerPC::ppcState.ps[3][0] << "]";
		DumpCollisionHitCapsule(out, "hit", PowerPC::ppcState.gpr[3]);
		DumpCollisionHitResult(out, "shield_hit", PowerPC::ppcState.gpr[4]);
		out << ",\"matrix_ptr\":" << PowerPC::ppcState.gpr[5];
		if (PowerPC::ppcState.gpr[5] != 0)
			DumpCollisionMtxBits(out, "matrix_bits", PowerPC::ppcState.gpr[5]);
	}
	else if (pc == MSL_LBCOLL_80007ECC)
	{
		out << ",\"hurt_pos_z_bits\":" << PowerPC::ppcState.ps[3][0];
		out << ",\"fpr_bits\":[" << PowerPC::ppcState.ps[1][0] << ","
		    << PowerPC::ppcState.ps[2][0] << "," << PowerPC::ppcState.ps[3][0] << "]";
		DumpCollisionHitCapsule(out, "hit", PowerPC::ppcState.gpr[3]);
		DumpCollisionHurtCapsule(out, "hurt", PowerPC::ppcState.gpr[4]);
		out << ",\"matrix_ptr\":" << PowerPC::ppcState.gpr[5];
		if (PowerPC::ppcState.gpr[5] != 0)
			DumpCollisionMtxBits(out, "matrix_bits", PowerPC::ppcState.gpr[5]);
	}
	else if (pc == MSL_LBCOLL_8000805C)
	{
		out << ",\"arg3_var_r22\":" << PowerPC::ppcState.gpr[6];
		out << ",\"fpr_bits\":[" << PowerPC::ppcState.ps[1][0] << ","
		    << PowerPC::ppcState.ps[2][0] << "," << PowerPC::ppcState.ps[3][0] << "]";
		DumpCollisionHitCapsule(out, "hit", PowerPC::ppcState.gpr[3]);
		DumpCollisionHurtCapsule(out, "hurt", PowerPC::ppcState.gpr[4]);
		out << ",\"matrix_ptr\":" << PowerPC::ppcState.gpr[5];
		if (PowerPC::ppcState.gpr[5] != 0)
			DumpCollisionMtxBits(out, "matrix_bits", PowerPC::ppcState.gpr[5]);
	}
	else if (pc == MSL_FTCOLL_8007699C)
	{
		DumpCollisionFighter(out, "fighter0", PowerPC::ppcState.gpr[3]);
		DumpCollisionFighter(out, "fighter1", PowerPC::ppcState.gpr[5]);
		DumpCollisionHitCapsule(out, "hit0", PowerPC::ppcState.gpr[4]);
		DumpCollisionHitCapsule(out, "hit1", PowerPC::ppcState.gpr[6]);
	}
	else
	{
		DumpCollisionFighter(out, "attacker_fighter", PowerPC::ppcState.gpr[3]);
		DumpCollisionFighter(out, "defender_fighter", PowerPC::ppcState.gpr[5]);
		DumpCollisionHitCapsule(out, "hit", PowerPC::ppcState.gpr[4]);
		DumpCollisionHurtCapsule(out, "hurt", PowerPC::ppcState.gpr[6]);
	}
	out << "}\n";
}

void MaybeCaptureThrowAttachProbe(u32 pc)
{
	static bool initialized = false;
	static bool enabled = false;
	static std::ofstream out;
	if (!initialized)
	{
		initialized = true;
		const char* path = std::getenv("MSL_THROW_ATTACH_PROBE_PATH");
		if (path != nullptr && path[0] != '\0')
		{
			out.open(path, std::ios::out | std::ios::app);
			enabled = out.good();
		}
	}
	if (!enabled)
		return;

	if (g_throw_attach_probe_depth > 0 &&
	    pc == g_throw_attach_probe_stack[g_throw_attach_probe_depth - 1].lr)
	{
		const ThrowAttachProbeCall call =
		    g_throw_attach_probe_stack[g_throw_attach_probe_depth - 1];
		g_throw_attach_probe_depth--;
		const u32 owner_gobj = ReadEventU32(call.fighter_fp + MSL_FIGHTER_VICTIM_GOBJ_OFF);
		out << "{\"pc\":" << pc << ",\"phase\":\"return\",\"fn\":\"ftCo_800DE508\""
		    << ",\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR))
		    << ",\"lr\":" << call.lr << ",\"gobj\":" << call.gobj << ",\"owner_gobj\":"
		    << owner_gobj;
		DumpThrowAttachFighterBrief(out, "fighter", call.fighter_fp);
		DumpThrowAttachFighterBrief(out, "owner_fighter", FighterDataFromGobj(owner_gobj));
		out << "}\n";
	}

	if (pc != MSL_FTCO_800DE508)
		return;

	const u32 gobj = PowerPC::ppcState.gpr[3];
	const u32 fighter_data = FighterDataFromGobj(gobj);
	const u32 owner_gobj = ReadEventU32(fighter_data + MSL_FIGHTER_VICTIM_GOBJ_OFF);
	if (g_throw_attach_probe_depth <
	    sizeof(g_throw_attach_probe_stack) / sizeof(g_throw_attach_probe_stack[0]))
	{
		ThrowAttachProbeCall& call = g_throw_attach_probe_stack[g_throw_attach_probe_depth++];
		call.lr = LR;
		call.gobj = gobj;
		call.fighter_fp = fighter_data;
	}
	out << "{\"pc\":" << pc << ",\"phase\":\"entry\",\"fn\":\"ftCo_800DE508\""
	    << ",\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR))
	    << ",\"lr\":" << LR << ",\"gobj\":" << gobj << ",\"owner_gobj\":" << owner_gobj;
	DumpThrowAttachFighterBrief(out, "fighter", fighter_data);
	DumpThrowAttachFighterBrief(out, "owner_fighter", FighterDataFromGobj(owner_gobj));
	out << "}\n";
}

void DumpDamageSdiFighter(std::ofstream& out, const char* name, u32 fighter_fp)
{
	out << ",\"" << name << "\":{";
	out << "\"ptr\":" << fighter_fp;
	if (fighter_fp != 0)
	{
		out << ",\"action\":" << (ReadEventU32(fighter_fp + MSL_FIGHTER_ACTION_STATE_OFF) & 0xFFFF);
		out << ",\"action_frame_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_ACTION_FRAME_OFF);
		out << ",\"pos_x_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_POS_X_OFF);
		out << ",\"pos_y_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_POS_Y_OFF);
		out << ",\"lstick_x_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK_X_OFF);
		out << ",\"lstick_y_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK_Y_OFF);
		out << ",\"lstick1_x_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK1_X_OFF);
		out << ",\"lstick1_y_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK1_Y_OFF);
		out << ",\"lstick_x\":" << ReadEventF32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK_X_OFF);
		out << ",\"lstick_y\":" << ReadEventF32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK_Y_OFF);
		out << ",\"lstick1_x\":" << ReadEventF32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK1_X_OFF);
		out << ",\"lstick1_y\":" << ReadEventF32(fighter_fp + MSL_FIGHTER_INPUT_LSTICK1_Y_OFF);
		out << ",\"x670\":" << static_cast<u32>(ReadEventU8(fighter_fp + MSL_FIGHTER_X670_TIMER_OFF));
		out << ",\"x671\":" << static_cast<u32>(ReadEventU8(fighter_fp + MSL_FIGHTER_X671_TIMER_OFF));
		out << ",\"state_flags_221a\":" << static_cast<u32>(ReadEventU8(fighter_fp + MSL_FIGHTER_STATE_FLAGS_221A_OFF));
		out << ",\"state_flags_221d\":" << static_cast<u32>(ReadEventU8(fighter_fp + MSL_FIGHTER_STATE_FLAGS_221D_OFF));
		out << ",\"hitlag_bits\":" << ReadEventU32(fighter_fp + MSL_FIGHTER_HITLAG_FRAMES_OFF);
		out << ",\"hitlag\":" << ReadEventF32(fighter_fp + MSL_FIGHTER_HITLAG_FRAMES_OFF);
	}
	out << "}";
}

void MaybeCaptureDamageSdiProbe(u32 pc)
{
	static bool initialized = false;
	static bool enabled = false;
	static std::ofstream out;
	if (!initialized)
	{
		initialized = true;
		const char* path = std::getenv("MSL_DAMAGE_SDI_PROBE_PATH");
		if (path != nullptr && path[0] != '\0')
		{
			out.open(path, std::ios::out | std::ios::app);
			enabled = out.good();
		}
	}
	if (!enabled)
		return;

	if (g_damage_sdi_probe_depth > 0 &&
	    pc == g_damage_sdi_probe_stack[g_damage_sdi_probe_depth - 1].lr)
	{
		const DamageSdiProbeCall call =
		    g_damage_sdi_probe_stack[g_damage_sdi_probe_depth - 1];
		g_damage_sdi_probe_depth--;
		out << "{\"pc\":" << pc << ",\"phase\":\"return\",\"fn\":\"ftCo_Damage_OnEveryHitlag\"";
		out << ",\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
		out << ",\"lr\":" << call.lr << ",\"gobj\":" << call.gobj;
		out << ",\"pre_pos_x_bits\":" << call.pre_pos_x_bits;
		out << ",\"pre_pos_y_bits\":" << call.pre_pos_y_bits;
		out << ",\"pre_x670\":" << static_cast<u32>(call.pre_x670);
		out << ",\"pre_x671\":" << static_cast<u32>(call.pre_x671);
		DumpDamageSdiFighter(out, "fighter", call.fighter_fp);
		out << "}\n";
	}

	if (pc != MSL_FN_FTCO_DAMAGE_ON_EVERY_HITLAG)
		return;

	const u32 gobj = PowerPC::ppcState.gpr[3];
	const u32 fighter_fp = FighterDataFromGobj(gobj);
	if (g_damage_sdi_probe_depth <
	    sizeof(g_damage_sdi_probe_stack) / sizeof(g_damage_sdi_probe_stack[0]))
	{
		DamageSdiProbeCall& call = g_damage_sdi_probe_stack[g_damage_sdi_probe_depth++];
		call.lr = LR;
		call.gobj = gobj;
		call.fighter_fp = fighter_fp;
		call.pre_pos_x_bits = ReadEventU32(fighter_fp + MSL_FIGHTER_POS_X_OFF);
		call.pre_pos_y_bits = ReadEventU32(fighter_fp + MSL_FIGHTER_POS_Y_OFF);
		call.pre_x670 = ReadEventU8(fighter_fp + MSL_FIGHTER_X670_TIMER_OFF);
		call.pre_x671 = ReadEventU8(fighter_fp + MSL_FIGHTER_X671_TIMER_OFF);
	}
	out << "{\"pc\":" << pc << ",\"phase\":\"entry\",\"fn\":\"ftCo_Damage_OnEveryHitlag\"";
	out << ",\"frame\":" << static_cast<s32>(ReadEventU32(MSL_FRAME_INDEX_PTR));
	out << ",\"lr\":" << LR << ",\"gobj\":" << gobj;
	DumpDamageSdiFighter(out, "fighter", fighter_fp);
	out << "}\n";
}
}

bool Interpreter::m_EndBlock;

// function tables
Interpreter::Instruction Interpreter::m_opTable[64];
Interpreter::Instruction Interpreter::m_opTable4[1024];
Interpreter::Instruction Interpreter::m_opTable19[1024];
Interpreter::Instruction Interpreter::m_opTable31[1024];
Interpreter::Instruction Interpreter::m_opTable59[32];
Interpreter::Instruction Interpreter::m_opTable63[1024];

void Interpreter::RunTable4(UGeckoInstruction _inst)  { m_opTable4[_inst.SUBOP10](_inst); }
void Interpreter::RunTable19(UGeckoInstruction _inst) { m_opTable19[_inst.SUBOP10](_inst); }
void Interpreter::RunTable31(UGeckoInstruction _inst) { m_opTable31[_inst.SUBOP10](_inst); }
void Interpreter::RunTable59(UGeckoInstruction _inst) { m_opTable59[_inst.SUBOP5](_inst); }
void Interpreter::RunTable63(UGeckoInstruction _inst) { m_opTable63[_inst.SUBOP10](_inst); }

void Interpreter::Init()
{
	g_bReserve = false;
	m_EndBlock = false;
}

void Interpreter::Shutdown()
{
}

static int startTrace = 0;

static void Trace(UGeckoInstruction& instCode)
{
	std::string regs = "";
	for (int i = 0; i < 32; i++)
	{
		regs += StringFromFormat("r%02d: %08x ", i, PowerPC::ppcState.gpr[i]);
	}

	std::string fregs = "";
	for (int i = 0; i < 32; i++)
	{
		fregs += StringFromFormat("f%02d: %08" PRIx64 " %08" PRIx64 " ", i, PowerPC::ppcState.ps[i][0], PowerPC::ppcState.ps[i][1]);
	}

	std::string ppc_inst = GekkoDisassembler::Disassemble(instCode.hex, PC);
	DEBUG_LOG(POWERPC, "INTER PC: %08x SRR0: %08x SRR1: %08x CRval: %016lx FPSCR: %08x MSR: %08x LR: %08x %s %08x %s", PC, SRR0, SRR1, (unsigned long)PowerPC::ppcState.cr_val[0], PowerPC::ppcState.fpscr, PowerPC::ppcState.msr, PowerPC::ppcState.spr[8], regs.c_str(), instCode.hex, ppc_inst.c_str());
}

int Interpreter::SingleStepInner()
{
	static UGeckoInstruction instCode;
	MaybeCaptureInstanceProbe(PC);
	MaybeCaptureDamageFallIasaProbe(PC);
	MaybeCaptureCollisionProbe(PC);
	MaybeCaptureThrowAttachProbe(PC);
	MaybeCaptureThrowLaserEvents(PC);
	MaybeCaptureLaserShieldReflectEvents(PC);
	MaybeCaptureDamageSdiProbe(PC);
	u32 function = HLE::GetFunctionIndex(PC);
	if (function != 0)
	{
		int type = HLE::GetFunctionTypeByIndex(function);
		if (type == HLE::HLE_HOOK_START || type == HLE::HLE_HOOK_REPLACE)
		{
			int flags = HLE::GetFunctionFlagsByIndex(function);
			if (HLE::IsEnabled(flags))
			{
				HLEFunction(function);
				if (type == HLE::HLE_HOOK_START)
				{
					// Run the original.
					function = 0;
				}
			}
			else
			{
				function = 0;
			}
		}
	}

	if (function == 0)
	{
#ifdef USE_GDBSTUB
		if (gdb_active() && gdb_bp_x(PC))
		{
			Host_UpdateDisasmDialog();

			gdb_signal(SIGTRAP);
			gdb_handle_exception();
		}
#endif

		NPC = PC + sizeof(UGeckoInstruction);
		instCode.hex = PowerPC::Read_Opcode(PC);

		// Uncomment to trace the interpreter
		//if ((PC & 0xffffff)>=0x0ab54c && (PC & 0xffffff)<=0x0ab624)
		//	startTrace = 1;
		//else
		//	startTrace = 0;

		if (startTrace)
		{
			Trace(instCode);
		}

		if (instCode.hex != 0)
		{
			UReg_MSR& msr = (UReg_MSR&)MSR;
			if (msr.FP)  //If FPU is enabled, just execute
			{
				m_opTable[instCode.OPCD](instCode);
				if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
				{
					PowerPC::CheckExceptions();
					m_EndBlock = true;
				}
			}
			else
			{
				// check if we have to generate a FPU unavailable exception
				if (!PPCTables::UsesFPU(instCode))
				{
					m_opTable[instCode.OPCD](instCode);
					if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
					{
						PowerPC::CheckExceptions();
						m_EndBlock = true;
					}
				}
				else
				{
					PowerPC::ppcState.Exceptions |= EXCEPTION_FPU_UNAVAILABLE;
					PowerPC::CheckExceptions();
					m_EndBlock = true;
				}
			}
		}
		else
		{
			// Memory exception on instruction fetch
			PowerPC::CheckExceptions();
			m_EndBlock = true;
		}
	}
	last_pc = PC;
	PC = NPC;

	GekkoOPInfo *opinfo = GetOpInfo(instCode);
	return opinfo->numCycles;
}

void Interpreter::SingleStep()
{
	SingleStepInner();

	CoreTiming::g_slice_length = 1;
	PowerPC::ppcState.downcount = 0;
	CoreTiming::Advance();

	if (PowerPC::ppcState.Exceptions)
	{
		PowerPC::CheckExceptions();
		PC = NPC;
	}
}

//#define SHOW_HISTORY
#ifdef SHOW_HISTORY
std::vector <int> PCVec;
std::vector <int> PCBlockVec;
int ShowBlocks = 30;
int ShowSteps = 300;
#endif

// FastRun - inspired by GCemu (to imitate the JIT so that they can be compared).
void Interpreter::Run()
{
	while (!CPU::GetState())
	{
		//we have to check exceptions at branches apparently (or maybe just rfi?)
		if (SConfig::GetInstance().bEnableDebugging)
		{
#ifdef SHOW_HISTORY
			PCBlockVec.push_back(PC);
			if (PCBlockVec.size() > ShowBlocks)
				PCBlockVec.erase(PCBlockVec.begin());
#endif

			// Debugging friendly version of inner loop. Tries to do the timing as similarly to the
			// JIT as possible. Does not take into account that some instructions take multiple cycles.
			while (PowerPC::ppcState.downcount > 0)
			{
				m_EndBlock = false;
				int i;
				for (i = 0; !m_EndBlock; i++)
				{
#ifdef SHOW_HISTORY
					PCVec.push_back(PC);
					if (PCVec.size() > ShowSteps)
						PCVec.erase(PCVec.begin());
#endif


					//2: check for breakpoint
					if (PowerPC::breakpoints.IsAddressBreakPoint(PC))
					{
#ifdef SHOW_HISTORY
						NOTICE_LOG(POWERPC, "----------------------------");
						NOTICE_LOG(POWERPC, "Blocks:");
						for (int j = 0; j < PCBlockVec.size(); j++)
							NOTICE_LOG(POWERPC, "PC: 0x%08x", PCBlockVec.at(j));
						NOTICE_LOG(POWERPC, "----------------------------");
						NOTICE_LOG(POWERPC, "Steps:");
						for (int j = 0; j < PCVec.size(); j++)
						{
							// Write space
							if (j > 0)
							{
								if (PCVec.at(j) != PCVec.at(j - 1) + 4)
									NOTICE_LOG(POWERPC, "");
							}

							NOTICE_LOG(POWERPC, "PC: 0x%08x", PCVec.at(j));
						}
#endif
						INFO_LOG(POWERPC, "Hit Breakpoint - %08x", PC);
						CPU::Break();
						if (PowerPC::breakpoints.IsTempBreakPoint(PC))
							PowerPC::breakpoints.Remove(PC);

						Host_UpdateDisasmDialog();
						return;
					}
					SingleStepInner();
				}
				PowerPC::ppcState.downcount -= i;
			}
		}
		else
		{
			// "fast" version of inner loop. well, it's not so fast.
			while (PowerPC::ppcState.downcount > 0)
			{
				m_EndBlock = false;

				int cycles = 0;
				while (!m_EndBlock)
				{
					cycles += SingleStepInner();
				}
				PowerPC::ppcState.downcount -= cycles;
			}
		}

		CoreTiming::Advance();
	}
}

void Interpreter::unknown_instruction(UGeckoInstruction _inst)
{
	std::string disasm = GekkoDisassembler::Disassemble(PowerPC::HostRead_U32(last_pc), last_pc);
	NOTICE_LOG(POWERPC, "Last PC = %08x : %s", last_pc, disasm.c_str());
	Dolphin_Debugger::PrintCallstack();
	NOTICE_LOG(POWERPC, "\nIntCPU: Unknown instruction %08x at PC = %08x  last_PC = %08x  LR = %08x\n", _inst.hex, PC, last_pc, LR);
	for (int i = 0; i < 32; i += 4)
		NOTICE_LOG(POWERPC, "r%d: 0x%08x r%d: 0x%08x r%d:0x%08x r%d: 0x%08x",
			i, rGPR[i],
			i + 1, rGPR[i + 1],
			i + 2, rGPR[i + 2],
			i + 3, rGPR[i + 3]);

  std::string msg;
	
  msg.append(StringFromFormat("\nIntCPU: Unknown instruction %08x at PC = %08x  last_PC = %08x  LR = %08x\n\n", _inst.hex, PC, last_pc, LR));
  
  std::vector<Dolphin_Debugger::CallstackEntry> callstack;
  Dolphin_Debugger::GetCallstack(callstack);

  for (auto it = callstack.begin(); it != callstack.end(); ++it)
  {
    msg.append(it->Name);
  }

  _assert_msg_(POWERPC, 0, "%s", msg.c_str());
}

void Interpreter::ClearCache()
{
	// Do nothing.
}

const char *Interpreter::GetName()
{
#ifdef _ARCH_64
	return "Interpreter64";
#else
	return "Interpreter32";
#endif
}

Interpreter *Interpreter::getInstance()
{
	static Interpreter instance;
	return &instance;
}
