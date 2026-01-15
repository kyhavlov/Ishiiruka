#include "Core/Slippi/EngineDumpWriter.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_map>

#include "Common/FileUtil.h"
#include "Core/HW/Memmap.h"

namespace {
constexpr u8 ENGINE_DUMP_MAGIC[8] = {'M', 'S', 'I', 'M', 'D', 'M', 'P', 0};
constexpr u32 ENGINE_DUMP_ENDIAN_TAG = 0x01020304;
constexpr u32 ENGINE_DUMP_VERSION = 6;

constexpr u32 R13_BASE = 0x804DB6A0;
constexpr u32 FRAME_INDEX_PTR = R13_BASE - 0x49AC;
constexpr u32 PLAYER_SLOTS = 0x80453080;
constexpr u32 STATIC_PLAYER_SIZE = 0xE90;
constexpr u32 GOBJ_USER_DATA_OFF = 0x2C;
constexpr u32 FIGHTER_POS_X_OFF = 0xB0;
constexpr u32 FIGHTER_POS_Y_OFF = 0xB4;
constexpr u32 FIGHTER_POS_Z_OFF = 0xB8;
constexpr u32 FIGHTER_SELF_VEL_X_OFF = 0x80;
constexpr u32 FIGHTER_SELF_VEL_Y_OFF = 0x84;
constexpr u32 FIGHTER_GR_VEL_OFF = 0xEC;
constexpr u32 FIGHTER_ACTION_OFF = 0x10;
constexpr u32 FIGHTER_ANIM_OFF = 0x14;
constexpr u32 FIGHTER_ACTION_FRAME_OFF = 0x894;
constexpr u32 FIGHTER_ANIM_FRAME_OFF = 0x8A8;
constexpr u32 FIGHTER_GROUND_OR_AIR_OFF = 0xE0;
constexpr u32 FIGHTER_FACING_OFF = 0x2C;
constexpr u32 FIGHTER_PERCENT_OFF = 0x1830;
constexpr u32 FIGHTER_TEAM_OFF = 0x61B;
constexpr u32 FIGHTER_COSTUME_OFF = 0x619;
constexpr u32 FIGHTER_STATE_FLAGS_2218_OFF = 0x2218;
constexpr u32 FIGHTER_STATE_FLAGS_221A_OFF = 0x221A;
constexpr u32 FIGHTER_STATE_FLAGS_221B_OFF = 0x221B;
constexpr u32 FIGHTER_STATE_FLAGS_221C_OFF = 0x221C;
constexpr u32 FIGHTER_STATE_FLAGS_221F_OFF = 0x221F;
constexpr u32 FIGHTER_HITLAG_OFF = 0x195C;
constexpr u32 FIGHTER_MISC_AS_OFF = 0x2340;
constexpr u32 FIGHTER_SHIELD_HEALTH_OFF = 0x1998;
constexpr u32 FIGHTER_HURTBOX_COLLISION1_OFF = 0x1988;
constexpr u32 FIGHTER_HURTBOX_COLLISION2_OFF = 0x198C;
constexpr u32 FIGHTER_ECB_TOP_OFF = 0x794;
constexpr u32 FIGHTER_ECB_BOTTOM_OFF = 0x79C;
constexpr u32 FIGHTER_ECB_RIGHT_OFF = 0x7A4;
constexpr u32 FIGHTER_ECB_LEFT_OFF = 0x7AC;
constexpr u32 FIGHTER_FLOOR_NORMAL_X_OFF = 0x844;
constexpr u32 FIGHTER_FLOOR_NORMAL_Y_OFF = 0x848;
constexpr u32 FIGHTER_GROUND_ACCEL_1_OFF = 0xE4;
constexpr u32 FIGHTER_GROUND_ACCEL_2_OFF = 0xE8;
constexpr u32 FIGHTER_ANIM_VEL_X_OFF = 0x74;
constexpr u32 FIGHTER_ANIM_VEL_Y_OFF = 0x78;
constexpr u32 FIGHTER_HITBOX_BASE_OFF = 0x914;
constexpr u32 HITBOX_STRIDE = 0x138;
constexpr u32 FIGHTER_HURTBOX_BASE_OFF = 0x11A0;
constexpr u32 HURTBOX_STRIDE = 0x4C;
constexpr u32 ITEM_MANAGER_PTR = R13_BASE - 0x3E74;
constexpr u32 ITEM_MANAGER_FIRST_GOBJ_OFF = 0x24;
constexpr u32 GOBJ_NEXT_OFF = 0x08;
constexpr u32 ITEM_DATA_OFF = 0x2C;
constexpr u32 ITEM_KIND_OFF = 0x10;
constexpr u32 ITEM_STATE_OFF = 0x24;
constexpr u32 ITEM_ANIM_ID_OFF = 0x28;
constexpr u32 ITEM_FACING_OFF = 0x2C;
constexpr u32 ITEM_POS_OFF = 0x4C;
constexpr u32 ITEM_VEL_OFF = 0x40;
constexpr u32 ITEM_ANIM_FRAME_OFF = 0x5CC;
constexpr u32 ITEM_LIFETIME_OFF = 0xD44;
constexpr u32 ITEM_HITBOX0_OFF = 0x5D4;
constexpr u32 ITEM_OWNER_OFF = 0x518;
constexpr u32 MAX_ITEMS = 15;
constexpr u32 RNG_STATE_ADDR = 0x804D5F90;
constexpr u32 RNG_MULTIPLIER = 0x343FD;
constexpr u32 RNG_INCREMENT = 0x269EC3;
constexpr u16 FRAME_FLAG_RNG_SEED_EXISTS = 1u << 0;
constexpr u32 RNG_STEPS_UNKNOWN = 0xFFFFFFFFu;
constexpr u32 RNG_STEP_SEARCH_MAX = 8192;
constexpr u32 GAME_TIMER_ADDR = 0x8046B6C8;
constexpr u32 TEAMS_FLAG_ADDR = 0x804807C8;
constexpr u32 P1_STOCK_ADDR = 0x8045310E;
constexpr u32 P2_STOCK_ADDR = 0x80453F9E;

constexpr u32 BUTTON_A = 0x0100;
constexpr u32 BUTTON_B = 0x0200;
constexpr u32 BUTTON_X = 0x0400;
constexpr u32 BUTTON_Y = 0x0800;
constexpr u32 BUTTON_Z = 0x0010;
constexpr u32 BUTTON_L = 0x0040;
constexpr u32 BUTTON_R = 0x0020;
constexpr u32 BUTTON_START = 0x1000;
constexpr u32 BUTTON_D_UP = 0x0008;
constexpr u32 BUTTON_LR = 1u << 31;

inline u32 ReadU32(u32 addr)
{
	return Memory::Read_U32(addr);
}

inline u8 ReadU8(u32 addr)
{
	return Memory::Read_U8(addr);
}

inline s32 ReadS32(u32 addr)
{
	return static_cast<s32>(Memory::Read_U32(addr));
}

inline u32 FloatBits(float v)
{
	u32 bits = 0;
	std::memcpy(&bits, &v, sizeof(bits));
	return bits;
}

inline u32 AdvanceRng(u32 state)
{
	return state * RNG_MULTIPLIER + RNG_INCREMENT;
}

inline u32 RngStepsBetween(u32 start, u32 target, u32 max_steps)
{
	u32 state = start;
	if (state == target)
		return 0;
	for (u32 step = 1; step <= max_steps; step++)
	{
		state = AdvanceRng(state);
		if (state == target)
			return step;
	}
	return RNG_STEPS_UNKNOWN;
}

inline void AppendU8(std::vector<u8>& out, u8 v)
{
	out.push_back(v);
}

inline void AppendU16(std::vector<u8>& out, u16 v)
{
	out.push_back(static_cast<u8>(v & 0xFF));
	out.push_back(static_cast<u8>((v >> 8) & 0xFF));
}

inline void AppendU32(std::vector<u8>& out, u32 v)
{
	out.push_back(static_cast<u8>(v & 0xFF));
	out.push_back(static_cast<u8>((v >> 8) & 0xFF));
	out.push_back(static_cast<u8>((v >> 16) & 0xFF));
	out.push_back(static_cast<u8>((v >> 24) & 0xFF));
}

inline void AppendI32(std::vector<u8>& out, s32 v)
{
	AppendU32(out, static_cast<u32>(v));
}

inline u32 FighterGobjForPort(int port)
{
	u32 base = PLAYER_SLOTS + static_cast<u32>(port - 1) * STATIC_PLAYER_SIZE;
	u8 transformed = ReadU8(base + 0x0C);
	return ReadU32(base + 0xB0 + static_cast<u32>(transformed) * 4);
}

inline u32 FighterPtrForPort(int port)
{
	u32 gobj = FighterGobjForPort(port);
	if (!gobj)
		return 0;
	return ReadU32(gobj + GOBJ_USER_DATA_OFF);
}
} // namespace

