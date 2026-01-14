#pragma once

#include <SlippiLib/SlippiGame.h>
#include <cstdint>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class EngineDumpWriter
{
public:
	EngineDumpWriter(const std::string& path, int start_frame, int end_frame);
	~EngineDumpWriter();

	bool IsEnabled() const { return !m_path.empty(); }
	void SetGameSettings(const Slippi::GameSettings& settings);
	void CaptureFrame(s32 frame_index, Slippi::FrameData* frame);
	void Finalize();

private:
	std::string m_path;
	int m_start_frame;
	int m_end_frame;
	bool m_started = false;
	s32 m_last_frame = INT_MIN;
	u8 m_port_count = 2;
	u16 m_stage_id = 0;
	u8 m_is_teams = 0;
	u32 m_last_rng_seed = 0;
	bool m_last_rng_seed_valid = false;

	struct FrameRecord
	{
		s32 frame_index;
		u32 rng_state;
		u32 rng_seed;
		u32 rng_steps;
		u16 item_count;
		u32 item_offset;
		u16 flags;
		u32 game_timer;
		u8 randall_exists;
		u32 randall_x_bits;
		u32 randall_y_bits;
		u8 fountain0_exists;
		u32 fountain0_y_bits;
		u8 fountain1_exists;
		u32 fountain1_y_bits;
	};

	struct InputRecord
	{
		u32 buttons;
		u32 stick_x_bits;
		u32 stick_y_bits;
		u32 cstick_x_bits;
		u32 cstick_y_bits;
		u32 l_shoulder_bits;
		u32 r_shoulder_bits;
	};

	struct FighterRecord
	{
		u32 flags;
		u32 pos_x_bits;
		u32 pos_y_bits;
		u32 pos_z_bits;
		u32 self_vel_x_bits;
		u32 self_vel_y_bits;
		u32 gr_vel_bits;
		u16 action_state;
		u16 anim_id;
		u32 anim_frame_bits;
		u32 action_frame_bits;
		u8 state_flags_2218;
		u8 state_flags_221a;
		u8 state_flags_221b;
		u8 state_flags_221c;
	u8 state_flags_221f;
		u8 invulnerable;
		u8 ground_or_air;
		u8 stocks;
		u8 team;
		u8 costume_id;
		u32 facing_bits;
		u32 percent_bits;
		u32 hitlag_left_bits;
	u32 misc_as_bits;
		u32 shield_health_bits;
		u32 ecb_top_x_bits;
		u32 ecb_top_y_bits;
		u32 ecb_bottom_x_bits;
		u32 ecb_bottom_y_bits;
		u32 ecb_left_x_bits;
		u32 ecb_left_y_bits;
		u32 ecb_right_x_bits;
		u32 ecb_right_y_bits;
	};

	struct ItemRecord
	{
		u32 item_id;
		u16 kind;
		u16 state;
		int8_t owner_port;
		u8 flags;
		u32 pos_x_bits;
		u32 pos_y_bits;
		u32 pos_z_bits;
		u32 vel_x_bits;
		u32 vel_y_bits;
		u32 vel_z_bits;
		u32 facing_bits;
		u16 anim_id;
		u32 anim_frame_bits;
		u32 lifetime_bits;
		u32 damage;
	};

	struct HitboxRecord
	{
		u32 state;
		u32 group;
		u32 damage;
		u32 damage_stale_bits;
		u32 offset_x_bits;
		u32 offset_y_bits;
		u32 offset_z_bits;
		u32 size_bits;
		u32 angle;
		u32 kbg;
		u32 wsk;
		u32 bkb;
		u32 element;
		u32 shield_damage;
		u32 sfx;
		u32 sfx_kind;
		u8 flags[8];
		u32 bone_ptr;
		u32 pos_x_bits;
		u32 pos_y_bits;
		u32 pos_z_bits;
	};

	struct HurtboxRecord
	{
		u32 state;
		u32 a_offset_x_bits;
		u32 a_offset_y_bits;
		u32 a_offset_z_bits;
		u32 b_offset_x_bits;
		u32 b_offset_y_bits;
		u32 b_offset_z_bits;
		u32 scale_bits;
		u8 flags;
		u32 a_pos_x_bits;
		u32 a_pos_y_bits;
		u32 a_pos_z_bits;
		u32 b_pos_x_bits;
		u32 b_pos_y_bits;
		u32 b_pos_z_bits;
		s32 bone_index;
		u32 height;
		u8 is_grabbable;
	};

	std::vector<FrameRecord> m_frames;
	std::vector<InputRecord> m_inputs;
	std::vector<FighterRecord> m_fighters;
	std::vector<ItemRecord> m_items;
	std::vector<HitboxRecord> m_hitboxes;
	std::vector<HurtboxRecord> m_hurtboxes;
};
