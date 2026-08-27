/*
 * Copyright (c) 2018-2026 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <bdk.h>
#include <libs/fatfs/ff.h>

#include "gui.h"
#include "gui_tools.h"
#include "gui_tools_files.h"

#define FM_PATH_SIZE     1024
#define FM_NAME_SIZE     256
#define FM_MAX_ENTRIES   2048
#define FM_COPY_BUF_SIZE 0x100000
#define FM_VIEW_MAX      0x10000
#define FM_LONGPRESS_MS  400

#define FM_KB_NEWDIR 0
#define FM_KB_RENAME 1

extern lv_res_t launch_payload_path(const char *path);
extern lv_img_dsc_t *bmp_to_lvimg_obj(const char *path);
extern lv_img_dsc_t *png_to_lvimg_obj(const char *path);
extern lv_img_dsc_t *jpg_to_lvimg_obj(const char *path);

typedef struct _fm_entry_t
{
	char  name[FM_NAME_SIZE];
	u64   size;
	u16   date;
	u16   time;
	bool  is_dir;
} fm_entry_t;

typedef struct _file_manager_t
{
	lv_obj_t *win;
	lv_obj_t *list;
	lv_obj_t *status_lbl;
	lv_obj_t *kb_ta;
	lv_obj_t *btn_new;
	lv_obj_t *btn_copy;
	lv_obj_t *btn_cut;
	lv_obj_t *btn_rename;
	lv_obj_t *btn_delete;
	lv_obj_t *delete_mbox;
	lv_obj_t *xb_label;
	lv_obj_t *yb_label;
	u32       kb_op;
	char      cwd[FM_PATH_SIZE];
	char      sel[FM_NAME_SIZE];
	bool      has_sel;
	bool      sel_is_dir;
	char      clip[FM_PATH_SIZE];
	bool      has_clip;
	bool      clip_cut;
	bool      clip_is_dir;
} file_manager_t;

static file_manager_t fm;
static u64 fm_progress_total = 0;
static u64 fm_progress_done = 0;
static u32 fm_progress_last = 101;
static lv_obj_t *fm_progress_bg = NULL;
static lv_obj_t *fm_progress_mbox = NULL;

static char fm_pending_src[FM_PATH_SIZE];
static char fm_pending_dst[FM_PATH_SIZE];
static char fm_pending_name[FM_NAME_SIZE];

static bool fm_pending_overwrite = false;

static lv_task_t *fm_pending_task = NULL;

// File list
static fm_entry_t *fm_entries = NULL;
static u32 fm_entry_count = 0;

static lv_obj_t *fm_btns[FM_MAX_ENTRIES + 1];
static lv_obj_t *fm_selected_btn = NULL;
static lv_obj_t *fm_last_btn = NULL;
static u32 fm_selected_idx = 0;
static char fm_return_sel[FM_NAME_SIZE] = "";

static bool fm_press_pending = false;
static bool fm_press_fired = false;

// Text viewer
static lv_obj_t *fm_view_win = NULL;
static lv_obj_t *fm_view_ta = NULL;
static lv_obj_t *fm_view_kb = NULL;
static char fm_view_path[FM_PATH_SIZE];
static bool fm_view_editing = false;
static bool fm_view_truncated = false;

// Image viewer
static lv_obj_t *fm_image_win = NULL;
static lv_obj_t *fm_image_obj = NULL;
static lv_obj_t *fm_image_controls = NULL;
static lv_img_dsc_t *fm_image_original = NULL;
static lv_img_dsc_t *fm_image_data = NULL;
static u32 fm_image_rotation = 0;
static bool fm_image_switching = false;
static bool fm_image_read_mode = false;

static lv_obj_t *fm_image_close_btn = NULL;
static lv_obj_t *fm_image_prev_btn = NULL;
static lv_obj_t *fm_image_next_btn = NULL;
static lv_obj_t *fm_image_rotate_btn = NULL;
static lv_obj_t *fm_image_read_btn = NULL;

static lv_obj_t *fm_image_prev_lbl = NULL;
static lv_obj_t *fm_image_rotate_lbl = NULL;
static lv_obj_t *fm_image_read_lbl = NULL;
static lv_obj_t *fm_image_read_prev_btn = NULL;
static lv_obj_t *fm_image_read_prev_lbl = NULL;

static lv_obj_t *fm_image_hint_bb_bg = NULL;
static lv_obj_t *fm_image_hint_bb_label = NULL;
static lv_obj_t *fm_image_hint_zlrb_bg = NULL;
static lv_obj_t *fm_image_hint_zlrb_label = NULL;
static lv_obj_t *fm_image_hint_lrb_bg = NULL;
static lv_obj_t *fm_image_hint_lrb_label = NULL;

// Cache one decoded image for instant reverse navigation.
static lv_img_dsc_t *fm_image_prev_original = NULL;
static lv_img_dsc_t *fm_image_next_original = NULL;

static lv_img_dsc_t *fm_image_prev_data = NULL;
static lv_img_dsc_t *fm_image_next_data = NULL;

static char fm_image_prev_name[FM_NAME_SIZE] = "";
static char fm_image_next_name[FM_NAME_SIZE] = "";

static lv_task_t *fm_image_cache_task = NULL;
static u32 fm_image_cache_generation = 0;

// Keyboard / message box
static lv_obj_t *fm_kb = NULL;
static lv_obj_t *fm_mbox_btnm = NULL;

// Input hints
static lv_obj_t *yb_bg = NULL;
static lv_obj_t *yb_label = NULL;
static lv_obj_t *xb_bg = NULL;
static lv_obj_t *xb_label = NULL;
static lv_obj_t *ab_bg = NULL;
static lv_obj_t *ab_label = NULL;
static lv_obj_t *bb_bg = NULL;
static lv_obj_t *bb_label = NULL;
static lv_obj_t *dpad_bg = NULL;
static lv_obj_t *dpad_label = NULL;
static lv_obj_t *lrb_bg = NULL;
static lv_obj_t *lrb_label = NULL;
static lv_obj_t *zlrb_bg = NULL;
static lv_obj_t *zlrb_label = NULL;
static lv_obj_t *mb_label = NULL;
static lv_obj_t *pb_label = NULL;

// File browser hints
static lv_obj_t *fb_ab_bg = NULL;
static lv_obj_t *fb_ab_label = NULL;
static lv_obj_t *fb_bb_bg = NULL;
static lv_obj_t *fb_bb_label = NULL;
static lv_obj_t *fb_dpad_bg = NULL;
static lv_obj_t *fb_dpad_label = NULL;

static lv_res_t _fm_newfolder_action(lv_obj_t *btn);
static lv_res_t _fm_copy_action(lv_obj_t *btn);
static lv_res_t _fm_cut_action(lv_obj_t *btn);
static lv_res_t _fm_paste_action(lv_obj_t *btn);
static lv_res_t _fm_delete_action(lv_obj_t *btn);
static lv_res_t _fm_rename_action(lv_obj_t *btn);
static lv_res_t _fm_kb_close_action(lv_obj_t *kb);

static void _fm_refresh(void);
static void _fm_update_status(void);
static void _fm_progress_show(const char *text);
static void _fm_progress_set(const char *text);
static void _fm_progress_close(void);

static void _fm_progress_begin(const char *text, u64 total);
static void _fm_progress_update(const char *text, u64 amount);
static void _fm_progress_end(void);
static void _fm_paste_execute(const char *src_path, const char *dst_path, bool overwrite);

static void _fm_paste_task(void *param);
static void _fm_delete_task(void *param);
static void _fm_enter_dir_task(void *param);
static void _fm_image_open_task(void *param);
static void _fm_dpad(int dir);
static void _fm_b_action(void);
static void _fm_b_long_action(void);
static void _fm_enter_selected(void);
static void _fm_enter_dir(const char *name);
static void _fm_restore_browser_input(void);
static void _fm_select_btn(lv_obj_t *btn);
static void _fm_image_cache_task_cb(void *param);
static void _fm_image_schedule_cache(void);
static void _fm_image_previous(void);
static void _fm_image_next(void);
static void _fm_image_rotate_left(void);
static void _fm_image_rotate_right(void);
static void _fm_image_previous_input(void);
static void _fm_image_next_input(void);
static void _fm_image_rotate_left_input(void);
static void _fm_image_rotate_right_input(void);
static void _fm_image_set_read_mode(bool enable);
static lv_res_t _fm_image_read_btn_action(lv_obj_t *btn);

static void _fm_view_dpad(int dir);
static void _fm_view_a(void);

static void _fm_cancel_clipboard_action(void);
static lv_res_t _fm_cancel_clipboard_btn_action(lv_obj_t *btn);

static void _fm_del_obj(lv_obj_t **obj)
{
	if (*obj)
	{
		lv_obj_del(*obj);
		*obj = NULL;
	}
}

static void _fm_clear_hint_labels(void)
{
	_fm_del_obj(&yb_bg);
	_fm_del_obj(&yb_label);

	_fm_del_obj(&xb_bg);
	_fm_del_obj(&xb_label);

	_fm_del_obj(&ab_bg);
	_fm_del_obj(&ab_label);

	_fm_del_obj(&bb_bg);
	_fm_del_obj(&bb_label);

	_fm_del_obj(&dpad_bg);
	_fm_del_obj(&dpad_label);

	_fm_del_obj(&lrb_bg);
	_fm_del_obj(&lrb_label);

	_fm_del_obj(&zlrb_bg);
	_fm_del_obj(&zlrb_label);

	_fm_del_obj(&mb_label);
	_fm_del_obj(&pb_label);
}

static void _fm_set_browser_hints_hidden(bool hidden)
{
	if (fb_ab_bg)    lv_obj_set_hidden(fb_ab_bg, hidden);
	if (fb_ab_label) lv_obj_set_hidden(fb_ab_label, hidden);

	if (fb_bb_bg)    lv_obj_set_hidden(fb_bb_bg, hidden);
	if (fb_bb_label) lv_obj_set_hidden(fb_bb_label, hidden);

	if (fb_dpad_bg)    lv_obj_set_hidden(fb_dpad_bg, hidden);
	if (fb_dpad_label) lv_obj_set_hidden(fb_dpad_label, hidden);
}

static void _fm_close_browser_keyboard(lv_obj_t *kb)
{
	lv_obj_t *dark_bg = lv_obj_get_parent(kb);

	nyx_jc_kb_repeat = false;

	_fm_clear_hint_labels();
	lv_obj_del(dark_bg);
	_fm_set_browser_hints_hidden(false);

	fm.kb_ta = NULL;
	fm_kb = NULL;
}

static lv_obj_t *_fm_create_hint_bg(lv_obj_t *parent, const char *text)
{
	lv_obj_t *obj = lv_label_create(parent, NULL);

	lv_obj_set_style(obj, &hint_small_style_white);
	lv_label_set_static_text(obj, text);

	return obj;
}

static lv_obj_t *_fm_create_dark_bg(void)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);

	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	return dark_bg;
}

static lv_obj_t *_fm_create_hint_label(lv_obj_t *parent, const char *text)
{
	lv_obj_t *obj = lv_label_create(parent, NULL);

	lv_obj_set_style(obj, &hint_small_style);
	lv_label_set_recolor(obj, true);
	lv_label_set_static_text(obj, text);

	return obj;
}

static lv_obj_t *_fm_create_keyboard(lv_obj_t *parent, lv_obj_t *ta, lv_action_t ok_action, lv_action_t hide_action)
{
	lv_obj_t *kb = lv_kb_create(parent, NULL);

	lv_kb_set_ta(kb, ta);
	lv_kb_set_mode(kb, LV_KB_MODE_TEXT);
	lv_kb_set_cursor_manage(kb, true);
	lv_kb_set_ok_action(kb, ok_action);
	lv_kb_set_hide_action(kb, hide_action);
	lv_obj_set_size(kb, LV_HOR_RES, LV_VER_RES * 2 / 5);
	lv_obj_align(kb, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

	nyx_jc_kb_repeat = true;

	return kb;
}

static void _fm_setup_keyboard_hints(lv_obj_t *parent, lv_obj_t *anchor, bool vertical_cursor)
{
	// Y
	yb_bg = _fm_create_hint_bg(parent, "ⓝ");
	lv_obj_align(yb_bg, anchor, LV_ALIGN_OUT_TOP_RIGHT, -85, -10);
	yb_label = _fm_create_hint_label(parent, "#2C8F76 ⓨ#  입력 전환");
	lv_obj_align(yb_label, yb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	// X
	xb_bg = _fm_create_hint_bg(parent, "ⓝ");
	lv_obj_align(xb_bg, yb_bg, LV_ALIGN_OUT_LEFT_MID, -103, 0);
	xb_label = _fm_create_hint_label(parent, "#1374E6 ⓧ#  스페이스");
	lv_obj_align(xb_label, xb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	// B
	bb_bg = _fm_create_hint_bg(parent, "ⓝ");
	lv_obj_align(bb_bg, xb_bg, LV_ALIGN_OUT_LEFT_MID, -89, 0);
	bb_label = _fm_create_hint_label(parent, "#C7AD59 ⓑ#  지우기");
	lv_obj_align(bb_label, bb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	// A
	ab_bg = _fm_create_hint_bg(parent, "ⓝ");
	lv_obj_align(ab_bg, bb_bg, LV_ALIGN_OUT_LEFT_MID, -75, 0);
	ab_label = _fm_create_hint_label(parent, "#D14149 ⓐ#  입력");
	lv_obj_align(ab_label, ab_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	lv_obj_t *left_anchor = ab_bg;

	if (vertical_cursor)
	{
		// ZL/ZR
		zlrb_bg = _fm_create_hint_bg(parent, "ⓘ ⓘ");
		lv_obj_align(zlrb_bg, left_anchor, LV_ALIGN_OUT_LEFT_MID, -115, 0);
		zlrb_label = _fm_create_hint_label(parent, "#4B4B4B ⓖⓗ#  상하 커서");
		lv_obj_align(zlrb_label, zlrb_bg, LV_ALIGN_IN_LEFT_MID, -2, 0);

		left_anchor = zlrb_bg;
	}

	// L/R
	lrb_bg = _fm_create_hint_bg(parent, "ⓘ ⓘ");
	lv_obj_align(lrb_bg, left_anchor, LV_ALIGN_OUT_LEFT_MID, vertical_cursor ? -LV_DPI * 6 / 5 : -115, 0);
	lrb_label = _fm_create_hint_label(parent, "#4B4B4B ⓔⓕ#  좌우 커서");
	lv_obj_align(lrb_label, lrb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	// D-Pad
	dpad_bg = _fm_create_hint_bg(parent, "ⓓ");
	lv_obj_align(dpad_bg, lrb_bg, LV_ALIGN_OUT_LEFT_MID, -75, 0);
	dpad_label = _fm_create_hint_label(parent, "#4B4B4B ⓒ#  이동");
	lv_obj_align(dpad_label, dpad_bg, LV_ALIGN_IN_LEFT_MID, 0, 0);

	// -
	mb_label = _fm_create_hint_label(parent, "#4B4B4B ⓜ#  취소");
	lv_obj_align(mb_label, dpad_bg, LV_ALIGN_OUT_LEFT_MID, -30, 0);

	// +
	pb_label = _fm_create_hint_label(parent, vertical_cursor ? "#4B4B4B ⓟ#  저장" : "#4B4B4B ⓟ#  완료");
	lv_obj_align(pb_label, mb_label, LV_ALIGN_OUT_LEFT_MID, -30, 0);
}

static lv_obj_t *_fm_get_active_ta(void)
{
	return fm.kb_ta ? fm.kb_ta : fm_view_ta;
}

static void _fm_cursor_left(void)
{
	lv_obj_t *ta = _fm_get_active_ta();

	if (ta)
		lv_ta_cursor_left(ta);
}

static void _fm_cursor_right(void)
{
	lv_obj_t *ta = _fm_get_active_ta();

	if (ta)
		lv_ta_cursor_right(ta);
}

static void _fm_cursor_up(void)
{
	lv_obj_t *ta = _fm_get_active_ta();

	if (ta)
		lv_ta_cursor_up(ta);
}

static void _fm_cursor_down(void)
{
	lv_obj_t *ta = _fm_get_active_ta();

	if (ta)
		lv_ta_cursor_down(ta);
}

static bool _fm_dpad_to_group_key(int dir, char *key)
{
	switch (dir)
	{
	case NYX_DPAD_LEFT:
		*key = LV_GROUP_KEY_LEFT;
		break;
	case NYX_DPAD_RIGHT:
		*key = LV_GROUP_KEY_RIGHT;
		break;
	case NYX_DPAD_UP:
		*key = LV_GROUP_KEY_UP;
		break;
	case NYX_DPAD_DOWN:
		*key = LV_GROUP_KEY_DOWN;
		break;
	default:
		return false;
	}

	return true;
}

static void _fm_view_dummy(void)
{
	// Ignore unused button inputs.
}

static void _fm_clear_input_actions(void)
{
	nyx_jc_plus_action  = NULL;
	nyx_jc_minus_action = NULL;
	nyx_jc_r3_action    = NULL;

	nyx_jc_dpad_action = NULL;
	nyx_jc_a_action    = NULL;
	nyx_jc_b_action    = NULL;
	nyx_jc_b_long_action = NULL;
	nyx_jc_x_action    = NULL;
	nyx_jc_y_action    = NULL;

	nyx_jc_l_action  = NULL;
	nyx_jc_r_action  = NULL;
	nyx_jc_zl_action = NULL;
	nyx_jc_zr_action = NULL;
}

static void _fm_set_view_input(void)
{
	nyx_jc_dpad_action = _fm_view_dpad;
	nyx_jc_a_action    = _fm_view_a;
	nyx_jc_b_action    = _fm_b_action;

	nyx_jc_plus_action  = _fm_view_dummy;
	nyx_jc_minus_action = _fm_view_dummy;
	nyx_jc_x_action     = _fm_view_dummy;
	nyx_jc_y_action     = _fm_view_dummy;
	nyx_jc_r3_action    = _fm_view_dummy;

	nyx_jc_l_action  = NULL;
	nyx_jc_r_action  = NULL;
	nyx_jc_zl_action = NULL;
	nyx_jc_zr_action = NULL;

	nyx_jc_dpad_mode = true;
}

static void _fm_mbox_dpad(int dir)
{
	if (!fm_mbox_btnm)
		return;

	char key;

	if (!_fm_dpad_to_group_key(dir, &key))
		return;

	lv_btnm_control(fm_mbox_btnm, key);
}

static void _fm_mbox_a(void)
{
	if (fm_mbox_btnm)
		lv_btnm_control(fm_mbox_btnm, LV_GROUP_KEY_ENTER);
}

static void _fm_close_mbox(lv_obj_t *mbox)
{
	if (!mbox)
		return;

	lv_obj_t *dark_bg = lv_obj_get_parent(mbox);

	if (fm.delete_mbox == mbox)
		fm.delete_mbox = NULL;

	fm_mbox_btnm = NULL;

	lv_obj_del(dark_bg);

	// Restore input according to the currently active screen.
	if (fm_view_ta)
		_fm_set_view_input();
	else
		_fm_restore_browser_input();
}

static void _fm_msg_b(void)
{
	if (!fm_mbox_btnm)
		return;

	lv_obj_t *mbox = lv_mbox_get_from_btn(fm_mbox_btnm);

	_fm_close_mbox(mbox);
}

static void _fm_mbox_lock_input(void)
{
	// Allow only D-Pad, A, and B while a message box is active.
	nyx_jc_dpad_action = _fm_mbox_dpad;
	nyx_jc_a_action = _fm_mbox_a;
	nyx_jc_b_action = _fm_msg_b;

	// Block all other inputs.
	nyx_jc_plus_action  = _fm_view_dummy;
	nyx_jc_minus_action = _fm_view_dummy;

	nyx_jc_x_action = _fm_view_dummy;
	nyx_jc_y_action = _fm_view_dummy;

	nyx_jc_l_action  = _fm_view_dummy;
	nyx_jc_r_action  = _fm_view_dummy;
	nyx_jc_zl_action = _fm_view_dummy;
	nyx_jc_zr_action = _fm_view_dummy;

	nyx_jc_r3_action = _fm_view_dummy;
}

static bool _fm_join(char *out, const char *dir, const char *name)
{
	u32 dir_len = strlen(dir);
	u32 name_len = strlen(name);
	u32 len;

	// Avoid adding an extra separator to the root path.
	if (dir[1] == 0)
		len = 1 + name_len;
	else
		len = dir_len + 1 + name_len;

	if (len >= FM_PATH_SIZE)
	{
		out[0] = 0;
		return false;
	}

	if (dir[1] == 0)
		s_printf(out, "/%s", name);
	else
		s_printf(out, "%s/%s", dir, name);

	return true;
}

static void _fm_go_up(void)
{
	if (fm.cwd[1] == 0)
		return;

	char *p = strrchr(fm.cwd, '/');
	if (p == fm.cwd)
		p[1] = 0;
	else
		*p = 0;
}

static const char *_fm_basename(const char *path)
{
	const char *p = strrchr(path, '/');
	return p ? p + 1 : path;
}

static lv_coord_t _fm_text_width(const char *text, const lv_style_t *style)
{
	lv_point_t size;

	lv_txt_get_size(&size, text, style->text.font, style->text.letter_space, style->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);

	return size.x;
}

static void _fm_fit_name(char *dst, u32 dst_size, const char *src, const lv_style_t *style, lv_coord_t max_width)
{
	u32 len = strlen(src);

	if (len >= dst_size)
		len = dst_size - 1;

	memcpy(dst, src, len);
	dst[len] = 0;

	if (_fm_text_width(dst, style) <= max_width)
		return;

	while (dst[0])
	{
		u32 len = strlen(dst);

		do
		{
			len--;
		}
		while (len && ((u8)dst[len] & 0xC0) == 0x80);

		dst[len] = 0;

		char tmp[FM_NAME_SIZE];
		s_printf(tmp, "%s...", dst);

		if (_fm_text_width(tmp, style) <= max_width)
		{
			strcpy(dst, tmp);
			return;
		}
	}
	strcpy(dst, "...");
}

static bool _fm_name_valid(const char *name)
{
	if (!name[0])
		return false;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return false;

	for (const char *p = name; *p; p++)
	{
		char c = *p;
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			return false;
	}

	return true;
}

static void _fm_size_str(char *out, u64 size)
{
	if (size < 1000)
		s_printf(out, "%d B", (u32)size);
	else if (size < 1000ull * 1000)
		s_printf(out, "%d KB", (u32)(size / 1000));
	else if (size < 1000ull * 1000 * 1000)
		s_printf(out, "%d.%d MB",
			(u32)(size / (1000ull * 1000)),
			(u32)(((size % (1000ull * 1000)) * 10) / (1000ull * 1000)));
	else
		s_printf(out, "%d.%d GB",
			(u32)(size / (1000ull * 1000 * 1000)),
			(u32)(((size % (1000ull * 1000 * 1000)) * 10) / (1000ull * 1000 * 1000)));
}

static void _fm_date_str(char *out, u16 date, u16 time)
{
	if (!date)
	{
		strcpy(out, "-");
		return;
	}

	u32 year  = 1980 + ((date >> 9) & 0x7F);
	u32 month = (date >> 5) & 0xF;
	u32 day   = date & 0x1F;
	u32 hour  = (time >> 11) & 0x1F;
	u32 min   = (time >> 5) & 0x3F;

	// Convert 24-hour time to 12-hour AM/PM format.
	const char *ampm = (hour < 12) ? "오전" : "오후";

	hour %= 12;
	if (hour == 0)
		hour = 12;

	s_printf(out, "%04d-%02d-%02d %s %02d:%02d", year, month, day, ampm, hour, min);
}

static lv_res_t _fm_msg_action(lv_obj_t *btns, const char *txt)
{
	lv_obj_t *mbox = lv_mbox_get_from_btn(btns);

	_fm_close_mbox(mbox);

	return LV_RES_INV;
}

static void _fm_msg(const char *text)
{
	lv_obj_t *dark_bg = _fm_create_dark_bg();

	static const char *mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_mbox_set_text(mbox, text);
	lv_mbox_add_btns(mbox, mbox_btn_map, _fm_msg_action);
	fm_mbox_btnm = lv_obj_get_child(mbox, NULL);
	lv_btnm_control(fm_mbox_btnm, LV_GROUP_KEY_RIGHT);
	lv_btnm_control(fm_mbox_btnm, LV_GROUP_KEY_RIGHT);

	_fm_mbox_lock_input();

	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);
}

static u64 _fm_get_total_size(char *path)
{
	FILINFO st;

	if (f_stat(path, &st) != FR_OK)
		return 0;

	if (!(st.fattrib & AM_DIR))
		return st.fsize;

	DIR dir;

	if (f_opendir(&dir, path) != FR_OK)
		return 0;

	FILINFO *fno = malloc(sizeof(FILINFO));

	if (!fno)
	{
		f_closedir(&dir);
		return 0;
	}

	u64 total = 0;
	u32 len = strlen(path);

	for (;;)
	{
		if (f_readdir(&dir, fno) != FR_OK ||
			fno->fname[0] == 0)
			break;

		u32 nlen = strlen(fno->fname);

		if (len + 1 + nlen >= FM_PATH_SIZE)
			continue;

		path[len] = '/';
		strcpy(&path[len + 1], fno->fname);

		if (fno->fattrib & AM_DIR)
			total += _fm_get_total_size(path);
		else
			total += fno->fsize;

		path[len] = 0;

		manual_system_maintenance(true);
	}

	f_closedir(&dir);
	free(fno);

	return total;
}

static int _fm_copy_file(const char *src, const char *dst)
{
	FIL *fs = malloc(sizeof(FIL));
	FIL *fd = malloc(sizeof(FIL));
	u8 *buf = NULL;

	if (!fs || !fd)
	{
		free(fs);
		free(fd);
		return FR_NOT_ENOUGH_CORE;
	}

	int res = f_open(fs, src, FA_READ | FA_OPEN_EXISTING);
	if (res != FR_OK)
		goto out;

	res = f_open(fd, dst, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK)
	{
		f_close(fs);
		goto out;
	}

	buf = malloc(FM_COPY_BUF_SIZE);
	if (!buf)
	{
		f_close(fs);
		f_close(fd);
		res = FR_NOT_ENOUGH_CORE;
		goto out;
	}

	for (;;)
	{
		UINT br = 0, bw = 0;

		res = f_read(fs, buf, FM_COPY_BUF_SIZE, &br);
		if (res != FR_OK || br == 0)
			break;

		res = f_write(fd, buf, br, &bw);
		if (res != FR_OK)
			break;

		if (bw != br)
		{
			res = FR_DISK_ERR;
			break;
		}

		_fm_progress_update("붙여넣는 중...", bw);

		// Keep system services responsive during long file operations.
		manual_system_maintenance(true);
	}

	f_close(fs);
	f_close(fd);

out:
	if (buf)
		free(buf);

	free(fs);
	free(fd);

	return res;
}

static int _fm_copy_recursive(char *src, char *dst)
{
	FILINFO st;
	int res = f_stat(src, &st);
	if (res != FR_OK)
		return res;

	if (!(st.fattrib & AM_DIR))
		return _fm_copy_file(src, dst);

	res = f_mkdir(dst);
	if (res != FR_OK && res != FR_EXIST)
		return res;

	DIR dir;
	res = f_opendir(&dir, src);
	if (res != FR_OK)
		return res;

	FILINFO *fno = malloc(sizeof(FILINFO));
	if (!fno)
	{
		f_closedir(&dir);
		return FR_NOT_ENOUGH_CORE;
	}

	u32 slen = strlen(src);
	u32 dlen = strlen(dst);

	for (;;)
	{
		res = f_readdir(&dir, fno);
		if (res != FR_OK || fno->fname[0] == 0)
			break;

		u32 nlen = strlen(fno->fname);

		if (slen + 1 + nlen >= FM_PATH_SIZE || dlen + 1 + nlen >= FM_PATH_SIZE)
		{
			res = FR_INVALID_NAME;
			break;
		}

		src[slen] = '/';
		strcpy(&src[slen + 1], fno->fname);

		dst[dlen] = '/';
		strcpy(&dst[dlen + 1], fno->fname);

		res = _fm_copy_recursive(src, dst);

		src[slen] = 0;
		dst[dlen] = 0;

		if (res != FR_OK)
			break;
	}

	f_closedir(&dir);
	free(fno);

	return res;
}

static int _fm_delete_recursive(char *path)
{
	FILINFO st;

	if (f_stat(path, &st) != FR_OK)
		return FR_NO_FILE;

	if (!(st.fattrib & AM_DIR))
	{
		u64 size = st.fsize;

		int res = f_unlink(path);

		if (res == FR_OK)
			_fm_progress_update("삭제 중...", size);

		return res;
	}

	DIR dir;
	int res = f_opendir(&dir, path);
	if (res != FR_OK)
		return res;

	FILINFO *fno = malloc(sizeof(FILINFO));
	if (!fno)
	{
		f_closedir(&dir);
		return FR_NOT_ENOUGH_CORE;
	}

	u32 len = strlen(path);

	for (;;)
	{
		res = f_readdir(&dir, fno);
		if (res != FR_OK || fno->fname[0] == 0)
			break;

		u32 nlen = strlen(fno->fname);

		if (len + 1 + nlen >= FM_PATH_SIZE)
		{
			res = FR_INVALID_NAME;
			break;
		}

		path[len] = '/';
		strcpy(&path[len + 1], fno->fname);

		if (fno->fattrib & AM_DIR)
		{
			res = _fm_delete_recursive(path);
		}
		else
		{
			u64 size = fno->fsize;

			res = f_unlink(path);

			if (res == FR_OK)
				_fm_progress_update("삭제 중...", size);
		}

		path[len] = 0;

		// Keep system services responsive during long file operations.
		manual_system_maintenance(true);

		if (res != FR_OK)
			break;
	}

	f_closedir(&dir);
	free(fno);

	if (res == FR_OK)
		res = f_unlink(path);

	return res;
}

static void _fm_progress_show(const char *text)
{
	if (fm_progress_bg)
		return;

	fm_progress_bg = _fm_create_dark_bg();
	fm_progress_mbox = lv_mbox_create(fm_progress_bg, NULL);
	lv_mbox_set_recolor_text(fm_progress_mbox, true);
	lv_mbox_set_text(fm_progress_mbox, text);
	lv_obj_set_width(fm_progress_mbox, LV_HOR_RES / 3);
	lv_obj_align(fm_progress_mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(fm_progress_mbox, true);

	_fm_clear_input_actions();
	nyx_jc_dpad_mode = false;
}

static void _fm_progress_set(const char *text)
{
	if (!fm_progress_mbox)
		return;

	lv_mbox_set_text(fm_progress_mbox, text);
	lv_obj_align(fm_progress_mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_invalidate(fm_progress_mbox);
}

static void _fm_progress_close(void)
{
	if (fm_progress_bg)
	{
		lv_obj_del(fm_progress_bg);

		fm_progress_bg = NULL;
		fm_progress_mbox = NULL;
	}

	_fm_restore_browser_input();
}

static void _fm_progress_begin(const char *text, u64 total)
{
	fm_progress_total = total;
	fm_progress_done = 0;
	fm_progress_last = 101;

	char buf[128];

	if (total)
		s_printf(buf, "%s 0%%", text);
	else
		strcpy(buf, text);

	_fm_progress_set(buf);
}

static void _fm_progress_update(const char *text, u64 amount)
{
	fm_progress_done += amount;

	if (!fm_progress_total)
		return;

	u32 percent =
		(u32)((fm_progress_done * 100) /
		fm_progress_total);

	if (percent > 100)
		percent = 100;

	if (percent == fm_progress_last)
		return;

	fm_progress_last = percent;

	char buf[128];

	s_printf(buf, "%s %d%%", text, percent);

	_fm_progress_set(buf);
}

static void _fm_progress_end(void)
{
	fm_progress_total = 0;
	fm_progress_done = 0;
	fm_progress_last = 101;
}

static void _fm_update_status(void)
{
	char buf[FM_PATH_SIZE + FM_NAME_SIZE + 64];
	char clip[FM_PATH_SIZE + 16];

	if (fm_kb)
	{
		clip[0] = 0;
	}
	else if (fm.has_clip)
	{
		s_printf(clip, "%s %s: %s",
			fm.clip_cut ? "잘라낸" : "복사된",
			fm.clip_is_dir ? "폴더" : "파일",
			_fm_basename(fm.clip));
	}
	else
	{
		strcpy(clip, "비어있음");
	}

	s_printf(buf, "#C7EA46  "SYMBOL_LIST"  클립보드# | %s", clip);
	lv_label_set_text(fm.status_lbl, buf);

	bool copy_is_paste =
		fm.has_clip &&
		lv_btn_get_action(fm.btn_copy, LV_BTN_ACTION_CLICK) == _fm_paste_action;

	bool cut_is_paste =
		fm.has_clip &&
		lv_btn_get_action(fm.btn_cut, LV_BTN_ACTION_CLICK) == _fm_paste_action;

	if (fm.has_clip)
	{
		// Allow paste/cancel regardless of the current selection.
		lv_btn_set_state(fm.btn_copy, LV_BTN_STATE_REL);
		lv_btn_set_state(fm.btn_cut, LV_BTN_STATE_REL);

		if (cut_is_paste)
		{
			// X: paste, Y: cancel after cut.
			nyx_jc_x_action = (void (*)(void))_fm_paste_action;
			nyx_jc_y_action = _fm_cancel_clipboard_action;
		}
		else if (copy_is_paste)
		{
			// Y: paste, X: cancel after copy.
			nyx_jc_y_action = (void (*)(void))_fm_paste_action;
			nyx_jc_x_action = _fm_cancel_clipboard_action;
		}
	}
	else
	{
		// Enable copy/cut only when an item is selected.
		lv_btn_set_state(fm.btn_copy,
			fm.has_sel ? LV_BTN_STATE_REL : LV_BTN_STATE_INA);
		lv_btn_set_state(fm.btn_cut,
			fm.has_sel ? LV_BTN_STATE_REL : LV_BTN_STATE_INA);

		if (fm.has_sel)
		{
			nyx_jc_x_action = (void (*)(void))_fm_cut_action;
			nyx_jc_y_action = (void (*)(void))_fm_copy_action;
		}
		else
		{
			nyx_jc_x_action = _fm_view_dummy;
			nyx_jc_y_action = _fm_view_dummy;
		}
	}

	lv_btn_set_state(fm.btn_rename,
		fm.has_sel ? LV_BTN_STATE_REL : LV_BTN_STATE_INA);
	lv_btn_set_state(fm.btn_delete,
		fm.has_sel ? LV_BTN_STATE_REL : LV_BTN_STATE_INA);

	if (fm.has_sel)
	{
		nyx_jc_r3_action    = (void (*)(void))_fm_rename_action;
		nyx_jc_minus_action = (void (*)(void))_fm_delete_action;
	}
	else
	{
		nyx_jc_r3_action    = _fm_view_dummy;
		nyx_jc_minus_action = _fm_view_dummy;
	}

	// Keep new folder available regardless of the current selection.
	lv_btn_set_state(fm.btn_new, LV_BTN_STATE_REL);
	nyx_jc_plus_action = (void (*)(void))_fm_newfolder_action;
}

static void _fm_reset_clipboard_ui(void)
{
	lv_label_set_static_text(fm.yb_label, " ⓝ  복사 ");
	lv_btn_set_action(fm.btn_copy, LV_BTN_ACTION_CLICK, _fm_copy_action);

	lv_label_set_static_text(fm.xb_label, " ⓝ  잘라내기 ");
	lv_btn_set_action(fm.btn_cut, LV_BTN_ACTION_CLICK, _fm_cut_action);

	nyx_jc_y_action = (void (*)(void))_fm_copy_action;
	nyx_jc_x_action = (void (*)(void))_fm_cut_action;
}

static void _fm_cancel_clipboard_action(void)
{
	fm.has_clip = false;
	fm.clip_cut = false;
	fm.clip_is_dir = false;
	fm.clip[0] = 0;

	_fm_reset_clipboard_ui();
	_fm_update_status();
}

static lv_res_t _fm_cancel_clipboard_btn_action(lv_obj_t *btn)
{
	_fm_cancel_clipboard_action();

	return LV_RES_OK;
}

static bool _fm_ext_is(const char *name, const char *ext)
{
	u32 nl = strlen(name);
	u32 el = strlen(ext);

	if (el + 1 >= nl)
		return false;

	if (name[nl - el - 1] != '.')
		return false;

	for (u32 k = 0; k < el; k++)
	{
		char a = name[nl - el + k];
		if (a >= 'A' && a <= 'Z')
			a += 32;
		if (a != ext[k])
			return false;
	}

	return true;
}

static bool _fm_is_archive(const char *name)
{
	static const char *exts[] = {
		"zip", "7z", "rar", "tar", "gz", "bz2", "xz",
		"tgz", "tbz", "txz"
	};

	for (u32 i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
		if (_fm_ext_is(name, exts[i]))
			return true;

	return false;
}

static bool _fm_is_image(const char *name)
{
	return _fm_ext_is(name, "bmp") ||
	       _fm_ext_is(name, "png") ||
	       _fm_ext_is(name, "jpg") ||
	       _fm_ext_is(name, "jpeg");
}

static bool _fm_is_payload(const char *cwd, const char *name)
{
	if (!cwd || !name)
		return false;

	// update.bin: Only /bootloader/update.bin is allowed. update.bin anywhere else must never be treated as a payload.
	if (!strcasecmp(name, "update.bin"))
		return !strcmp(cwd, "/bootloader");

	// Keep existing behavior: Any .bin inside /bootloader/payloads is allowed.
	if (!strcmp(cwd, "/bootloader/payloads") &&
		_fm_ext_is(name, "bin"))
		return true;

	// Allow known payload filenames anywhere.
	static const char *payload_names[] = {
		"payload.bin",
		"fusee.bin",
		"reboot_payload.bin",
	};

	for (u32 i = 0; i < sizeof(payload_names) / sizeof(payload_names[0]); i++)
	{
		if (!strcasecmp(name, payload_names[i]))
			return true;
	}

	return false;
}

static lv_res_t _fm_payload_confirm_action(lv_obj_t *btns, const char *txt)
{
	lv_obj_t *mbox = lv_mbox_get_from_btn(btns);

	if (!strcmp(txt, "실행"))
	{
		char path[FM_PATH_SIZE];

		if (_fm_join(path, fm.cwd, fm.sel))
		{
			_fm_close_mbox(mbox);
			launch_payload_path(path);
			return LV_RES_INV;
		}
	}

	_fm_close_mbox(mbox);

	return LV_RES_INV;
}

static void _fm_launch_payload(void)
{
	if (!fm.has_sel || fm.sel_is_dir)
		return;

	if (!_fm_is_payload(fm.cwd, fm.sel))
		return;

	lv_obj_t *dark_bg = _fm_create_dark_bg();

	static const char *mbox_btn_map[] = {
		"\222실행",
		"\222취소",
		""
	};

	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);

	lv_mbox_set_recolor_text(mbox, true);

	char msg[FM_NAME_SIZE + 64];
	s_printf(msg, "#FF8000 %s#를 실행하시겠습니까?", fm.sel);

	lv_mbox_set_text(mbox, msg);
	lv_mbox_add_btns(mbox, mbox_btn_map, _fm_payload_confirm_action);

	fm_mbox_btnm = lv_obj_get_child(mbox, NULL);

	lv_btnm_control(fm_mbox_btnm, LV_GROUP_KEY_RIGHT);
	lv_btnm_control(fm_mbox_btnm, LV_GROUP_KEY_RIGHT);

	_fm_mbox_lock_input();

	// Keep the default width and expand only for long filenames.
	lv_coord_t min_width = LV_HOR_RES / 9 * 5;
	lv_coord_t max_width = LV_HOR_RES * 9 / 10;
	lv_coord_t name_width = _fm_text_width(fm.sel, &hint_small_style);

	// Extra space for suffix and message box padding.
	lv_coord_t mbox_width = name_width + LV_DPI * 3;

	if (mbox_width < min_width)
		mbox_width = min_width;
	else if (mbox_width > max_width)
		mbox_width = max_width;

	lv_obj_set_width(mbox, mbox_width);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);
}

static bool _fm_is_text(const char *name)
{
	static const char *exts[] = {
		"txt", "log", "ini", "cfg", "conf", "json", "md", "xml",
		"html", "htm", "css", "js", "c", "h", "cpp", "hpp", "py",
		"sh", "yaml", "yml", "toml", "csv", "nfo", "asm", "mk",
		"bat", "rc", "list", "map", "srt", "keys", "config"
	};

	for (u32 k = 0; k < sizeof(exts) / sizeof(exts[0]); k++)
		if (_fm_ext_is(name, exts[k]))
			return true;

	return false;
}

static bool _fm_write_file(const char *path, const char *txt)
{
	if (sd_mount())
		return false;

	FIL *fp = malloc(sizeof(FIL));
	if (!fp)
		return false;

	if (f_open(fp, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
	{
		free(fp);
		return false;
	}

	u32 len = strlen(txt);
	UINT bw = 0;
	int res = f_write(fp, txt, len, &bw);

	f_close(fp);
	free(fp);

	return res == FR_OK && bw == len;
}

static void _fm_setup_text_view_hints(bool editable)
{
	bb_bg = _fm_create_hint_bg(fm_view_win, "ⓝ");
	lv_obj_align(bb_bg, fm_view_ta, LV_ALIGN_OUT_TOP_RIGHT, -50, -10);
	bb_label = _fm_create_hint_label(fm_view_win, "#C7AD59 ⓑ#  뒤로");
	lv_obj_align(bb_label, bb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	if (!editable)
		return;

	ab_bg = _fm_create_hint_bg(fm_view_win, "ⓝ");
	lv_obj_align(ab_bg, bb_bg, LV_ALIGN_OUT_LEFT_MID, -75, 0);
	ab_label = _fm_create_hint_label(fm_view_win, "#D14149 ⓐ#  편집");
	lv_obj_align(ab_label, ab_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	dpad_bg = _fm_create_hint_bg(fm_view_win, "ⓓ");
	lv_obj_align(dpad_bg, ab_bg, LV_ALIGN_OUT_LEFT_MID, -75, 0);
	dpad_label = _fm_create_hint_label(fm_view_win, "#4B4B4B ⓒ#  이동");
	lv_obj_align(dpad_label, dpad_bg, LV_ALIGN_IN_LEFT_MID, 0, 0);
}

static void _fm_view_exit_edit(void)
{
	nyx_jc_kb_repeat = false;

	if (fm_view_kb)
	{
		lv_obj_del(fm_view_kb);
		fm_view_kb = NULL;
		fm_kb = NULL;
	}

	if (fm_view_ta)
	{
		lv_ta_set_cursor_type(fm_view_ta, LV_CURSOR_NONE);
		lv_obj_set_size(fm_view_ta, LV_HOR_RES * 94 / 100, LV_VER_RES * 80 / 100);
	}

	_fm_clear_hint_labels();

	fm_view_editing = false;
	_fm_set_view_input();

	_fm_setup_text_view_hints(true);
}

static lv_res_t _fm_view_save(lv_obj_t *kb)
{
	bool ok = _fm_write_file(fm_view_path, lv_ta_get_text(fm_view_ta));

	_fm_view_exit_edit();
	_fm_msg(ok ? "#96FF00 변경사항 저장됨#" : "#FFDD00 저장 실패!#");

	return LV_RES_INV;
}

static lv_res_t _fm_view_edit_cancel(lv_obj_t *kb)
{
	_fm_view_exit_edit();

	return LV_RES_INV;
}

static void _fm_view_save_action(void)
{
	if (fm_view_kb)
		_fm_view_save(fm_view_kb);
}

static void _fm_view_cancel_action(void)
{
	if (fm_view_kb)
		_fm_view_edit_cancel(fm_view_kb);
}

static void _fm_view_space_action(void)
{
	lv_obj_t *ta = _fm_get_active_ta();

	if (ta)
		lv_ta_add_char(ta, ' ');
}

static void _fm_view_mode_action(void)
{
	if (!fm_kb)
		return;

	lv_kb_cycle_map(fm_kb);
}

static void _fm_view_toggle_edit(void)
{
	if (!fm_view_ta)
		return;

	if (fm_view_editing)
	{
		_fm_view_exit_edit();
		return;
	}

	if (fm_view_truncated)
	{
		_fm_msg("#FFDD00 파일이 너무 커 편집할 수 없습니다#");
		return;
	}

	_fm_del_obj(&dpad_bg);
	_fm_del_obj(&dpad_label);
	_fm_del_obj(&ab_bg);
	_fm_del_obj(&ab_label);
	_fm_del_obj(&bb_bg);
	_fm_del_obj(&bb_label);

	fm_view_editing = true;

	nyx_jc_r3_action = _fm_view_dummy;
	nyx_jc_l_action  = _fm_cursor_left;
	nyx_jc_r_action  = _fm_cursor_right;
	nyx_jc_zl_action = _fm_cursor_up;
	nyx_jc_zr_action = _fm_cursor_down;

	nyx_jc_plus_action  = _fm_view_save_action;
	nyx_jc_minus_action = _fm_view_cancel_action;
	nyx_jc_x_action     = _fm_view_space_action;
	nyx_jc_y_action     = _fm_view_mode_action;

	lv_ta_set_cursor_type(fm_view_ta, LV_CURSOR_LINE);
	lv_obj_set_size(fm_view_ta, LV_HOR_RES * 94 / 100, LV_VER_RES * 48 / 100);

	lv_obj_t *kb = _fm_create_keyboard(
		lv_scr_act(),
		fm_view_ta,
		_fm_view_save,
		_fm_view_edit_cancel);

	fm_view_kb = kb;
	fm_kb = kb;

	_fm_setup_keyboard_hints(fm_view_win, fm_view_ta, true);
}

static lv_res_t _fm_view_edit_btn(lv_obj_t *btn)
{
	_fm_view_toggle_edit();

	return LV_RES_OK;
}

static void _fm_view_dpad(int dir)
{
	if (fm_kb)
	{
		char key;

		if (!_fm_dpad_to_group_key(dir, &key))
			return;

		lv_btnm_control(fm_kb, key);
		return;
	}

	if (!fm_view_ta)
		return;

	lv_obj_t *scrl = lv_page_get_scrl(fm_view_ta);

	switch (dir)
	{
	case NYX_DPAD_UP:
		lv_obj_set_y(scrl, lv_obj_get_y(scrl) + LV_DPI);
		break;

	case NYX_DPAD_DOWN:
		lv_obj_set_y(scrl, lv_obj_get_y(scrl) - LV_DPI);
		break;
	}
}

static lv_res_t _fm_close(lv_obj_t *btn)
{
	_fm_clear_input_actions();
	nyx_jc_dpad_mode = false;

	return nyx_win_close_action(btn);
}

static lv_res_t _fm_view_close(lv_obj_t *btn)
{
	if (fm_view_kb)
	{
		lv_obj_del(fm_view_kb);
		fm_view_kb = NULL;
		fm_kb = NULL;
	}

	fm_view_ta = NULL;
	fm_view_editing = false;

	_fm_restore_browser_input();

	if (btn)
	{
		fm_view_win = NULL;
		return nyx_win_close_action(btn);
	}

	if (fm_view_win)
	{
		lv_obj_del(fm_view_win);
		fm_view_win = NULL;
	}

	return LV_RES_OK;
}

static void _fm_view_a(void)
{
	if (fm_kb)
		lv_btnm_control(fm_kb, LV_GROUP_KEY_ENTER);
	else
		_fm_view_toggle_edit();
}

static void _fm_view_text(void)
{
	if (sd_mount())
	{
		_fm_msg("#FFDD00 SD 카드 초기화 실패!#");
		return;
	}

	if (!_fm_join(fm_view_path, fm.cwd, fm.sel))
	{
		_fm_msg("#FFDD00 경로가 너무 깁니다!#");
		return;
	}

	FIL *fp = malloc(sizeof(FIL));
	if (!fp)
	{
		_fm_msg("#FFDD00 메모리 할당 실패!#");
		return;
	}

	int res = f_open(fp, fm_view_path, FA_READ | FA_OPEN_EXISTING);

	if (res != FR_OK)
	{
		free(fp);
		_fm_msg("#FFDD00 파일 열기 실패!#");
		return;
	}

	u32 fsize = f_size(fp);
	bool truncated = fsize > FM_VIEW_MAX;
	u32 cap = truncated ? FM_VIEW_MAX : fsize;

	char *text = malloc(cap + 64);
	if (!text)
	{
		f_close(fp);
		free(fp);
		_fm_msg("#FFDD00 메모리 할당 실패!#");
		return;
	}

	UINT total = 0, br = 0;

	while (total < cap)
	{
		if (f_read(fp, text + total, cap - total, &br) != FR_OK || br == 0)
			break;
		total += br;
	}

	f_close(fp);
	free(fp);

	u32 w = 0;
	for (u32 r = 0; r < total; r++)
		if (text[r] != '\r')
			text[w++] = text[r];
	total = w;

	text[total] = 0;
	if (truncated)
		strcpy(&text[total], "\n\n[... file truncated ...]");

	fm_view_truncated = truncated;

	char title[FM_NAME_SIZE + 16];
	if (fm_view_truncated)
		s_printf(title, SYMBOL_FILE"  %s  (미지원 인코딩)", fm.sel);
	else
		s_printf(title, SYMBOL_FILE"  %s", fm.sel);

	fm_view_win = nyx_create_standard_window(title, _fm_view_close);
	lv_obj_t *win = fm_view_win;
	if (!fm_view_truncated)
		lv_win_add_btn(win, NULL, SYMBOL_FILE_CODE" 편집", _fm_view_edit_btn);
	fm_view_kb = NULL;
	fm_view_editing = false;
	nyx_jc_r3_action = _fm_view_dummy;
	nyx_jc_plus_action = _fm_view_dummy;

	fm_view_ta = lv_ta_create(win, NULL);
	lv_obj_t *ta = fm_view_ta;
	lv_ta_ext_t *ta_ext = lv_obj_get_ext_attr(ta);
	lv_obj_set_style(ta_ext->label, &monospace_text);
	lv_ta_set_cursor_type(ta, LV_CURSOR_NONE);
	lv_ta_set_text(ta, text);
	lv_ta_set_cursor_pos(ta, 0);
	lv_obj_set_size(ta, LV_HOR_RES * 94 / 100, LV_VER_RES * 80 / 100);

	_fm_setup_text_view_hints(!fm_view_truncated);

	_fm_set_view_input();

	if (fm_view_truncated)
		nyx_jc_a_action = NULL;

	free(text);
}

static void _fm_image_close(void)
{
	// Invalidate pending image cache work.
	fm_image_cache_generation++;
	fm_image_cache_task = NULL;

	if (fm_image_win)
	{
		lv_obj_del(fm_image_win);
		fm_image_win = NULL;
		fm_image_obj = NULL;
		fm_image_controls = NULL;
	}

	if (fm_image_data)
	{
		free(fm_image_data);
		fm_image_data = NULL;
	}

	if (fm_image_original)
	{
		free(fm_image_original);
		fm_image_original = NULL;
	}

	if (fm_image_prev_data)
	{
		free(fm_image_prev_data);
		fm_image_prev_data = NULL;
	}

	if (fm_image_next_data)
	{
		free(fm_image_next_data);
		fm_image_next_data = NULL;
	}

	if (fm_image_prev_original)
	{
		free(fm_image_prev_original);
		fm_image_prev_original = NULL;
	}

	if (fm_image_next_original)
	{
		free(fm_image_next_original);
		fm_image_next_original = NULL;
	}

	fm_image_prev_name[0] = 0;
	fm_image_next_name[0] = 0;

	fm_image_rotation = 0;
	fm_image_switching = false;
	fm_image_read_mode = false;

	nyx_vol_down_action = NULL;
	nyx_vol_up_action   = NULL;

	fm_image_close_btn = NULL;
	fm_image_prev_btn = NULL;
	fm_image_next_btn = NULL;
	fm_image_rotate_btn = NULL;
	fm_image_read_btn = NULL;

	fm_image_prev_lbl = NULL;
	fm_image_rotate_lbl = NULL;
	fm_image_read_lbl = NULL;

	fm_image_read_prev_btn = NULL;
	fm_image_read_prev_lbl = NULL;

	fm_image_hint_bb_bg = NULL;
	fm_image_hint_bb_label = NULL;
	fm_image_hint_zlrb_bg = NULL;
	fm_image_hint_zlrb_label = NULL;
	fm_image_hint_lrb_bg = NULL;
	fm_image_hint_lrb_label = NULL;

	_fm_restore_browser_input();
}

static void _fm_image_b_action(void)
{
	if (fm_image_read_mode)
	{
		_fm_image_rotate_right();
		_fm_image_set_read_mode(false);
		return;
	}

	_fm_image_close();
}

static lv_res_t _fm_image_close_btn_action(lv_obj_t *btn)
{
	_fm_image_close();

	return LV_RES_INV;
}

static lv_res_t _fm_image_prev_btn_action(lv_obj_t *btn)
{
	_fm_image_previous();

	return LV_RES_OK;
}

static lv_res_t _fm_image_next_btn_action(lv_obj_t *btn)
{
	_fm_image_next();

	return LV_RES_OK;
}

static lv_res_t _fm_image_rotate_btn_action(lv_obj_t *btn)
{
	_fm_image_rotate_right();

	return LV_RES_OK;
}

static void _fm_image_set_read_mode(bool enable)
{
	fm_image_read_mode = enable;
	if (fm_image_obj) lv_obj_align(fm_image_obj, NULL, enable ? LV_ALIGN_IN_LEFT_MID : LV_ALIGN_CENTER, 0, 0);

	if (enable)
	{
		nyx_vol_down_action = _fm_image_previous;
		nyx_vol_up_action   = _fm_image_next;
		nyx_jc_plus_action = _fm_view_dummy;
		nyx_jc_minus_action = _fm_view_dummy;
		nyx_jc_r3_action = _fm_view_dummy;
		nyx_jc_dpad_action = NULL;
		nyx_jc_a_action = _fm_view_dummy;
		nyx_jc_x_action = _fm_view_dummy;
		nyx_jc_y_action = _fm_view_dummy;
		nyx_jc_l_action = _fm_view_dummy;
		nyx_jc_r_action = _fm_view_dummy;
		nyx_jc_zl_action = _fm_view_dummy;
		nyx_jc_zr_action = _fm_view_dummy;
		nyx_jc_b_action = _fm_image_b_action;

		// Hide controls not used in read mode.
		if (fm_image_rotate_btn) lv_obj_set_hidden(fm_image_rotate_btn, true);
		if (fm_image_prev_btn) lv_obj_set_hidden(fm_image_prev_btn, true);
		if (fm_image_read_btn) lv_obj_set_hidden(fm_image_read_btn, true);

		// Hide physical button hints.
		if (fm_image_hint_bb_bg) lv_obj_set_hidden(fm_image_hint_bb_bg, true);
		if (fm_image_hint_bb_label) lv_obj_set_hidden(fm_image_hint_bb_label, true);
		if (fm_image_hint_zlrb_bg) lv_obj_set_hidden(fm_image_hint_zlrb_bg, true);
		if (fm_image_hint_zlrb_label) lv_obj_set_hidden(fm_image_hint_zlrb_label, true);
		if (fm_image_hint_lrb_bg) lv_obj_set_hidden(fm_image_hint_lrb_bg, true);
		if (fm_image_hint_lrb_label) lv_obj_set_hidden(fm_image_hint_lrb_label, true);

		// Close button -> next image.
		if (fm_image_close_btn)
		{
			lv_btn_set_action(fm_image_close_btn, LV_BTN_ACTION_CLICK, _fm_image_next_btn_action);
			lv_obj_t *lbl = lv_obj_get_child(fm_image_close_btn, NULL);
			if (lbl)
			{
				lv_label_set_text(lbl, SYMBOL_UP);
				lv_obj_set_opa_scale(lbl, LV_OPA_50);
				lv_obj_align(lbl, NULL, LV_ALIGN_CENTER, 0, 0);
			}
		}

		// Next button -> exit read mode.
		if (fm_image_next_btn)
		{
			lv_btn_set_action(fm_image_next_btn, LV_BTN_ACTION_CLICK, _fm_image_read_btn_action);
			lv_obj_t *lbl = lv_obj_get_child(fm_image_next_btn, NULL);
			if (lbl)
			{
				lv_label_set_text(lbl, "ⓝ");
				lv_obj_set_opa_scale(lbl, LV_OPA_50);
				lv_obj_align(lbl, NULL, LV_ALIGN_CENTER, 0, 0);
			}
		}

		// Create previous image button.
		if (!fm_image_read_prev_btn && fm_image_controls)
		{
			fm_image_read_prev_btn = lv_btn_create(fm_image_controls, NULL);
			lv_btn_set_style(fm_image_read_prev_btn, LV_BTN_STYLE_REL, &btn_custom_rel);
			lv_btn_set_style(fm_image_read_prev_btn, LV_BTN_STYLE_PR, &btn_custom_pr2);
			lv_obj_set_size(fm_image_read_prev_btn, LV_DPI * 3 / 5, LV_DPI * 3 / 5);
			lv_btn_set_layout(fm_image_read_prev_btn, LV_LAYOUT_OFF);
			lv_obj_align(fm_image_read_prev_btn, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -LV_DPI / 5, -LV_DPI / 5);
			lv_btn_set_action(fm_image_read_prev_btn, LV_BTN_ACTION_CLICK, _fm_image_prev_btn_action);

			fm_image_read_prev_lbl = lv_label_create(fm_image_read_prev_btn, NULL);
			lv_obj_set_opa_scale_enable(fm_image_read_prev_lbl, true);
			lv_obj_set_opa_scale(fm_image_read_prev_lbl, LV_OPA_50);
			lv_label_set_text(fm_image_read_prev_lbl, SYMBOL_DOWN);
			lv_obj_align(fm_image_read_prev_lbl, NULL, LV_ALIGN_CENTER, 0, 0);
		}
		else if (fm_image_read_prev_btn)
			lv_obj_set_hidden(fm_image_read_prev_btn, false);
	}
	else
	{
		nyx_vol_down_action = NULL;
		nyx_vol_up_action   = NULL;
		nyx_jc_plus_action = _fm_view_dummy;
		nyx_jc_minus_action = _fm_view_dummy;
		nyx_jc_r3_action = _fm_view_dummy;
		nyx_jc_dpad_action = NULL;
		nyx_jc_a_action = NULL;
		nyx_jc_x_action = NULL;
		nyx_jc_y_action = NULL;
		nyx_jc_l_action = _fm_image_rotate_left_input;
		nyx_jc_r_action = _fm_image_rotate_right_input;
		nyx_jc_zl_action = _fm_image_previous_input;
		nyx_jc_zr_action = _fm_image_next_input;
		nyx_jc_b_action = _fm_image_b_action;

		if (fm_image_read_prev_btn) lv_obj_set_hidden(fm_image_read_prev_btn, true);

		// Restore normal controls.
		if (fm_image_rotate_btn) lv_obj_set_hidden(fm_image_rotate_btn, false);
		if (fm_image_prev_btn) lv_obj_set_hidden(fm_image_prev_btn, false);
		if (fm_image_read_btn) lv_obj_set_hidden(fm_image_read_btn, false);
		if (fm_image_close_btn) lv_obj_set_hidden(fm_image_close_btn, false);
		if (fm_image_next_btn) lv_obj_set_hidden(fm_image_next_btn, false);

		// Restore physical button hints.
		if (fm_image_hint_bb_bg) lv_obj_set_hidden(fm_image_hint_bb_bg, false);
		if (fm_image_hint_bb_label) lv_obj_set_hidden(fm_image_hint_bb_label, false);
		if (fm_image_hint_zlrb_bg) lv_obj_set_hidden(fm_image_hint_zlrb_bg, false);
		if (fm_image_hint_zlrb_label) lv_obj_set_hidden(fm_image_hint_zlrb_label, false);
		if (fm_image_hint_lrb_bg) lv_obj_set_hidden(fm_image_hint_lrb_bg, false);
		if (fm_image_hint_lrb_label) lv_obj_set_hidden(fm_image_hint_lrb_label, false);

		// Restore close button.
		if (fm_image_close_btn)
		{
			lv_btn_set_action(fm_image_close_btn, LV_BTN_ACTION_CLICK, _fm_image_close_btn_action);
			lv_obj_t *lbl = lv_obj_get_child(fm_image_close_btn, NULL);
			if (lbl)
			{
				lv_label_set_text(lbl, SYMBOL_CLOSE);
				lv_obj_set_opa_scale(lbl, LV_OPA_20);
				lv_obj_align(lbl, NULL, LV_ALIGN_CENTER, 0, 0);
			}
		}

		// Restore next image button.
		if (fm_image_next_btn)
		{
			lv_btn_set_action(fm_image_next_btn, LV_BTN_ACTION_CLICK, _fm_image_next_btn_action);
			lv_obj_t *lbl = lv_obj_get_child(fm_image_next_btn, NULL);
			if (lbl)
			{
				lv_label_set_text(lbl, SYMBOL_RIGHT);
				lv_obj_set_opa_scale(lbl, LV_OPA_20);
				lv_obj_align(lbl, NULL, LV_ALIGN_CENTER, 0, 0);
			}
		}
	}
}

static lv_res_t _fm_image_read_btn_action(lv_obj_t *btn)
{
	if (!fm_image_read_mode)
	{
		_fm_image_rotate_left();
		_fm_image_set_read_mode(true);
	}
	else
	{
		_fm_image_rotate_right();
		_fm_image_set_read_mode(false);
	}

	return LV_RES_OK;
}

static void _fm_image_hide_controls(void)
{
	if (fm_image_controls)
		lv_obj_set_hidden(fm_image_controls, true);
}

static void _fm_image_previous_input(void)
{
	_fm_image_hide_controls();
	_fm_image_previous();
}

static void _fm_image_next_input(void)
{
	_fm_image_hide_controls();
	_fm_image_next();
}

static void _fm_image_rotate_left_input(void)
{
	_fm_image_hide_controls();
	_fm_image_rotate_left();
}

static void _fm_image_rotate_right_input(void)
{
	_fm_image_hide_controls();
	_fm_image_rotate_right();
}

static lv_res_t _fm_image_show_controls_action(lv_obj_t *obj)
{
	if (fm_image_read_mode)
		return LV_RES_OK;

	if (fm_image_controls)
		lv_obj_set_hidden(fm_image_controls, !lv_obj_get_hidden(fm_image_controls));

	return LV_RES_OK;
}

static inline u32 _fm_image_bilinear(const u32 *src, u32 src_w, u32 src_h, u32 x_fp, u32 y_fp)
{
	u32 x0 = x_fp >> 16;
	u32 y0 = y_fp >> 16;
	u32 fx = (x_fp >> 8) & 0xFF;
	u32 fy = (y_fp >> 8) & 0xFF;

	if (x0 >= src_w)
		x0 = src_w - 1;
	if (y0 >= src_h)
		y0 = src_h - 1;

	u32 x1 = x0 + 1;
	u32 y1 = y0 + 1;

	if (x1 >= src_w)
		x1 = x0;
	if (y1 >= src_h)
		y1 = y0;

	const u8 *p00 = (const u8 *)&src[y0 * src_w + x0];
	const u8 *p10 = (const u8 *)&src[y0 * src_w + x1];
	const u8 *p01 = (const u8 *)&src[y1 * src_w + x0];
	const u8 *p11 = (const u8 *)&src[y1 * src_w + x1];

	u32 out = 0;
	u8 *d = (u8 *)&out;

	for (u32 c = 0; c < 4; c++)
	{
		u32 top = p00[c] * (256 - fx) + p10[c] * fx;
		u32 bottom = p01[c] * (256 - fx) + p11[c] * fx;
		d[c] = (u8)((top * (256 - fy) + bottom * fy + 32768) >> 16);
	}

	return out;
}

static lv_img_dsc_t *_fm_image_render_from(lv_img_dsc_t *original, u32 rotation)
{
	if (!original)
		return NULL;

	u32 src_w = original->header.w;
	u32 src_h = original->header.h;

	if (!src_w || !src_h)
		return NULL;

	rotation &= 3;

	u32 rot_w = (rotation & 1) ? src_h : src_w;
	u32 rot_h = (rotation & 1) ? src_w : src_h;
	u32 dst_w = rot_w;
	u32 dst_h = rot_h;

	if (rot_w > LV_HOR_RES || rot_h > LV_VER_RES)
	{
		u64 scale_w = ((u64)LV_HOR_RES << 16) / rot_w;
		u64 scale_h = ((u64)LV_VER_RES << 16) / rot_h;
		u64 scale = scale_w < scale_h ? scale_w : scale_h;

		dst_w = ((u64)rot_w * scale) >> 16;
		dst_h = ((u64)rot_h * scale) >> 16;

		if (!dst_w)
			dst_w = 1;
		if (!dst_h)
			dst_h = 1;
	}

	u32 data_size = dst_w * dst_h * sizeof(u32);
	u32 alloc_size = sizeof(lv_img_dsc_t) + 0x10 + data_size;
	u8 *output = malloc(alloc_size);

	if (!output)
		return NULL;

	lv_img_dsc_t *img_desc = (lv_img_dsc_t *)output;
	uptr offset = ALIGN((uptr)output + sizeof(lv_img_dsc_t), 0x10);
	u32 *src = (u32 *)original->data;
	u32 *dst = (u32 *)offset;

	img_desc->header.always_zero = 0;
	img_desc->header.w = dst_w;
	img_desc->header.h = dst_h;
	img_desc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
	img_desc->data_size = data_size;
	img_desc->data = (u8 *)dst;

	bool scaling = dst_w != rot_w || dst_h != rot_h;

	if (rotation == 0 && !scaling)
	{
		memcpy(dst, src, data_size);
		return img_desc;
	}

	if (!scaling)
	{
		if (rotation == 1)
		{
			for (u32 y = 0; y < dst_h; y++)
			{
				u32 *d = dst + y * dst_w;
				u32 sx = y;

				for (u32 x = 0; x < dst_w; x++)
					d[x] = src[(src_h - 1 - x) * src_w + sx];
			}
		}
		else if (rotation == 2)
		{
			u32 pixels = src_w * src_h;

			for (u32 i = 0; i < pixels; i++)
				dst[i] = src[pixels - 1 - i];
		}
		else
		{
			for (u32 y = 0; y < dst_h; y++)
			{
				u32 *d = dst + y * dst_w;
				u32 sx = src_w - 1 - y;

				for (u32 x = 0; x < dst_w; x++)
					d[x] = src[x * src_w + sx];
			}
		}

		return img_desc;
	}

	u32 rx_step = dst_w > 1 && rot_w > 1 ? (u32)(((u64)(rot_w - 1) << 16) / (dst_w - 1)) : 0;
	u32 ry_step = dst_h > 1 && rot_h > 1 ? (u32)(((u64)(rot_h - 1) << 16) / (dst_h - 1)) : 0;
	u32 ry_fp = 0;

	if (rotation == 0)
	{
		for (u32 y = 0; y < dst_h; y++)
		{
			if (y == dst_h - 1 && rot_h > 1)
				ry_fp = (rot_h - 1) << 16;

			u32 rx_fp = 0;
			u32 *d = dst + y * dst_w;

			for (u32 x = 0; x < dst_w; x++)
			{
				if (x == dst_w - 1 && rot_w > 1)
					rx_fp = (rot_w - 1) << 16;

				d[x] = _fm_image_bilinear(src, src_w, src_h, rx_fp, ry_fp);
				rx_fp += rx_step;
			}

			ry_fp += ry_step;
		}
	}
	else if (rotation == 1)
	{
		u32 max_y = (src_h - 1) << 16;

		for (u32 y = 0; y < dst_h; y++)
		{
			if (y == dst_h - 1 && rot_h > 1)
				ry_fp = (rot_h - 1) << 16;

			u32 rx_fp = 0;
			u32 *d = dst + y * dst_w;

			for (u32 x = 0; x < dst_w; x++)
			{
				if (x == dst_w - 1 && rot_w > 1)
					rx_fp = (rot_w - 1) << 16;

				d[x] = _fm_image_bilinear(src, src_w, src_h, ry_fp, max_y - rx_fp);
				rx_fp += rx_step;
			}

			ry_fp += ry_step;
		}
	}
	else if (rotation == 2)
	{
		u32 max_x = (src_w - 1) << 16;
		u32 max_y = (src_h - 1) << 16;

		for (u32 y = 0; y < dst_h; y++)
		{
			if (y == dst_h - 1 && rot_h > 1)
				ry_fp = (rot_h - 1) << 16;

			u32 rx_fp = 0;
			u32 *d = dst + y * dst_w;

			for (u32 x = 0; x < dst_w; x++)
			{
				if (x == dst_w - 1 && rot_w > 1)
					rx_fp = (rot_w - 1) << 16;

				d[x] = _fm_image_bilinear(src, src_w, src_h, max_x - rx_fp, max_y - ry_fp);
				rx_fp += rx_step;
			}

			ry_fp += ry_step;
		}
	}
	else
	{
		u32 max_x = (src_w - 1) << 16;

		for (u32 y = 0; y < dst_h; y++)
		{
			if (y == dst_h - 1 && rot_h > 1)
				ry_fp = (rot_h - 1) << 16;

			u32 rx_fp = 0;
			u32 *d = dst + y * dst_w;

			for (u32 x = 0; x < dst_w; x++)
			{
				if (x == dst_w - 1 && rot_w > 1)
					rx_fp = (rot_w - 1) << 16;

				d[x] = _fm_image_bilinear(src, src_w, src_h, max_x - ry_fp, rx_fp);
				rx_fp += rx_step;
			}

			ry_fp += ry_step;
		}
	}

	return img_desc;
}

static lv_img_dsc_t *_fm_image_render(u32 rotation)
{
	return _fm_image_render_from(
		fm_image_original,
		rotation);
}

static void _fm_image_update(void)
{
	if (!fm_image_original || !fm_image_obj)
		return;

	lv_img_dsc_t *new_data =
		_fm_image_render(fm_image_rotation);

	if (!new_data)
		return;

	lv_img_dsc_t *old_data = fm_image_data;

	fm_image_data = new_data;

	lv_img_set_src(fm_image_obj, fm_image_data);
	lv_obj_align(fm_image_obj, NULL, fm_image_read_mode ? LV_ALIGN_IN_LEFT_MID : LV_ALIGN_CENTER, 0, 0);

	lv_obj_invalidate(fm_image_obj);

	if (old_data)
		free(old_data);
}

static void _fm_image_rotate(bool clockwise)
{
	if (!fm_image_original || !fm_image_obj)
		return;

	if (clockwise)
		fm_image_rotation = (fm_image_rotation + 1) & 3;
	else
		fm_image_rotation = (fm_image_rotation + 3) & 3;

	_fm_image_update();

	if (fm_image_prev_data)
	{
		free(fm_image_prev_data);
		fm_image_prev_data = NULL;
	}

	if (fm_image_next_data)
	{
		free(fm_image_next_data);
		fm_image_next_data = NULL;
	}

	fm_image_prev_name[0] = 0;
	fm_image_next_name[0] = 0;

	_fm_image_schedule_cache();
}

static void _fm_image_rotate_left(void)
{
	_fm_image_rotate(false);
}

static void _fm_image_rotate_right(void)
{
	_fm_image_rotate(true);
}

static lv_img_dsc_t *_fm_image_load(const char *name)
{
	char path[FM_PATH_SIZE];

	if (!_fm_join(path, fm.cwd, name))
		return NULL;

	if (_fm_ext_is(name, "bmp"))
		return bmp_to_lvimg_obj(path);

	if (_fm_ext_is(name, "png"))
		return png_to_lvimg_obj(path);

	if (_fm_ext_is(name, "jpg") ||
		_fm_ext_is(name, "jpeg"))
		return jpg_to_lvimg_obj(path);

	return NULL;
}

static s32 _fm_image_find_index(const char *name)
{
	if (!name)
		return -1;

	for (u32 i = 0; i < fm_entry_count; i++)
	{
		if (!fm_entries[i].is_dir &&
			!strcmp(fm_entries[i].name, name))
			return (s32)i;
	}

	return -1;
}

static s32 _fm_image_find_neighbor(s32 current, bool next)
{
	if (current < 0 || !fm_entry_count)
		return -1;

	s32 idx = current;

	for (u32 count = 0; count < fm_entry_count; count++)
	{
		if (next)
		{
			idx++;

			if (idx >= (s32)fm_entry_count)
				idx = 0;
		}
		else
		{
			idx--;

			if (idx < 0)
				idx = (s32)fm_entry_count - 1;
		}

		if (idx == current)
			break;

		if (fm_entries[idx].is_dir)
			continue;

		if (!_fm_is_image(fm_entries[idx].name))
			continue;

		return idx;
	}

	return -1;
}

static void _fm_image_build_cache(void)
{
	if (!fm_image_win || !fm_image_obj || !fm_image_original)
		return;

	s32 current = _fm_image_find_index(fm.sel);
	if (current < 0)
		return;

	s32 prev = _fm_image_find_neighbor(current, false);
	s32 next = _fm_image_find_neighbor(current, true);

	if (next >= 0)
	{
		const char *name = fm_entries[next].name;
		bool same = fm_image_next_original && !strcmp(fm_image_next_name, name);

		if (!same)
		{
			if (fm_image_next_data)
			{
				free(fm_image_next_data);
				fm_image_next_data = NULL;
			}

			if (fm_image_next_original)
			{
				free(fm_image_next_original);
				fm_image_next_original = NULL;
			}

			fm_image_next_name[0] = 0;
			fm_image_next_original = _fm_image_load(name);

			if (fm_image_next_original)
				strcpy(fm_image_next_name, name);
		}

		if (fm_image_next_original && !fm_image_next_data)
		{
			fm_image_next_data = _fm_image_render_from(fm_image_next_original, fm_image_rotation);

			if (!fm_image_next_data)
			{
				free(fm_image_next_original);
				fm_image_next_original = NULL;
				fm_image_next_name[0] = 0;
			}
		}
	}
	else
	{
		if (fm_image_next_data)
		{
			free(fm_image_next_data);
			fm_image_next_data = NULL;
		}

		if (fm_image_next_original)
		{
			free(fm_image_next_original);
			fm_image_next_original = NULL;
		}

		fm_image_next_name[0] = 0;
	}

	if (!fm_image_win || !fm_image_obj || !fm_image_original)
		return;

	if (prev >= 0 && prev != next)
	{
		const char *name = fm_entries[prev].name;
		bool same = fm_image_prev_original && !strcmp(fm_image_prev_name, name);

		if (!same)
		{
			if (fm_image_prev_data)
			{
				free(fm_image_prev_data);
				fm_image_prev_data = NULL;
			}

			if (fm_image_prev_original)
			{
				free(fm_image_prev_original);
				fm_image_prev_original = NULL;
			}

			fm_image_prev_name[0] = 0;
			fm_image_prev_original = _fm_image_load(name);

			if (fm_image_prev_original)
				strcpy(fm_image_prev_name, name);
		}

		if (fm_image_prev_original && !fm_image_prev_data)
		{
			fm_image_prev_data = _fm_image_render_from(fm_image_prev_original, fm_image_rotation);

			if (!fm_image_prev_data)
			{
				free(fm_image_prev_original);
				fm_image_prev_original = NULL;
				fm_image_prev_name[0] = 0;
			}
		}
	}
	else
	{
		if (fm_image_prev_data)
		{
			free(fm_image_prev_data);
			fm_image_prev_data = NULL;
		}

		if (fm_image_prev_original)
		{
			free(fm_image_prev_original);
			fm_image_prev_original = NULL;
		}

		fm_image_prev_name[0] = 0;
	}
}

static void _fm_image_cache_task_cb(void *param)
{
	u32 generation =
		(u32)(uptr)param;

	// This task is one-shot.
	fm_image_cache_task = NULL;

	// Viewer was closed or another cache request replaced this one.
	if (!fm_image_win ||
		!fm_image_obj ||
		generation != fm_image_cache_generation)
		return;

	_fm_image_build_cache();
}

static void _fm_image_schedule_cache(void)
{
	if (fm_image_cache_task)
		return;

	fm_image_cache_generation++;

	if (!fm_image_win || !fm_image_obj)
		return;

	fm_image_cache_task = lv_task_create(_fm_image_cache_task_cb, 1, LV_TASK_PRIO_MID, (void *)(uptr)fm_image_cache_generation);

	if (fm_image_cache_task)
		lv_task_once(fm_image_cache_task);
}

static void _fm_image_switch(bool next)
{
	if (!fm_image_obj || fm_image_switching)
		return;

	fm_image_switching = true;

	s32 current = _fm_image_find_index(fm.sel);
	if (current < 0)
		goto out;

	s32 target = _fm_image_find_neighbor(current, next);
	if (target < 0)
		goto out;

	const char *target_name = fm_entries[target].name;
	lv_img_dsc_t *new_original = NULL;
	lv_img_dsc_t *new_data = NULL;

	if (next)
	{
		if (fm_image_next_original && fm_image_next_data && !strcmp(fm_image_next_name, target_name))
		{
			new_original = fm_image_next_original;
			new_data = fm_image_next_data;
			fm_image_next_original = NULL;
			fm_image_next_data = NULL;
			fm_image_next_name[0] = 0;
		}
	}
	else
	{
		if (fm_image_prev_original && fm_image_prev_data && !strcmp(fm_image_prev_name, target_name))
		{
			new_original = fm_image_prev_original;
			new_data = fm_image_prev_data;
			fm_image_prev_original = NULL;
			fm_image_prev_data = NULL;
			fm_image_prev_name[0] = 0;
		}
	}

	if (!new_original)
	{
		new_original = _fm_image_load(target_name);
		if (!new_original)
			goto out;

		new_data = _fm_image_render_from(new_original, fm_image_rotation);
		if (!new_data)
		{
			free(new_original);
			goto out;
		}
	}

	lv_img_dsc_t *old_original = fm_image_original;
	lv_img_dsc_t *old_data = fm_image_data;
	char old_name[FM_NAME_SIZE];
	strcpy(old_name, fm.sel);

	fm_image_original = new_original;
	fm_image_data = new_data;
	strcpy(fm.sel, target_name);
	fm.sel_is_dir = false;
	fm.has_sel = true;

	lv_img_set_src(fm_image_obj, fm_image_data);
	lv_obj_align(fm_image_obj, NULL, fm_image_read_mode ? LV_ALIGN_IN_LEFT_MID : LV_ALIGN_CENTER, 0, 0);
	lv_obj_invalidate(fm_image_obj);

	if (next)
	{
		if (fm_image_prev_data)
			free(fm_image_prev_data);

		if (fm_image_prev_original)
			free(fm_image_prev_original);

		fm_image_prev_original = old_original;
		fm_image_prev_data = old_data;
		strcpy(fm_image_prev_name, old_name);

		if (fm_image_next_data)
		{
			free(fm_image_next_data);
			fm_image_next_data = NULL;
		}

		if (fm_image_next_original)
		{
			free(fm_image_next_original);
			fm_image_next_original = NULL;
		}

		fm_image_next_name[0] = 0;
	}
	else
	{
		if (fm_image_next_data)
			free(fm_image_next_data);

		if (fm_image_next_original)
			free(fm_image_next_original);

		fm_image_next_original = old_original;
		fm_image_next_data = old_data;
		strcpy(fm_image_next_name, old_name);

		if (fm_image_prev_data)
		{
			free(fm_image_prev_data);
			fm_image_prev_data = NULL;
		}

		if (fm_image_prev_original)
		{
			free(fm_image_prev_original);
			fm_image_prev_original = NULL;
		}

		fm_image_prev_name[0] = 0;
	}

	fm_image_switching = false;
	_fm_image_schedule_cache();
	return;

out:
	fm_image_switching = false;
}

static void _fm_image_previous(void)
{
	_fm_image_switch(false);
}

static void _fm_image_next(void)
{
	_fm_image_switch(true);
}

static void _fm_view_image(void)
{
	char path[FM_PATH_SIZE];

	if (!_fm_join(path, fm.cwd, fm.sel))
	{
		_fm_msg("#FFDD00 경로가 너무 깁니다!#");
		return;
	}

	// Load original image. Keep this unchanged while the viewer is open.
	if (_fm_ext_is(fm.sel, "bmp"))
		fm_image_original = bmp_to_lvimg_obj(path);
	else if (_fm_ext_is(fm.sel, "png"))
		fm_image_original = png_to_lvimg_obj(path);
	else if (_fm_ext_is(fm.sel, "jpg") ||
		_fm_ext_is(fm.sel, "jpeg"))
		fm_image_original = jpg_to_lvimg_obj(path);
	else
		fm_image_original = NULL;

	if (!fm_image_original)
	{
		_fm_msg("#FFDD00 이미지를 열 수 없습니다!#");
		return;
	}

	if (fm_image_prev_data)
	{
		free(fm_image_prev_data);
		fm_image_prev_data = NULL;
	}

	if (fm_image_next_data)
	{
		free(fm_image_next_data);
		fm_image_next_data = NULL;
	}

	if (fm_image_prev_original)
	{
		free(fm_image_prev_original);
		fm_image_prev_original = NULL;
	}

	if (fm_image_next_original)
	{
		free(fm_image_next_original);
		fm_image_next_original = NULL;
	}

	fm_image_prev_name[0] = 0;
	fm_image_next_name[0] = 0;

	fm_image_rotation = 0;
	fm_image_read_mode = false;

	fm_image_close_btn = NULL;
	fm_image_prev_btn = NULL;
	fm_image_next_btn = NULL;
	fm_image_rotate_btn = NULL;
	fm_image_read_btn = NULL;

	fm_image_prev_lbl = NULL;
	fm_image_rotate_lbl = NULL;
	fm_image_read_lbl = NULL;

	fm_image_read_prev_btn = NULL;
	fm_image_read_prev_lbl = NULL;

	fm_image_hint_bb_bg = NULL;
	fm_image_hint_bb_label = NULL;
	fm_image_hint_zlrb_bg = NULL;
	fm_image_hint_zlrb_label = NULL;
	fm_image_hint_lrb_bg = NULL;
	fm_image_hint_lrb_label = NULL;

	// Create initial fitted image from original.
	fm_image_data = _fm_image_render(0);

	if (!fm_image_data)
	{
		free(fm_image_original);
		fm_image_original = NULL;

		_fm_msg("#FFDD00 이미지를 표시할 수 없습니다!#");
		return;
	}

	fm_image_win = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_size(fm_image_win, LV_HOR_RES, LV_VER_RES);
	lv_obj_set_pos(fm_image_win, 0, 0);
	lv_obj_set_style(fm_image_win, &lv_style_plain);
	lv_obj_set_top(fm_image_win, true);

	fm_image_obj = lv_img_create(fm_image_win, NULL);
	lv_img_set_src(fm_image_obj, fm_image_data);
	lv_obj_align(fm_image_obj, NULL, fm_image_read_mode ? LV_ALIGN_IN_RIGHT_MID : LV_ALIGN_CENTER, 0, 0);

	// Invisible full-screen touch layer. First touch shows image viewer controls.
	lv_obj_t *touch_area = lv_btn_create(fm_image_win, NULL);
	lv_obj_set_size(touch_area, LV_HOR_RES, LV_VER_RES);
	lv_obj_set_pos(touch_area, 0, 0);
	lv_obj_set_opa_scale_enable(touch_area, true);
	lv_obj_set_opa_scale(touch_area, LV_OPA_0);
	lv_btn_set_action(touch_area, LV_BTN_ACTION_CLICK, _fm_image_show_controls_action);

	// Parent object for all touch controls and hints.
	fm_image_controls = lv_obj_create(fm_image_win, NULL);
	lv_obj_set_size(fm_image_controls, LV_HOR_RES, LV_VER_RES);
	lv_obj_set_pos(fm_image_controls, 0, 0);
	lv_obj_set_style(fm_image_controls, &lv_style_transp);
	lv_obj_set_click(fm_image_controls, false);

	s32 image_idx = _fm_image_find_index(fm.sel);
	bool has_other_image =
		image_idx >= 0 &&
		_fm_image_find_neighbor(image_idx, true) >= 0;

	// Close image button.
	fm_image_close_btn = lv_btn_create(fm_image_controls, NULL);
	lv_obj_t *close_btn = fm_image_close_btn;
	lv_btn_set_style(close_btn, LV_BTN_STYLE_REL, &btn_custom_rel);
	lv_btn_set_style(close_btn, LV_BTN_STYLE_PR, &btn_custom_pr2);
	lv_obj_set_size(close_btn, LV_DPI * 3 / 5, LV_DPI * 3 / 5);
	lv_btn_set_layout(close_btn, LV_LAYOUT_OFF);
	lv_obj_align(close_btn, NULL, LV_ALIGN_IN_TOP_RIGHT, -LV_DPI / 5, LV_DPI / 5);
	lv_btn_set_action(close_btn, LV_BTN_ACTION_CLICK, _fm_image_close_btn_action);
	lv_obj_t *close_lbl = lv_label_create(close_btn, NULL);
	lv_obj_set_opa_scale_enable(close_lbl, true);
	lv_obj_set_opa_scale(close_lbl, LV_OPA_20);
	lv_label_set_text(close_lbl, SYMBOL_CLOSE);
	lv_obj_align(close_lbl, NULL, LV_ALIGN_CENTER, 0, 0);

	if (has_other_image)
	{
		// Previous image button.
		fm_image_prev_btn = lv_btn_create(fm_image_controls, NULL);
		lv_obj_t *prev_btn = fm_image_prev_btn;
		lv_btn_set_style(prev_btn, LV_BTN_STYLE_REL, &btn_custom_rel);
		lv_btn_set_style(prev_btn, LV_BTN_STYLE_PR, &btn_custom_pr2);
		lv_obj_set_size(prev_btn, LV_DPI * 3 / 5, LV_DPI * 3 / 5);
		lv_btn_set_layout(prev_btn, LV_LAYOUT_OFF);
		lv_obj_align(prev_btn, NULL, LV_ALIGN_IN_LEFT_MID, LV_DPI / 5, 0);
		lv_btn_set_action(prev_btn, LV_BTN_ACTION_CLICK, _fm_image_prev_btn_action);

		fm_image_prev_lbl = lv_label_create(prev_btn, NULL);
		lv_obj_t *prev_lbl = fm_image_prev_lbl;
		lv_obj_set_opa_scale_enable(prev_lbl, true);
		lv_obj_set_opa_scale(prev_lbl, LV_OPA_20);
		lv_label_set_text(prev_lbl, SYMBOL_LEFT);
		lv_obj_align(prev_lbl, NULL, LV_ALIGN_CENTER, 0, 0);

		// Next image button.
		fm_image_next_btn = lv_btn_create(fm_image_controls, NULL);
		lv_obj_t *next_btn = fm_image_next_btn;
		lv_btn_set_style(next_btn, LV_BTN_STYLE_REL, &btn_custom_rel);
		lv_btn_set_style(next_btn, LV_BTN_STYLE_PR, &btn_custom_pr2);
		lv_obj_set_size(next_btn, LV_DPI * 3 / 5, LV_DPI * 3 / 5);
		lv_btn_set_layout(next_btn, LV_LAYOUT_OFF);
		lv_obj_align(next_btn, NULL, LV_ALIGN_IN_RIGHT_MID, -LV_DPI / 5, 0);
		lv_btn_set_action(next_btn, LV_BTN_ACTION_CLICK, _fm_image_next_btn_action);

		lv_obj_t *next_lbl = lv_label_create(next_btn, NULL);
		lv_obj_set_opa_scale_enable(next_lbl, true);
		lv_obj_set_opa_scale(next_lbl, LV_OPA_20);
		lv_label_set_text(next_lbl, SYMBOL_RIGHT);
		lv_obj_align(next_lbl, NULL, LV_ALIGN_CENTER, 0, 0);

		// Read mode button.
		fm_image_read_btn = lv_btn_create(fm_image_controls, NULL);
		lv_btn_set_style(fm_image_read_btn, LV_BTN_STYLE_REL, &btn_custom_rel);
		lv_btn_set_style(fm_image_read_btn, LV_BTN_STYLE_PR, &btn_custom_pr2);
		lv_obj_set_size(fm_image_read_btn, LV_DPI * 3 / 5, LV_DPI * 3 / 5);
		lv_btn_set_layout(fm_image_read_btn, LV_LAYOUT_OFF);
		lv_obj_align(fm_image_read_btn, NULL, LV_ALIGN_IN_BOTTOM_LEFT, LV_DPI / 5, -LV_DPI / 5);
		lv_btn_set_action(fm_image_read_btn, LV_BTN_ACTION_CLICK, _fm_image_read_btn_action);

		fm_image_read_lbl = lv_label_create( fm_image_read_btn, NULL);
		lv_obj_set_opa_scale_enable(fm_image_read_lbl, true);
		lv_obj_set_opa_scale(fm_image_read_lbl, LV_OPA_20);
		lv_label_set_text(fm_image_read_lbl, SYMBOL_LIST);
		lv_obj_align(fm_image_read_lbl, NULL, LV_ALIGN_CENTER, 0, 0);
	}

	// Rotate image button.
	fm_image_rotate_btn = lv_btn_create(fm_image_controls, NULL);
	lv_obj_t *rotate_btn = fm_image_rotate_btn;
	lv_btn_set_style(rotate_btn, LV_BTN_STYLE_REL, &btn_custom_rel);
	lv_btn_set_style(rotate_btn, LV_BTN_STYLE_PR, &btn_custom_pr2);
	lv_obj_set_size(rotate_btn, LV_DPI * 3 / 5, LV_DPI * 3 / 5);
	lv_btn_set_layout(rotate_btn, LV_LAYOUT_OFF);
	lv_obj_align(rotate_btn, NULL, LV_ALIGN_IN_TOP_LEFT, LV_DPI / 5, LV_DPI / 5);
	lv_btn_set_action(rotate_btn, LV_BTN_ACTION_CLICK, _fm_image_rotate_btn_action);
	fm_image_rotate_lbl = lv_label_create(rotate_btn, NULL);
	lv_obj_t *rotate_lbl = fm_image_rotate_lbl;
	lv_obj_set_opa_scale_enable(rotate_lbl, true);
	lv_obj_set_opa_scale(rotate_lbl, LV_OPA_20);
	lv_label_set_text(rotate_lbl, SYMBOL_REBOOT);
	lv_obj_align(rotate_lbl, NULL, LV_ALIGN_CENTER, 0, 0);

	// B button
	lv_obj_t *iv_bb_bg = _fm_create_hint_bg(fm_image_controls, "ⓝ");
	fm_image_hint_bb_bg = iv_bb_bg;
	lv_obj_align(iv_bb_bg, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -80, -LV_DPI / 5);
	lv_obj_t *iv_bb_label = _fm_create_hint_label(fm_image_controls, "#C7AD59 ⓑ#  뒤로");
	fm_image_hint_bb_label = iv_bb_label;
	lv_obj_align(iv_bb_label, iv_bb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	lv_obj_t *hint_anchor = iv_bb_bg;

	if (has_other_image)
	{
		// ZL/ZR
		lv_obj_t *iv_zlrb_bg = _fm_create_hint_bg(fm_image_controls, "ⓘ ⓘ");
		fm_image_hint_zlrb_bg = iv_zlrb_bg;
		lv_obj_align(iv_zlrb_bg, iv_bb_bg, LV_ALIGN_OUT_LEFT_MID, -115, 0);

		lv_obj_t *iv_zlrb_label = _fm_create_hint_label(fm_image_controls, "#4B4B4B ⓖⓗ#  이전/다음");
		fm_image_hint_zlrb_label = iv_zlrb_label;
		lv_obj_align(iv_zlrb_label, iv_zlrb_bg, LV_ALIGN_IN_LEFT_MID, -2, 0);

		hint_anchor = iv_zlrb_bg;
	}

	// L/R
	lv_obj_t *iv_lrb_bg = _fm_create_hint_bg(fm_image_controls, "ⓘ ⓘ");
	fm_image_hint_lrb_bg = iv_lrb_bg;
	lv_obj_align(iv_lrb_bg, hint_anchor, LV_ALIGN_OUT_LEFT_MID, -85, 0);
	lv_obj_t *iv_lrb_label = _fm_create_hint_label(fm_image_controls, "#4B4B4B ⓔⓕ#  회전");
	fm_image_hint_lrb_label = iv_lrb_label;
	lv_obj_align(iv_lrb_label, iv_lrb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	// Start with a clean image-only view. Touch the screen once to show controls.
	lv_obj_set_hidden(fm_image_controls, true);

	_fm_clear_input_actions();

	nyx_jc_plus_action  = _fm_view_dummy;
	nyx_jc_minus_action = _fm_view_dummy;

	// L/R: rotate image 90 degrees.
	nyx_jc_l_action = _fm_image_rotate_left_input;
	nyx_jc_r_action = _fm_image_rotate_right_input;

	// ZL/ZR: previous / next image.
	if (has_other_image)
	{
		nyx_jc_zl_action = _fm_image_previous_input;
		nyx_jc_zr_action = _fm_image_next_input;
	}

	nyx_jc_b_action = _fm_image_b_action;

	nyx_jc_dpad_mode = true;

	// Decode previous and next images in advance.
	_fm_image_schedule_cache();
}

static lv_res_t _fm_entry_action(lv_obj_t *btn)
{
	u32 i = lv_obj_get_free_num(btn);

	fm_press_pending = false;

	if (fm_press_fired)
	{
		fm_press_fired = false;
		return LV_RES_OK;
	}

	if (i >= fm_entry_count)
		return LV_RES_OK;

	// First touch: Select the item only. Second touch on the already selected item: Open / execute it.
	if (fm_selected_btn != btn)
	{
		if (fm_last_btn)
			lv_btn_set_state(fm_last_btn, LV_BTN_STATE_REL);

		fm_selected_btn = btn;
		fm_last_btn = btn;

		// Find the actual list index. fm_btns[] may contain ".." at index 0.
		u32 item_count = fm_entry_count;

		if (fm.cwd[1] != 0)
			item_count++;

		for (u32 n = 0; n < item_count; n++)
		{
			if (fm_btns[n] == btn)
			{
				fm_selected_idx = n;
				break;
			}
		}

		lv_btn_set_state(btn, LV_BTN_STATE_TGL_REL);
		lv_list_focus(btn, false);

		_fm_select_btn(btn);
		_fm_update_status();

		return LV_RES_OK;
	}

	// The same selected item was touched again.
	strcpy(fm.sel, fm_entries[i].name);
	fm.sel_is_dir = fm_entries[i].is_dir;
	fm.has_sel = true;

	_fm_update_status();

	if (fm_entries[i].is_dir)
	{
		_fm_enter_dir(fm_entries[i].name);
		return LV_RES_INV;
	}

	if (_fm_is_text(fm.sel))
	{
		_fm_view_text();
		return LV_RES_OK;
	}

	if (_fm_is_image(fm.sel))
	{
		_fm_progress_show("이미지 불러오는 중...");

		fm_pending_task = lv_task_create(
			_fm_image_open_task,
			1,
			LV_TASK_PRIO_HIGH,
			NULL);

		if (!fm_pending_task)
		{
			_fm_progress_close();
			_fm_msg("#FFDD00 메모리 할당 실패!#");
			return LV_RES_OK;
		}

		lv_task_once(fm_pending_task);

		return LV_RES_OK;
	}

	if (_fm_is_payload(fm.cwd, fm.sel))
	{
		_fm_launch_payload();
		return LV_RES_OK;
	}

	return LV_RES_OK;
}

static lv_res_t _fm_updir_action(lv_obj_t *btn)
{
	strcpy(fm_return_sel, _fm_basename(fm.cwd));

	_fm_go_up();
	fm.has_sel = false;
	_fm_refresh();

	return LV_RES_INV;
}

// List browser, D-PAD action.
static lv_obj_t *_fm_add_entry_row(u32 i)
{
	fm_entry_t *e = &fm_entries[i];

	char size[32] = "";
	char date[32];
	const char *icon;

	_fm_date_str(date, e->date, e->time);

	if (!e->is_dir)
		_fm_size_str(size, e->size);

	if (e->is_dir)
		icon = SYMBOL_DIRECTORY;
	else if (_fm_is_archive(e->name))
		icon = SYMBOL_FILE_ARC;
	else if (_fm_is_text(e->name))
		icon = SYMBOL_FILE_ALT;
	else
		icon = SYMBOL_FILE;

	lv_obj_t *btn = _fm_list_add(
		fm.list,
		icon,
		e->name,
		e->is_dir ? NULL : size,
		date,
		_fm_entry_action);

	lv_obj_set_free_num(btn, i);

	return btn;
}

static void _fm_select_btn(lv_obj_t *btn)
{
	if (!btn)
	{
		fm.has_sel = false;
		return;
	}

	u32 idx = lv_obj_get_free_num(btn);

	strcpy(fm.sel, fm_entries[idx].name);
	fm.sel_is_dir = fm_entries[idx].is_dir;
	fm.has_sel = true;
}

static void _fm_dpad(int dir)
{
	u32 item_count = fm_entry_count;
	if (fm.cwd[1] != 0)
		item_count++;

	if (!item_count)
		return;

	if (dir == NYX_DPAD_UP)
	{
		if (fm_selected_idx == 0)
			fm_selected_idx = item_count - 1;
		else
			fm_selected_idx--;
	} else if (dir == NYX_DPAD_DOWN) {
		if (fm_selected_idx + 1 >= item_count)
			fm_selected_idx = 0;
		else
			fm_selected_idx++;
	} else if (dir == NYX_DPAD_LEFT) {
		fm_selected_idx = 0;
	} else if (dir == NYX_DPAD_RIGHT) {
		fm_selected_idx = item_count - 1;
	}
	else
	{
		return;
	}

	if (fm_last_btn)
		lv_btn_set_state(fm_last_btn, LV_BTN_STATE_REL);

	fm_selected_btn = fm_btns[fm_selected_idx];

	lv_btn_set_state(fm_selected_btn, LV_BTN_STATE_TGL_REL);
	lv_list_focus(fm_selected_btn, false);

	fm_last_btn = fm_selected_btn;

	if (fm.cwd[1] != 0 && fm_selected_idx == 0)
	{
		fm.has_sel = false;
	}
	else
	{
		_fm_select_btn(fm_selected_btn);
	}

	_fm_update_status();
}

static int _fm_entry_cmp(const void *a, const void *b)
{
	const fm_entry_t *ea = (const fm_entry_t *)a;
	const fm_entry_t *eb = (const fm_entry_t *)b;

	if (ea->is_dir != eb->is_dir)
		return eb->is_dir - ea->is_dir;

	return strcasecmp(ea->name, eb->name);
}

static void _fm_refresh(void)
{
	lv_list_clean(fm.list);

	memset(fm_btns, 0, sizeof(fm_btns));
	fm_selected_btn = NULL;
	fm_last_btn = NULL;
	fm_selected_idx = 0;

	if (fm_entries)
	{
		free(fm_entries);
		fm_entries = NULL;
	}
	fm_entry_count = 0;

	if (sd_mount())
	{
		lv_win_set_title(fm.win, SYMBOL_DIRECTORY "  SD 카드 초기화 실패!");
		_fm_update_status();
		return;
	}

	fm_entries = malloc(sizeof(fm_entry_t) * FM_MAX_ENTRIES);
	if (!fm_entries)
	{
		lv_win_set_title(fm.win, SYMBOL_DIRECTORY "  메모리 할당 실패!");
		_fm_update_status();
		return;
	}

	DIR dir;
	FILINFO *fno = malloc(sizeof(FILINFO));
	if (!fno)
	{
		free(fm_entries);
		fm_entries = NULL;

		lv_win_set_title(fm.win, SYMBOL_DIRECTORY "  메모리 할당 실패!");
		_fm_update_status();
		return;
	}

	if (f_opendir(&dir, fm.cwd) == FR_OK)
	{
		while (fm_entry_count < FM_MAX_ENTRIES)
		{
			if (f_readdir(&dir, fno) != FR_OK || fno->fname[0] == 0)
				break;

			fm_entry_t *e = &fm_entries[fm_entry_count];
			strcpy(e->name, fno->fname);
			e->is_dir = (fno->fattrib & AM_DIR) != 0;
			e->size = fno->fsize;
			e->date = fno->fdate;
			e->time = fno->ftime;
			fm_entry_count++;
		}
		f_closedir(&dir);
	}

	free(fno);

	qsort(fm_entries, fm_entry_count, sizeof(fm_entry_t), _fm_entry_cmp);
	u32 btn_idx = 0;

	if (fm.cwd[1] != 0)
	{
		fm_btns[btn_idx++] =
			lv_list_add(fm.list, NULL, SYMBOL_UP"  ..", _fm_updir_action);
	}

	for (u32 i = 0; i < fm_entry_count; i++)
		fm_btns[btn_idx++] = _fm_add_entry_row(i);

	// Update the title with the current SD path.
	lv_win_set_file_browser_title(fm.win, fm.cwd);

	u32 sel = (fm.cwd[1] != 0 && fm_entry_count > 0) ? 1 : 0;

	// Restore the previous folder selection when returning to the parent directory.
	if (fm_return_sel[0])
	{
		u32 start = (fm.cwd[1] != 0) ? 1 : 0;

		for (u32 i = 0; i < fm_entry_count; i++)
		{
			if (!strcmp(fm_entries[i].name, fm_return_sel))
			{
				sel = i + start;
				break;
			}
		}

		fm_return_sel[0] = 0;
	}

	fm_selected_idx = sel;
	fm_selected_btn = fm_btns[sel];
	fm_last_btn = fm_selected_btn;

	if (fm_selected_btn)
	{
		lv_btn_set_state(fm_selected_btn, LV_BTN_STATE_TGL_REL);
		lv_list_focus(fm_selected_btn, false);

		if (fm.cwd[1] != 0 && sel == 0)
			fm.has_sel = false;
		else
			_fm_select_btn(fm_selected_btn);
	}
	else
	{
		fm.has_sel = false;
	}

	_fm_update_status();
}

static lv_res_t _fm_kb_ok_action(lv_obj_t *kb)
{
	char name[FM_NAME_SIZE];
	const char *txt = lv_ta_get_text(fm.kb_ta);
	u32 op = fm.kb_op;

	strncpy(name, txt, sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;

	_fm_close_browser_keyboard(kb);

	if (!_fm_name_valid(name))
	{
		_fm_restore_browser_input();
		_fm_msg("#FFDD00 잘못된 이름입니다#");
		return LV_RES_INV;
	}

	if (!sd_mount())
	{
		int res = FR_OK;
		char *path = malloc(FM_PATH_SIZE);

		if (!path)
		{
			_fm_msg("#FFDD00 메모리 할당 실패!#");
			_fm_refresh();
			_fm_restore_browser_input();
			return LV_RES_INV;
		}

		if (op == FM_KB_NEWDIR)
		{
			if (!_fm_join(path, fm.cwd, name))
			{
				free(path);
				_fm_msg("#FFDD00 경로가 너무 깁니다!#");
				_fm_refresh();
				_fm_restore_browser_input();
				return LV_RES_INV;
			}

			res = f_mkdir(path);
		}
		else
		{
			char *npath = malloc(FM_PATH_SIZE);

			if (!npath)
			{
				free(path);
				_fm_msg("#FFDD00 메모리 할당 실패!#");
				_fm_refresh();
				_fm_restore_browser_input();
				return LV_RES_INV;
			}

			if (!_fm_join(path, fm.cwd, fm.sel) || !_fm_join(npath, fm.cwd, name))
			{
				free(npath);
				free(path);

				_fm_msg("#FFDD00 경로가 너무 깁니다!#");
				_fm_refresh();
				_fm_restore_browser_input();
				return LV_RES_INV;
			}

			res = f_rename(path, npath);

			if (res == FR_OK)
			{
				// If the renamed item is currently in the clipboard, update the clipboard path to the new name.
				if (fm.has_clip && !strcmp(fm.clip, path))
				{
					strncpy(fm.clip, npath, sizeof(fm.clip) - 1);
					fm.clip[sizeof(fm.clip) - 1] = 0;
				}

				strncpy(fm.sel, name, sizeof(fm.sel) - 1);
				fm.sel[sizeof(fm.sel) - 1] = 0;
			}

			free(npath);
		}

		free(path);

		if (res != FR_OK)
			_fm_msg("#FFDD00 작업 실패!#");
	}
	else
	{
		_fm_msg("#FFDD00 SD 카드 초기화 실패!#");
	}

	_fm_refresh();
	_fm_restore_browser_input();

	return LV_RES_INV;
}

static void _fm_kb_save_action(void)
{
	if (fm_kb)
		_fm_kb_ok_action(fm_kb);
}

static void _fm_kb_cancel_action(void)
{
	if (fm_kb)
		_fm_kb_close_action(fm_kb);
}

static lv_res_t _fm_kb_close_action(lv_obj_t *kb)
{
	_fm_close_browser_keyboard(kb);
	_fm_restore_browser_input();

	return LV_RES_INV;
}

static void _fm_open_keyboard(const char *title, const char *prefill, u32 op)
{
	fm.kb_op = op;

	lv_obj_t *dark_bg = _fm_create_dark_bg();

	_fm_set_browser_hints_hidden(true);

	lv_obj_t *cont = lv_cont_create(dark_bg, NULL);
	lv_cont_set_fit(cont, false, true);
	lv_obj_set_width(cont, LV_HOR_RES * 7 / 10);

	lv_obj_t *lbl = lv_label_create(cont, NULL);
	lv_label_set_recolor(lbl, true);
	lv_label_set_static_text(lbl, title);
	lv_obj_align(lbl, NULL, LV_ALIGN_IN_TOP_LEFT, LV_DPI / 8, LV_DPI / 8);

	lv_obj_t *ta = lv_ta_create(cont, NULL);
	lv_ta_set_one_line(ta, true);
	lv_ta_set_cursor_type(ta, LV_CURSOR_LINE);
	lv_ta_set_max_length(ta, FM_NAME_SIZE - 1);
	lv_ta_set_text(ta, prefill);
	lv_obj_set_width(ta, LV_HOR_RES * 6 / 10);
	lv_obj_align(ta, lbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 8);

	fm.kb_ta = ta;

	lv_obj_t *kb = _fm_create_keyboard(
		dark_bg,
		ta,
		_fm_kb_ok_action,
		_fm_kb_close_action);

	fm_kb = kb;

	_fm_update_status();

	nyx_jc_dpad_action  = _fm_view_dpad;
	nyx_jc_plus_action  = _fm_kb_save_action;
	nyx_jc_minus_action = _fm_kb_cancel_action;
	nyx_jc_a_action     = _fm_view_a;
	nyx_jc_b_action     = _fm_b_action;
	nyx_jc_x_action     = _fm_view_space_action;
	nyx_jc_y_action     = _fm_view_mode_action;
	nyx_jc_l_action     = _fm_cursor_left;
	nyx_jc_r_action     = _fm_cursor_right;
	nyx_jc_zl_action    = NULL;
	nyx_jc_zr_action    = NULL;

	_fm_setup_keyboard_hints(dark_bg, fm.list, false);

	lv_obj_align(cont, kb, LV_ALIGN_OUT_TOP_MID, 0, -LV_DPI / 4);
	lv_obj_set_top(cont, true);
}

static void _fm_enter_dir_task(void *param)
{
	fm_pending_task = NULL;

	strcpy(fm.cwd, fm_pending_src);

	fm.has_sel = false;

	_fm_refresh();

	_fm_progress_close();
}

static void _fm_enter_dir(const char *name)
{
	strcpy(fm_return_sel, name);

	if (!_fm_join(
		fm_pending_src,
		fm.cwd,
		name))
	{
		_fm_msg("#FFDD00 경로가 너무 깁니다!#");
		return;
	}

	_fm_progress_show("로딩 중...");

	fm_pending_task = lv_task_create(
		_fm_enter_dir_task,
		1,
		LV_TASK_PRIO_HIGH,
		NULL);

	if (!fm_pending_task)
	{
		_fm_progress_close();
		_fm_msg("#FFDD00 메모리 할당 실패!#");
		return;
	}

	lv_task_once(fm_pending_task);
}

static void _fm_image_open_task(void *param)
{
	fm_pending_task = NULL;

	_fm_progress_close();

	_fm_view_image();
}

static void _fm_enter_selected(void)
{
	if (!fm.has_sel)
	{
		if (fm.cwd[1] != 0 && fm_selected_idx == 0)
			_fm_updir_action(NULL);
		return;
	}

	if (fm.sel_is_dir)
	{
		_fm_enter_dir(fm.sel);
		return;
	}

	if (_fm_is_text(fm.sel))
	{
		_fm_view_text();
		return;
	}

	if (_fm_is_image(fm.sel))
	{
		_fm_progress_show("이미지 불러오는 중...");

		fm_pending_task = lv_task_create(
			_fm_image_open_task,
			1,
			LV_TASK_PRIO_HIGH,
			NULL);

		if (!fm_pending_task)
		{
			_fm_progress_close();
			_fm_msg("#FFDD00 메모리 할당 실패!#");
			return;
		}

		lv_task_once(fm_pending_task);

		return;
	}

	if (_fm_is_payload(fm.cwd, fm.sel))
	{
		_fm_launch_payload();
		return;
	}
}

static void _fm_restore_browser_input(void)
{
	nyx_jc_dpad_action  = _fm_dpad;
	nyx_jc_a_action     = _fm_enter_selected;
	nyx_jc_b_action     = _fm_b_action;
	nyx_jc_b_long_action = _fm_b_long_action;

	nyx_jc_plus_action  = (void (*)(void))_fm_newfolder_action;
	nyx_jc_minus_action = (void (*)(void))_fm_delete_action;
	nyx_jc_r3_action    = (void (*)(void))_fm_rename_action;

	nyx_jc_l_action  = NULL;
	nyx_jc_r_action  = NULL;
	nyx_jc_zl_action = NULL;
	nyx_jc_zr_action = NULL;

	nyx_jc_dpad_mode = true;

	_fm_update_status();
}

static void _fm_b_action(void)
{
	if (fm_view_ta)
	{
		if (fm_view_editing)
			lv_ta_del_char(fm_view_ta);
		else
			_fm_view_close(NULL);

		return;
	}

	if (fm.kb_ta)
	{
		lv_ta_del_char(fm.kb_ta);
		return;
	}

	if (fm.cwd[1] != 0)
	{
		_fm_updir_action(NULL);
		return;
	}
}

static void _fm_b_long_action(void)
{
	if (fm_view_ta || fm.kb_ta || fm_mbox_btnm || fm_progress_bg || !close_btn)
		return;

	lv_obj_t *btn = close_btn;
	fm.win = NULL;
	_fm_close(btn);
}

static lv_res_t _fm_newfolder_action(lv_obj_t *btn)
{
	if (fm.kb_ta)
		return LV_RES_OK;

	_fm_open_keyboard(SYMBOL_DIRECTORY"  새 폴더", "", FM_KB_NEWDIR);

	return LV_RES_OK;
}

static lv_res_t _fm_rename_action(lv_obj_t *btn)
{
	if (fm.kb_ta)
		return LV_RES_OK;

	if (!fm.has_sel)
	{
		_fm_msg("#FFDD00 선택된 항목이 없습니다#");
		return LV_RES_OK;
	}

	_fm_open_keyboard(SYMBOL_EDIT"  새 이름", fm.sel, FM_KB_RENAME);

	return LV_RES_OK;
}

static lv_res_t _fm_set_clipboard(bool cut)
{
	if (!fm.has_sel)
	{
		_fm_msg("#FFDD00 선택된 항목이 없습니다#");
		return LV_RES_OK;
	}

	_fm_reset_clipboard_ui();

	if (!_fm_join(fm.clip, fm.cwd, fm.sel))
	{
		_fm_msg("#FFDD00 경로가 너무 깁니다!#");
		return LV_RES_OK;
	}

	fm.clip_cut = cut;
	fm.clip_is_dir = fm.sel_is_dir;
	fm.has_clip = true;

	if (cut)
	{
		// Cut mode: X = paste, Y = cancel.
		lv_label_set_static_text(fm.xb_label, " ⓝ  붙여넣기 ");
		lv_btn_set_action(fm.btn_cut, LV_BTN_ACTION_CLICK, _fm_paste_action);

		lv_label_set_static_text(fm.yb_label, " ⓝ  취소 ");
		lv_btn_set_action(fm.btn_copy, LV_BTN_ACTION_CLICK, _fm_cancel_clipboard_btn_action);

		nyx_jc_x_action = (void (*)(void))_fm_paste_action;
		nyx_jc_y_action = _fm_cancel_clipboard_action;
	}
	else
	{
		// Copy mode: Y = paste, X = cancel.
		lv_label_set_static_text(fm.yb_label, " ⓝ  붙여넣기 ");
		lv_btn_set_action(fm.btn_copy, LV_BTN_ACTION_CLICK, _fm_paste_action);

		lv_label_set_static_text(fm.xb_label, " ⓝ  취소 ");
		lv_btn_set_action(fm.btn_cut, LV_BTN_ACTION_CLICK, _fm_cancel_clipboard_btn_action);

		nyx_jc_y_action = (void (*)(void))_fm_paste_action;
		nyx_jc_x_action = _fm_cancel_clipboard_action;
	}

	_fm_update_status();

	return LV_RES_OK;
}

static lv_res_t _fm_copy_action(lv_obj_t *btn)
{
	return _fm_set_clipboard(false);
}

static lv_res_t _fm_cut_action(lv_obj_t *btn)
{
	return _fm_set_clipboard(true);
}

static void _fm_paste_task(void *param)
{
	fm_pending_task = NULL;

	_fm_paste_execute(fm_pending_src, fm_pending_dst, fm_pending_overwrite);
}

static void _fm_paste_execute(const char *src_path, const char *dst_path, bool overwrite)
{
	char *src = malloc(FM_PATH_SIZE);
	char *dst = malloc(FM_PATH_SIZE);

	if (!src || !dst)
	{
		free(src);
		free(dst);
		_fm_msg("#FFDD00 메모리 할당 실패!#");
		return;
	}

	strcpy(src, src_path);
	strcpy(dst, dst_path);

	u64 total_size = _fm_get_total_size(src);

	_fm_progress_begin("붙여넣는 중...", total_size);

	int res;

	if (fm.clip_cut)
	{
		if (overwrite)
		{
			res = _fm_copy_recursive(src, dst);

			if (res == FR_OK)
				res = _fm_delete_recursive(src);
		}
		else
		{
			res = f_rename(src, dst);

			// Fall back to copy/delete if the item cannot be moved directly.
			if (res != FR_OK)
			{
				res = _fm_copy_recursive(src, dst);

				if (res == FR_OK)
					res = _fm_delete_recursive(src);
			}
		}
	}
	else
	{
		res = _fm_copy_recursive(src, dst);
	}

	if (res == FR_OK && fm_progress_total)
	{
		fm_progress_done = fm_progress_total;
		_fm_progress_update("붙여넣는 중...", 0);
	}

	_fm_progress_end();

	free(src);
	free(dst);
	_fm_progress_close();

	if (res != FR_OK)
	{
		_fm_msg("#FFDD00 작업 실패!#");
	}
	else
	{
		_fm_cancel_clipboard_action();
	}

	fm.has_sel = false;
	_fm_refresh();
}

static lv_res_t _fm_paste_overwrite_action(lv_obj_t *btns, const char *txt)
{
	lv_obj_t *mbox = lv_mbox_get_from_btn(btns);
	lv_obj_t *dark_bg = lv_obj_get_parent(mbox);

	bool overwrite = !strcmp(txt, "예");

	fm_mbox_btnm = NULL;
	lv_obj_del(dark_bg);

	if (overwrite && fm.has_clip)
	{
		char *src = malloc(FM_PATH_SIZE);
		char *dst = malloc(FM_PATH_SIZE);

		if (!src || !dst)
		{
			free(src);
			free(dst);
			_fm_msg("#FFDD00 메모리 할당 실패!#");
			_fm_restore_browser_input();
			return LV_RES_INV;
		}

		strcpy(src, fm.clip);

		if (!_fm_join(dst, fm.cwd, _fm_basename(fm.clip)))
		{
			free(src);
			free(dst);
			_fm_msg("#FFDD00 경로가 너무 깁니다!#");
			_fm_restore_browser_input();
			return LV_RES_INV;
		}

		strcpy(fm_pending_src, src);
		strcpy(fm_pending_dst, dst);

		fm_pending_overwrite = true;

		free(src);
		free(dst);

		_fm_progress_show("붙여넣는 중...");

		fm_pending_task = lv_task_create(
			_fm_paste_task,
			1,
			LV_TASK_PRIO_HIGH,
			NULL);

		if (!fm_pending_task)
		{
			_fm_progress_close();
			_fm_msg("#FFDD00 메모리 할당 실패!#");
			return LV_RES_INV;
		}

		lv_task_once(fm_pending_task);
	}

	if (!overwrite)
		_fm_restore_browser_input();

	return LV_RES_INV;
}

static lv_res_t _fm_paste_action(lv_obj_t *btn)
{
	if (!fm.has_clip)
	{
		_fm_msg("#FFDD00 클립보드가 비어있습니다#");
		return LV_RES_OK;
	}

	char *src = malloc(FM_PATH_SIZE);
	char *dst = malloc(FM_PATH_SIZE);

	if (!src || !dst)
	{
		free(src);
		free(dst);
		_fm_msg("#FFDD00 메모리 할당 실패!#");
		return LV_RES_OK;
	}

	strcpy(src, fm.clip);

	if (!_fm_join(dst, fm.cwd, _fm_basename(fm.clip)))
	{
		free(src);
		free(dst);
		_fm_msg("#FFDD00 경로가 너무 깁니다!#");
		return LV_RES_OK;
	}

	// Do not allow pasting onto itself.
	if (!strcmp(src, dst))
	{
		free(src);
		free(dst);
		_fm_msg("#FFDD00 같은 위치에 붙여넣을 수 없습니다#");
		return LV_RES_OK;
	}

	// Do not allow copying/moving a directory into itself.
	u32 srclen = strlen(src);
	if (fm.clip_is_dir &&
		!strncmp(dst, src, srclen) &&
		dst[srclen] == '/')
	{
		free(src);
		free(dst);
		_fm_msg("#FFDD00 원본과 대상이 같습니다#");
		return LV_RES_OK;
	}

	if (sd_mount())
	{
		free(src);
		free(dst);
		_fm_msg("#FFDD00 SD 카드 초기화 실패!#");
		return LV_RES_OK;
	}

	FILINFO st;

	if (f_stat(dst, &st) == FR_OK)
	{
		free(src);
		free(dst);

		lv_obj_t *dark_bg = _fm_create_dark_bg();

		static const char *mbox_btn_map[] = {
			"\222예",
			"\222아니오",
			""
		};

		char buf[FM_NAME_SIZE + 128];
		char display_name[FM_NAME_SIZE];

		const char *name = _fm_basename(fm.clip);

		lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);

		// Actual text style used by the mbox.
		lv_mbox_ext_t *ext = lv_obj_get_ext_attr(mbox);
		lv_style_t *text_style = lv_obj_get_style(ext->text);

		lv_style_t *mbox_style = lv_mbox_get_style(mbox, LV_MBOX_STYLE_BG);

		lv_coord_t max_mbox_width = LV_HOR_RES * 9 / 10;
		lv_coord_t padding = mbox_style->body.padding.hor * 2;
		lv_coord_t max_text_width = max_mbox_width - padding;

		_fm_fit_name(display_name, sizeof(display_name), name, text_style, max_text_width);

		const char *line1 =
			fm.clip_is_dir ?
			"같은 이름의 폴더가 이미 존재합니다" :
			"같은 이름의 파일이 이미 존재합니다";

		const char *line3 = "덮어쓰시겠습니까?";

		lv_coord_t width = _fm_text_width(line1, text_style);
		lv_coord_t w;

		w = _fm_text_width(display_name, text_style);
		if (w > width)
			width = w;

		w = _fm_text_width(line3, text_style);
		if (w > width)
			width = w;

		width += padding;

		lv_coord_t min_mbox_width = LV_HOR_RES / 9 * 5;

		if (width < min_mbox_width)
			width = min_mbox_width;

		if (width > max_mbox_width)
			width = max_mbox_width;

		s_printf(
			buf,
			"#FFDD00 같은 이름의 %s 이미 존재합니다#\n\n"
			"#FF8000 %s#\n\n"
			"#FFDD00 덮어쓰시겠습니까?#",
			fm.clip_is_dir ? "폴더가" : "파일이",
			display_name);

		lv_obj_set_width(mbox, width);

		lv_mbox_set_recolor_text(mbox, true);
		lv_mbox_set_text(mbox, buf);

		lv_mbox_add_btns(mbox, mbox_btn_map, _fm_paste_overwrite_action);

		fm_mbox_btnm = lv_obj_get_child(mbox, NULL);
		lv_btnm_control(fm_mbox_btnm, LV_GROUP_KEY_RIGHT);

		_fm_mbox_lock_input();

		lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
		lv_obj_set_top(mbox, true);

		return LV_RES_OK;
	}

	strcpy(fm_pending_src, src);
	strcpy(fm_pending_dst, dst);

	fm_pending_overwrite = false;

	free(src);
	free(dst);

	_fm_progress_show("붙여넣는 중...");

	fm_pending_task = lv_task_create(
		_fm_paste_task,
		1,
		LV_TASK_PRIO_HIGH,
		NULL);

	if (!fm_pending_task)
	{
		_fm_progress_close();
		_fm_msg("#FFDD00 메모리 할당 실패!#");
		return LV_RES_OK;
	}

	lv_task_once(fm_pending_task);

	return LV_RES_OK;
}

static void _fm_delete_task(void *param)
{
	fm_pending_task = NULL;

	char path[FM_PATH_SIZE];

	if (!_fm_join(path, fm.cwd, fm_pending_name))
	{
		_fm_progress_close();
		_fm_msg("#FFDD00 경로가 너무 깁니다!#");
		return;
	}

	char size_path[FM_PATH_SIZE];
	strcpy(size_path, path);

	u64 total_size =
		_fm_get_total_size(size_path);

	_fm_progress_begin("삭제 중...", total_size);

	int res;

	FILINFO st;

	if (f_stat(path, &st) != FR_OK)
	{
		res = FR_NO_FILE;
	}
	else if (st.fattrib & AM_DIR)
	{
		res = _fm_delete_recursive(path);
	}
	else
	{
		u64 size = st.fsize;

		res = f_unlink(path);

		if (res == FR_OK)
			_fm_progress_update("삭제 중...", size);
	}

	if (res == FR_OK && fm_progress_total)
	{
		fm_progress_done =
			fm_progress_total;

		_fm_progress_update("삭제 중...", 0);
	}

	_fm_progress_end();
	_fm_progress_close();

	if (res != FR_OK)
		_fm_msg("#FFDD00 삭제 실패!#");

	fm.has_sel = false;
	_fm_refresh();
}

static lv_res_t _fm_delete_confirm_action(lv_obj_t *btns, const char *txt)
{
	lv_obj_t *mbox = lv_mbox_get_from_btn(btns);
	lv_obj_t *dark_bg = lv_obj_get_parent(mbox);

	if (!strcmp(txt, "예"))
	{
		strcpy(fm_pending_name, fm.sel);

		fm.delete_mbox = NULL;
		fm_mbox_btnm = NULL;

		lv_obj_del(dark_bg);

		_fm_progress_show("삭제 중...");

		fm_pending_task = lv_task_create(
			_fm_delete_task,
			1,
			LV_TASK_PRIO_HIGH,
			NULL);

		if (!fm_pending_task)
		{
			_fm_progress_close();
			_fm_msg("#FFDD00 메모리 할당 실패!#");
			return LV_RES_INV;
		}

		lv_task_once(fm_pending_task);

		return LV_RES_INV;
	}

	fm.delete_mbox = NULL;
	fm_mbox_btnm = NULL;

	lv_obj_del(dark_bg);

	_fm_restore_browser_input();

	return LV_RES_INV;
}

static lv_res_t _fm_delete_action(lv_obj_t *btn)
{
	if (!fm.has_sel)
		return LV_RES_OK;

	lv_obj_t *dark_bg = _fm_create_dark_bg();

	static const char *mbox_btn_map[] = {
		"\222예",
		"\222아니오",
		""
	};

	char buf[FM_NAME_SIZE + 128];
	char display_name[FM_NAME_SIZE];

	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);

	lv_mbox_ext_t *ext = lv_obj_get_ext_attr(mbox);
	lv_style_t *text_style = lv_obj_get_style(ext->text);

	lv_style_t *mbox_style =
		lv_mbox_get_style(mbox, LV_MBOX_STYLE_BG);

	lv_coord_t max_mbox_width = LV_HOR_RES * 9 / 10;
	lv_coord_t padding = mbox_style->body.padding.hor * 2;
	lv_coord_t max_text_width = max_mbox_width - padding;

	_fm_fit_name(display_name, sizeof(display_name), fm.sel, text_style, max_text_width);

	const char *line2 =
		fm.sel_is_dir ?
		"해당 폴더를 삭제하시겠습니까?" :
		"해당 파일을 삭제하시겠습니까?";

	lv_coord_t width =
		_fm_text_width(display_name, text_style);

	lv_coord_t w =
		_fm_text_width(line2, text_style);

	if (w > width)
		width = w;

	width += padding;

	lv_coord_t min_mbox_width = LV_HOR_RES / 9 * 5;

	if (width < min_mbox_width)
		width = min_mbox_width;

	if (width > max_mbox_width)
		width = max_mbox_width;

	s_printf(buf, "#FFDD00 해당 %s 삭제하시겠습니까?#\n\n" "#FF8000 %s#", fm.sel_is_dir ? "폴더를" : "파일을", display_name);

	lv_obj_set_width(mbox, width);

	lv_mbox_set_recolor_text(mbox, true);
	lv_mbox_set_text(mbox, buf);

	lv_mbox_add_btns(mbox, mbox_btn_map, _fm_delete_confirm_action);

	fm.delete_mbox = mbox;
	fm_mbox_btnm = lv_obj_get_child(mbox, NULL);

	lv_btnm_control(fm_mbox_btnm, LV_GROUP_KEY_RIGHT);

	_fm_mbox_lock_input();

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}

static lv_res_t _fm_refresh_action(lv_obj_t *btn)
{
	fm.has_sel = false;
	_fm_refresh();

	return LV_RES_OK;
}

lv_res_t create_file_browser(lv_obj_t *btn)
{
	fm.cwd[0] = '/';
	fm.cwd[1] = 0;
	fm.has_sel = false;
	fm.has_clip = false;

	lv_obj_t *win = nyx_create_file_browser_window(fm.cwd, _fm_close);
	lv_win_add_btn(win, NULL, SYMBOL_REBOOT, _fm_refresh_action);
	lv_win_add_btn(win, NULL, "Ｕ", action_ums_sd);
	fm.win = win;

	lv_obj_t *status_lbl = lv_label_create(win, NULL);
	lv_obj_set_style(status_lbl, &hint_small_style);
	lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_DOT);
	lv_label_set_recolor(status_lbl, true);
	lv_obj_set_width(status_lbl, LV_HOR_RES * 70 / 100);
	lv_label_set_text(status_lbl, "#C7EA46  "SYMBOL_LIST"  클립보드# |");
	lv_obj_align(status_lbl, NULL, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 4);
	fm.status_lbl = status_lbl;

	lv_obj_t *list = lv_list_create(win, NULL);
	lv_obj_set_size(list, LV_HOR_RES * 94 / 100, LV_VER_RES * 70 / 100);
	lv_obj_align(list, status_lbl, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 10);
	lv_list_set_single_mode(list, true);
	fm.list = list;

	// B button
	fb_bb_bg = _fm_create_hint_bg(win, "ⓝ");
	lv_obj_align(fb_bb_bg, list, LV_ALIGN_OUT_TOP_RIGHT, -50, -10);
	fb_bb_label = _fm_create_hint_label(win, "#C7AD59 ⓑ#  뒤로");
	lv_obj_align(fb_bb_label, fb_bb_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	// A button
	fb_ab_bg = _fm_create_hint_bg(win, "ⓝ");
	lv_obj_align(fb_ab_bg, fb_bb_bg, LV_ALIGN_OUT_LEFT_MID, -75, 0);
	fb_ab_label = _fm_create_hint_label(win, "#D14149 ⓐ#  확인");
	lv_obj_align(fb_ab_label, fb_ab_bg, LV_ALIGN_IN_LEFT_MID, -1, 0);

	// D-Pad
	fb_dpad_bg = _fm_create_hint_bg(win, "ⓓ");
	lv_obj_align(fb_dpad_bg, fb_ab_bg, LV_ALIGN_OUT_LEFT_MID, -75, 0);
	fb_dpad_label = _fm_create_hint_label(win, "#4B4B4B ⓒ#  이동");
	lv_obj_align(fb_dpad_label, fb_dpad_bg, LV_ALIGN_IN_LEFT_MID, 0, 0);

	// File operation buttons.
	lv_obj_t *btn_cont = lv_cont_create(win, NULL);
	lv_obj_set_size(btn_cont, LV_HOR_RES * 96 / 100, 65);
	lv_obj_align(btn_cont, list, LV_ALIGN_OUT_BOTTOM_LEFT, -10, LV_DPI / 8);
	lv_cont_set_layout(btn_cont, LV_LAYOUT_PRETTY);
	lv_cont_set_fit(btn_cont, false, false);

	fm.btn_new = lv_btn_create(btn_cont, NULL);
	lv_obj_set_size(fm.btn_new, LV_HOR_RES * 13 / 100, LV_DPI / 2);
	lv_obj_t *lbl7 = lv_label_create(fm.btn_new, NULL);
	lv_obj_set_style(lbl7, &hint_small_style_white);
	lv_label_set_static_text(lbl7, "ⓟ  새 폴더");
	lv_btn_set_action(fm.btn_new, LV_BTN_ACTION_CLICK, _fm_newfolder_action);

	fm.btn_delete = lv_btn_create(btn_cont, fm.btn_new);
	lv_obj_t *lbl6 = lv_label_create(fm.btn_delete, NULL);
	lv_obj_set_style(lbl6, &hint_small_style_white);
	lv_label_set_static_text(lbl6, "ⓜ  삭제");
	lv_btn_set_action(fm.btn_delete, LV_BTN_ACTION_CLICK, _fm_delete_action);

	fm.btn_rename = lv_btn_create(btn_cont, fm.btn_new);
	lv_obj_t *lbl5 = lv_label_create(fm.btn_rename, NULL);
	lv_obj_set_style(lbl5, &hint_small_style_white);
	lv_label_set_static_text(lbl5, "Ⓡ  이름 바꾸기");
	lv_btn_set_action(fm.btn_rename, LV_BTN_ACTION_CLICK, _fm_rename_action);

	fm.btn_cut = lv_btn_create(btn_cont, fm.btn_new);
	fm.xb_label = lv_label_create(fm.btn_cut, NULL);
	lv_obj_set_style(fm.xb_label, &hint_small_style_white);
	lv_label_set_text(fm.xb_label, " ⓝ  잘라내기 ");
	lv_obj_t *lbl3 = lv_label_create(fm.xb_label, NULL);
	lv_label_set_recolor(lbl3, true);
	lv_label_set_text(lbl3, "#1374E6 ⓧ#");
	lv_obj_align(lbl3, NULL, LV_ALIGN_IN_LEFT_MID, 6, 0);
	lv_btn_set_action(fm.btn_cut, LV_BTN_ACTION_CLICK, _fm_cut_action);

	fm.btn_copy = lv_btn_create(btn_cont, fm.btn_new);
	fm.yb_label = lv_label_create(fm.btn_copy, NULL);
	lv_obj_set_style(fm.yb_label, &hint_small_style_white);
	lv_label_set_text(fm.yb_label, " ⓝ  복사 ");
	lv_obj_t *lbl2 = lv_label_create(fm.yb_label, NULL);
	lv_label_set_recolor(lbl2, true);
	lv_label_set_text(lbl2, "#2C8F76 ⓨ#");
	lv_obj_align(lbl2, NULL, LV_ALIGN_IN_LEFT_MID, 6, 0);
	lv_btn_set_action(fm.btn_copy, LV_BTN_ACTION_CLICK, _fm_copy_action);

	_fm_refresh();

	nyx_jc_plus_action  = (void (*)(void))_fm_newfolder_action;
	nyx_jc_minus_action = (void (*)(void))_fm_delete_action;
	nyx_jc_r3_action    = (void (*)(void))_fm_rename_action;
	nyx_jc_dpad_action  = _fm_dpad;
	nyx_jc_a_action     = _fm_enter_selected;
	nyx_jc_b_action     = _fm_b_action;
	nyx_jc_b_long_action = _fm_b_long_action;
	nyx_jc_x_action     = (void (*)(void))_fm_cut_action;
	nyx_jc_y_action     = (void (*)(void))_fm_copy_action;
	nyx_jc_dpad_mode    = true;

	return LV_RES_OK;
}