EngineDumpWriter::EngineDumpWriter(const std::string& path, int start_frame, int end_frame)
    : m_path(path), m_start_frame(start_frame), m_end_frame(end_frame)
{
}

EngineDumpWriter::~EngineDumpWriter()
{
	Finalize();
}

void EngineDumpWriter::SetGameSettings(const Slippi::GameSettings& settings)
{
	m_stage_id = settings.stage;
	m_port_count = static_cast<u8>(settings.players.size());
	if (m_port_count == 0)
		m_port_count = 2;
}

void EngineDumpWriter::CaptureFrame(s32 frame_index, Slippi::FrameData* frame)
{
	if (m_path.empty() || frame == nullptr)
		return;
	if (frame_index < m_start_frame || frame_index > m_end_frame)
		return;

	u32 fp1 = FighterPtrForPort(1);
	u32 fp2 = FighterPtrForPort(2);
	if (!fp1 || !fp2)
		return;

	if (!m_started)
	{
		m_started = true;
		m_last_frame = frame_index - 1;
	}

	if (m_last_frame != INT_MIN && frame_index != m_last_frame + 1)
	{
		return;
	}

	m_last_frame = frame_index;

	m_is_teams = ReadU8(TEAMS_FLAG_ADDR);

	u32 rng_state = ReadU32(RNG_STATE_ADDR);
	u16 frame_flags = 0;
	u32 rng_seed = 0;
	if (frame->randomSeedExists)
	{
		frame_flags |= FRAME_FLAG_RNG_SEED_EXISTS;
		rng_seed = *(u32 *)&frame->randomSeed;
	}
	u32 rng_steps = m_last_rng_seed_valid
	                    ? RngStepsBetween(m_last_rng_seed, rng_state, RNG_STEP_SEARCH_MAX)
	                    : RNG_STEPS_UNKNOWN;
	u32 game_timer = ReadU32(GAME_TIMER_ADDR);

	FrameRecord fr = {};
	fr.frame_index = frame_index;
	fr.rng_state = rng_state;
	fr.rng_seed = rng_seed;
	fr.rng_steps = rng_steps;
	fr.item_offset = static_cast<u32>(m_items.size());
	fr.item_count = 0;
	fr.flags = frame_flags;
	fr.game_timer = game_timer;
	fr.randall_exists = 0;
	fr.randall_x_bits = 0;
	fr.randall_y_bits = 0;
	fr.fountain0_exists = 0;
	fr.fountain0_y_bits = 0;
	fr.fountain1_exists = 0;
	fr.fountain1_y_bits = 0;

	auto player_for_port = [frame](int port) -> const Slippi::PlayerFrameData* {
		auto it = frame->players.find(static_cast<u8>(port - 1));
		if (it == frame->players.end())
			return nullptr;
		return &it->second;
	};

	for (int port = 1; port <= 2; port++)
	{
		const Slippi::PlayerFrameData* pdata = player_for_port(port);
		InputRecord in = {};
		if (pdata)
		{
			u32 mask = pdata->buttons &
			           (BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y | BUTTON_Z | BUTTON_L | BUTTON_R |
			            BUTTON_START | BUTTON_D_UP);
			float l_trig = pdata->lTrigger;
			float r_trig = pdata->rTrigger;
			if (l_trig == 0.0f && r_trig == 0.0f)
			{
				l_trig = pdata->trigger;
				r_trig = pdata->trigger;
			}
			if ((mask & (BUTTON_L | BUTTON_R)) || l_trig > 0.0f || r_trig > 0.0f)
				mask |= BUTTON_LR;

			in.buttons = mask;
			in.stick_x_bits = FloatBits(pdata->joystickX);
			in.stick_y_bits = FloatBits(pdata->joystickY);
			in.cstick_x_bits = FloatBits(pdata->cstickX);
			in.cstick_y_bits = FloatBits(pdata->cstickY);
			in.l_shoulder_bits = FloatBits(l_trig);
			in.r_shoulder_bits = FloatBits(r_trig);
			in.raw_stick_x = pdata->joystickXRaw;
			in.raw_stick_y = pdata->joystickYRaw;
			in.raw_cstick_x = pdata->cstickXRaw;
			in.raw_cstick_y = pdata->cstickYRaw;
		}
		m_inputs.push_back(in);
	}

	auto add_fighter = [this](u32 fp_ptr, int port) {
		FighterRecord f = {};
		f.flags = 0;
		f.pos_x_bits = ReadU32(fp_ptr + FIGHTER_POS_X_OFF);
		f.pos_y_bits = ReadU32(fp_ptr + FIGHTER_POS_Y_OFF);
		f.pos_z_bits = ReadU32(fp_ptr + FIGHTER_POS_Z_OFF);
		f.self_vel_x_bits = ReadU32(fp_ptr + FIGHTER_SELF_VEL_X_OFF);
		f.self_vel_y_bits = ReadU32(fp_ptr + FIGHTER_SELF_VEL_Y_OFF);
		f.gr_vel_bits = ReadU32(fp_ptr + FIGHTER_GR_VEL_OFF);
		f.action_state = static_cast<u16>(ReadU32(fp_ptr + FIGHTER_ACTION_OFF) & 0xFFFF);
		f.anim_id = static_cast<u16>(ReadU32(fp_ptr + FIGHTER_ANIM_OFF) & 0xFFFF);
		f.anim_frame_bits = ReadU32(fp_ptr + FIGHTER_ANIM_FRAME_OFF);
		f.action_frame_bits = ReadU32(fp_ptr + FIGHTER_ACTION_FRAME_OFF);
		f.state_flags_2218 = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_2218_OFF);
		f.state_flags_221a = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221A_OFF);
		f.state_flags_221b = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221B_OFF);
		f.state_flags_221c = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221C_OFF);
		f.state_flags_221f = ReadU8(fp_ptr + FIGHTER_STATE_FLAGS_221F_OFF);
		u32 collision = ReadU32(fp_ptr + FIGHTER_HURTBOX_COLLISION1_OFF);
		if (collision == 0)
			collision = ReadU32(fp_ptr + FIGHTER_HURTBOX_COLLISION2_OFF);
		f.invulnerable = static_cast<u8>(collision & 0xFF);
		// ground_or_air is a 32-bit enum; reading a single byte would grab the MSB (0) on big-endian.
		f.ground_or_air = static_cast<u8>(ReadU32(fp_ptr + FIGHTER_GROUND_OR_AIR_OFF) & 0xFF);
		f.stocks = (port == 1) ? ReadU8(P1_STOCK_ADDR) : ReadU8(P2_STOCK_ADDR);
		f.team = ReadU8(fp_ptr + FIGHTER_TEAM_OFF);
		f.costume_id = ReadU8(fp_ptr + FIGHTER_COSTUME_OFF);
		f.facing_bits = ReadU32(fp_ptr + FIGHTER_FACING_OFF);
		f.percent_bits = ReadU32(fp_ptr + FIGHTER_PERCENT_OFF);
		f.hitlag_left_bits = ReadU32(fp_ptr + FIGHTER_HITLAG_OFF);
		f.misc_as_bits = ReadU32(fp_ptr + FIGHTER_MISC_AS_OFF);
		f.shield_health_bits = ReadU32(fp_ptr + FIGHTER_SHIELD_HEALTH_OFF);
		f.ecb_top_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_TOP_OFF + 0x00);
		f.ecb_top_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_TOP_OFF + 0x04);
		f.ecb_bottom_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_BOTTOM_OFF + 0x00);
		f.ecb_bottom_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_BOTTOM_OFF + 0x04);
		f.ecb_left_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_LEFT_OFF + 0x00);
		f.ecb_left_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_LEFT_OFF + 0x04);
		f.ecb_right_x_bits = ReadU32(fp_ptr + FIGHTER_ECB_RIGHT_OFF + 0x00);
		f.ecb_right_y_bits = ReadU32(fp_ptr + FIGHTER_ECB_RIGHT_OFF + 0x04);
		f.floor_normal_x_bits = ReadU32(fp_ptr + FIGHTER_FLOOR_NORMAL_X_OFF);
		f.floor_normal_y_bits = ReadU32(fp_ptr + FIGHTER_FLOOR_NORMAL_Y_OFF);
		f.ground_accel_1_bits = ReadU32(fp_ptr + FIGHTER_GROUND_ACCEL_1_OFF);
		f.ground_accel_2_bits = ReadU32(fp_ptr + FIGHTER_GROUND_ACCEL_2_OFF);
		f.anim_vel_x_bits = ReadU32(fp_ptr + FIGHTER_ANIM_VEL_X_OFF);
		f.anim_vel_y_bits = ReadU32(fp_ptr + FIGHTER_ANIM_VEL_Y_OFF);
		m_fighters.push_back(f);
	};

	add_fighter(fp1, 1);
	add_fighter(fp2, 2);

	for (int port = 1; port <= 2; port++)
	{
		u32 fp_ptr = (port == 1) ? fp1 : fp2;
		for (u32 idx = 0; idx < 15; idx++)
		{
			u32 base = fp_ptr + FIGHTER_HURTBOX_BASE_OFF + idx * HURTBOX_STRIDE;
			HurtboxRecord hb = {};
			hb.state = ReadU32(base + 0x00);
			hb.a_offset_x_bits = ReadU32(base + 0x04);
			hb.a_offset_y_bits = ReadU32(base + 0x08);
			hb.a_offset_z_bits = ReadU32(base + 0x0C);
			hb.b_offset_x_bits = ReadU32(base + 0x10);
			hb.b_offset_y_bits = ReadU32(base + 0x14);
			hb.b_offset_z_bits = ReadU32(base + 0x18);
			hb.scale_bits = ReadU32(base + 0x1C);
			hb.flags = ReadU8(base + 0x24);
			hb.a_pos_x_bits = ReadU32(base + 0x28);
			hb.a_pos_y_bits = ReadU32(base + 0x2C);
			hb.a_pos_z_bits = ReadU32(base + 0x30);
			hb.b_pos_x_bits = ReadU32(base + 0x34);
			hb.b_pos_y_bits = ReadU32(base + 0x38);
			hb.b_pos_z_bits = ReadU32(base + 0x3C);
			hb.bone_index = static_cast<s32>(ReadU32(base + 0x40));
			hb.height = ReadU32(base + 0x44);
			// `FighterHurtCapsule::is_grabbable` is written as a 32-bit value in the retail binary
			// (`ftColl_HurtboxInit` uses `stw r7, 0x48(r4)`), so reading a single byte would grab
			// the MSB (0) on big-endian for `0x00000001`. Use the low byte.
			hb.is_grabbable = static_cast<u8>(ReadU32(base + 0x48) & 0xFF);
			m_hurtboxes.push_back(hb);
		}
	}

	for (int port = 1; port <= 2; port++)
	{
		u32 fp_ptr = (port == 1) ? fp1 : fp2;
		for (u32 idx = 0; idx < 4; idx++)
		{
			u32 base = fp_ptr + FIGHTER_HITBOX_BASE_OFF + idx * HITBOX_STRIDE;
			HitboxRecord hb = {};
			hb.state = ReadU32(base + 0x00);
			hb.group = ReadU32(base + 0x04);
			hb.damage = ReadU32(base + 0x08);
			hb.damage_stale_bits = ReadU32(base + 0x0C);
			hb.offset_x_bits = ReadU32(base + 0x18);
			hb.offset_y_bits = ReadU32(base + 0x14);
			hb.offset_z_bits = ReadU32(base + 0x10);
			hb.size_bits = ReadU32(base + 0x1C);
			hb.angle = ReadU32(base + 0x20);
			hb.kbg = ReadU32(base + 0x24);
			hb.wsk = ReadU32(base + 0x28);
			hb.bkb = ReadU32(base + 0x2C);
			hb.element = ReadU32(base + 0x30);
			hb.shield_damage = ReadU32(base + 0x34);
			hb.sfx = ReadU32(base + 0x38);
			hb.sfx_kind = ReadU32(base + 0x3C);
			for (u32 i = 0; i < 8; i++)
				hb.flags[i] = ReadU8(base + 0x40 + i);
			hb.bone_ptr = ReadU32(base + 0x48);
			hb.pos_x_bits = ReadU32(base + 0x54);
			hb.pos_y_bits = ReadU32(base + 0x50);
			hb.pos_z_bits = ReadU32(base + 0x4C);
			m_hitboxes.push_back(hb);
		}
	}

	std::unordered_map<u32, int> owner_map;
	owner_map[FighterGobjForPort(1)] = 1;
	owner_map[FighterGobjForPort(2)] = 2;

	u32 manager_ptr = ReadU32(ITEM_MANAGER_PTR);
	u32 item_gobj = manager_ptr ? ReadU32(manager_ptr + ITEM_MANAGER_FIRST_GOBJ_OFF) : 0;
	u32 count = 0;
	while (item_gobj != 0 && count < MAX_ITEMS)
	{
		u32 item_data = ReadU32(item_gobj + ITEM_DATA_OFF);
		if (item_data != 0)
		{
			ItemRecord it = {};
			it.item_id = item_data;
			it.kind = static_cast<u16>(ReadU32(item_data + ITEM_KIND_OFF) & 0xFFFF);
			it.state = static_cast<u16>(ReadU32(item_data + ITEM_STATE_OFF) & 0xFFFF);
			u32 owner_gobj = ReadU32(item_data + ITEM_OWNER_OFF);
			auto found = owner_map.find(owner_gobj);
			it.owner_port = found == owner_map.end() ? -1 : static_cast<int8_t>(found->second);
			it.flags = 0;
			it.pos_x_bits = ReadU32(item_data + ITEM_POS_OFF + 0x00);
			it.pos_y_bits = ReadU32(item_data + ITEM_POS_OFF + 0x04);
			it.pos_z_bits = ReadU32(item_data + ITEM_POS_OFF + 0x08);
			it.vel_x_bits = ReadU32(item_data + ITEM_VEL_OFF + 0x00);
			it.vel_y_bits = ReadU32(item_data + ITEM_VEL_OFF + 0x04);
			it.vel_z_bits = ReadU32(item_data + ITEM_VEL_OFF + 0x08);
			it.facing_bits = ReadU32(item_data + ITEM_FACING_OFF);
			it.anim_id = static_cast<u16>(ReadU32(item_data + ITEM_ANIM_ID_OFF) & 0xFFFF);
			it.anim_frame_bits = ReadU32(item_data + ITEM_ANIM_FRAME_OFF);
			it.lifetime_bits = ReadU32(item_data + ITEM_LIFETIME_OFF);
			it.damage = ReadU32(item_data + ITEM_HITBOX0_OFF + 0x08);
			m_items.push_back(it);
			fr.item_count += 1;
		}
		item_gobj = ReadU32(item_gobj + GOBJ_NEXT_OFF);
		count += 1;
	}

	m_frames.push_back(fr);
	if (frame_flags & FRAME_FLAG_RNG_SEED_EXISTS)
	{
		m_last_rng_seed = rng_seed;
		m_last_rng_seed_valid = true;
	}

	if (frame_index >= m_end_frame)
		Finalize();
}

void EngineDumpWriter::Finalize()
{
	if (m_path.empty() || m_frames.empty())
		return;

	const u32 frame_count = static_cast<u32>(m_frames.size());
	const u32 port_count = m_port_count;
	const u32 total_items = static_cast<u32>(m_items.size());

	const u32 frame_rec_size = 48;
	const u32 input_rec_size = 32;
	const u32 fighter_rec_size = 128;
	const u32 item_rec_size = 54;
	const u32 hitbox_rec_size = 88;
	const u32 hurtbox_rec_size = 68;

	const u32 frames_offset = 80;
	const u32 inputs_offset = frames_offset + frame_count * frame_rec_size;
	const u32 fighters_offset = inputs_offset + frame_count * port_count * input_rec_size;
	const u32 items_offset = fighters_offset + frame_count * port_count * fighter_rec_size;
	const u32 hitboxes_offset = items_offset + total_items * item_rec_size;
	const u32 hurtboxes_offset = hitboxes_offset + frame_count * port_count * 4 * hitbox_rec_size;

	std::vector<u8> out;
	out.reserve(hurtboxes_offset + frame_count * port_count * 15 * hurtbox_rec_size);

	for (u32 i = 0; i < 8; i++)
		AppendU8(out, ENGINE_DUMP_MAGIC[i]);
	AppendU32(out, ENGINE_DUMP_VERSION);
	AppendU32(out, ENGINE_DUMP_ENDIAN_TAG);
	AppendU32(out, frame_count);
	AppendU8(out, static_cast<u8>(port_count));
	AppendU16(out, m_stage_id);
	AppendU8(out, m_is_teams);
	AppendU8(out, 0);
	for (u32 i = 0; i < 16; i++)
		AppendU8(out, 0);
	AppendU32(out, frames_offset);
	AppendU32(out, inputs_offset);
	AppendU32(out, fighters_offset);
	AppendU32(out, items_offset);
	AppendU32(out, hitboxes_offset);
	AppendU32(out, hurtboxes_offset);
	AppendU32(out, total_items);
	for (u32 i = 0; i < 11; i++)
		AppendU8(out, 0);
	while (out.size() < frames_offset)
		out.push_back(0);

	for (const auto& fr : m_frames)
	{
		AppendI32(out, fr.frame_index);
		AppendU32(out, fr.rng_state);
		AppendU32(out, fr.rng_seed);
		AppendU32(out, fr.rng_steps);
		AppendU16(out, fr.item_count);
		AppendU32(out, fr.item_offset);
		AppendU16(out, fr.flags);
		AppendU32(out, fr.game_timer);
		AppendU8(out, fr.randall_exists);
		AppendU32(out, fr.randall_x_bits);
		AppendU32(out, fr.randall_y_bits);
		AppendU8(out, fr.fountain0_exists);
		AppendU32(out, fr.fountain0_y_bits);
		AppendU8(out, fr.fountain1_exists);
		AppendU32(out, fr.fountain1_y_bits);
		AppendU8(out, 0);
	}

	for (const auto& in : m_inputs)
	{
		AppendU32(out, in.buttons);
		AppendU32(out, in.stick_x_bits);
		AppendU32(out, in.stick_y_bits);
		AppendU32(out, in.cstick_x_bits);
		AppendU32(out, in.cstick_y_bits);
		AppendU32(out, in.l_shoulder_bits);
		AppendU32(out, in.r_shoulder_bits);
		AppendU8(out, in.raw_stick_x);
		AppendU8(out, in.raw_stick_y);
		AppendU8(out, in.raw_cstick_x);
		AppendU8(out, in.raw_cstick_y);
	}

	for (const auto& f : m_fighters)
	{
		AppendU32(out, f.flags);
		AppendU32(out, f.pos_x_bits);
		AppendU32(out, f.pos_y_bits);
		AppendU32(out, f.pos_z_bits);
		AppendU32(out, f.self_vel_x_bits);
		AppendU32(out, f.self_vel_y_bits);
		AppendU32(out, f.gr_vel_bits);
		AppendU16(out, f.action_state);
		AppendU16(out, f.anim_id);
		AppendU32(out, f.anim_frame_bits);
		AppendU32(out, f.action_frame_bits);
		AppendU8(out, f.state_flags_2218);
		AppendU8(out, f.state_flags_221a);
		AppendU8(out, f.state_flags_221b);
		AppendU8(out, f.state_flags_221c);
		AppendU8(out, f.state_flags_221f);
		AppendU8(out, f.invulnerable);
		AppendU8(out, f.ground_or_air);
		AppendU8(out, f.stocks);
		AppendU8(out, f.team);
		AppendU8(out, f.costume_id);
		AppendU32(out, f.facing_bits);
		AppendU32(out, f.percent_bits);
		AppendU32(out, f.hitlag_left_bits);
		AppendU32(out, f.misc_as_bits);
		AppendU32(out, f.shield_health_bits);
		AppendU32(out, f.ecb_top_x_bits);
		AppendU32(out, f.ecb_top_y_bits);
		AppendU32(out, f.ecb_bottom_x_bits);
		AppendU32(out, f.ecb_bottom_y_bits);
		AppendU32(out, f.ecb_left_x_bits);
		AppendU32(out, f.ecb_left_y_bits);
		AppendU32(out, f.ecb_right_x_bits);
		AppendU32(out, f.ecb_right_y_bits);
		AppendU32(out, f.floor_normal_x_bits);
		AppendU32(out, f.floor_normal_y_bits);
		AppendU32(out, f.ground_accel_1_bits);
		AppendU32(out, f.ground_accel_2_bits);
		AppendU32(out, f.anim_vel_x_bits);
		AppendU32(out, f.anim_vel_y_bits);
		AppendU16(out, 0);
	}

	for (const auto& it : m_items)
	{
		AppendU32(out, it.item_id);
		AppendU16(out, it.kind);
		AppendU16(out, it.state);
		AppendU8(out, static_cast<u8>(it.owner_port));
		AppendU8(out, it.flags);
		AppendU32(out, it.pos_x_bits);
		AppendU32(out, it.pos_y_bits);
		AppendU32(out, it.pos_z_bits);
		AppendU32(out, it.vel_x_bits);
		AppendU32(out, it.vel_y_bits);
		AppendU32(out, it.vel_z_bits);
		AppendU32(out, it.facing_bits);
		AppendU16(out, it.anim_id);
		AppendU16(out, 0);
		AppendU32(out, it.anim_frame_bits);
		AppendU32(out, it.lifetime_bits);
		AppendU32(out, it.damage);
	}

	for (const auto& hb : m_hitboxes)
	{
		AppendU32(out, hb.state);
		AppendU32(out, hb.group);
		AppendU32(out, hb.damage);
		AppendU32(out, hb.damage_stale_bits);
		AppendU32(out, hb.offset_x_bits);
		AppendU32(out, hb.offset_y_bits);
		AppendU32(out, hb.offset_z_bits);
		AppendU32(out, hb.size_bits);
		AppendU32(out, hb.angle);
		AppendU32(out, hb.kbg);
		AppendU32(out, hb.wsk);
		AppendU32(out, hb.bkb);
		AppendU32(out, hb.element);
		AppendU32(out, hb.shield_damage);
		AppendU32(out, hb.sfx);
		AppendU32(out, hb.sfx_kind);
		for (u32 i = 0; i < 8; i++)
			AppendU8(out, hb.flags[i]);
		AppendU32(out, hb.bone_ptr);
		AppendU32(out, hb.pos_x_bits);
		AppendU32(out, hb.pos_y_bits);
		AppendU32(out, hb.pos_z_bits);
	}

	for (const auto& hb : m_hurtboxes)
	{
		AppendU32(out, hb.state);
		AppendU32(out, hb.a_offset_x_bits);
		AppendU32(out, hb.a_offset_y_bits);
		AppendU32(out, hb.a_offset_z_bits);
		AppendU32(out, hb.b_offset_x_bits);
		AppendU32(out, hb.b_offset_y_bits);
		AppendU32(out, hb.b_offset_z_bits);
		AppendU32(out, hb.scale_bits);
		AppendU32(out, hb.a_pos_x_bits);
		AppendU32(out, hb.a_pos_y_bits);
		AppendU32(out, hb.a_pos_z_bits);
		AppendU32(out, hb.b_pos_x_bits);
		AppendU32(out, hb.b_pos_y_bits);
		AppendU32(out, hb.b_pos_z_bits);
		AppendI32(out, hb.bone_index);
		AppendU32(out, hb.height);
		AppendU8(out, hb.is_grabbable);
		AppendU8(out, hb.flags);
		AppendU16(out, 0);
	}

	File::CreateFullPath(m_path);
	std::ofstream file(m_path, std::ios::binary);
	if (!file.is_open())
		return;
	file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
	file.close();

	m_path.clear();
	m_frames.clear();
	m_inputs.clear();
	m_fighters.clear();
	m_items.clear();
	m_hitboxes.clear();
	m_hurtboxes.clear();
	m_started = false;
	m_last_frame = INT_MIN;
}
