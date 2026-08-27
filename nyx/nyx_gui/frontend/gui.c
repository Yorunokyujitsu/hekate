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

#include <bdk.h>

#include "gui.h"
#include "gui_emummc_tools.h"
#include "gui_tools.h"
#include "gui_info.h"
#include "gui_options.h"
#include <libs/lvgl/lv_themes/lv_theme_hekate.h>
#include <libs/lvgl/lvgl.h>
#include "../gfx/logos-gui.h"

#include "../config.h"
#include <libs/fatfs/ff.h>

//==========================
//  ASAP: include, define.
//==========================
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <libs/lvgl/lv_misc/lv_math.h>
#include "fe_emummc_tools.h"
#include "gui_emmc_tools.h"
#include "../gfx/gfx.h"
#include "../gfx/asap_custom.h"
#define PROFILE_SIZE 365
#define MAX_HOME_ENTRIES 5
// hocate
#include "gui_tools_files.h"
#include "gui_l4t_oc.h"
//==========================

extern volatile boot_cfg_t *b_cfg;
extern volatile nyx_storage_t *nyx_str;

extern lv_res_t launch_payload(lv_obj_t *list);

//===============================================
//  ASAP: bool, lv, exturn, struct, unit, enum.
//===============================================
extern lv_res_t launch_fusee(lv_obj_t *list);
extern lv_res_t launch_atlas(lv_obj_t *list);
extern lv_res_t reload_action(lv_obj_t *btns);

static void _do_ofw_boot(void);
static lv_res_t _reboot_ofw_action(lv_obj_t *btn);
static lv_res_t _create_mbox_ofw_warning(void);
static FRESULT g_restore_fr = FR_OK;

static const char *g_restore_step = NULL;
static bool g_ofw_dram_warning = false;
static bool g_ofw_fuse7_warning = false;
static bool g_ofw_dram_confirmed = false;
static bool g_ofw_stock_launch = false;
static int g_pending_launch_idx = -1;

typedef enum {
	RAM_MODE_4GB,
	RAM_MODE_8GB,
} ram_mode_t;

typedef struct { 
	const lv_img_dsc_t *icon;
	const char *label;
	bool is_stock;
	bool is_cfw;
	// hocate
	char ini_name[128];
} entry_t;

static entry_t entries[MAX_HOME_ENTRIES];

typedef struct _launch_menu_entries_t
{
	lv_obj_t *btn[20];
	lv_obj_t *label[20];
	uint8_t dd_map[6];
	uint8_t dd_count;
	// hocate
	char name[20][128];
	const char *ddlabel[20];
} launch_menu_entries_t;

static launch_menu_entries_t launch_ctxt;
static lv_obj_t *launch_bg = NULL;
static bool launch_bg_done = false;

//hocate
static u32 launch_oc_press_ms      = 0;
static lv_indev_t *launch_oc_press_indev   = NULL;
static char *launch_oc_press_name    = NULL;
static bool launch_oc_press_pending = false;
static bool launch_oc_press_fired   = false;
static lv_task_t *launch_oc_press_task    = NULL;
static const char *launch_oc_press_label = NULL;

static lv_img_dsc_t *ext_img = NULL;
static lv_img_dsc_t *profile_img = NULL;
static lv_signal_func_t old_parent_signal;

static lv_res_t (*pin_action)(lv_obj_t *) = NULL;
static lv_obj_t *pin_param = NULL;
static bool pin_unlocked = false;

static bool g_sd_is_exfat = false;
static lv_signal_func_t _old_emu_sig_cb;

lv_obj_t *pw_area;

lv_obj_t *atmo_bg_obj;
lv_obj_t *atmo_sphere_obj;
lv_obj_t *nandmng_label;
lv_obj_t *nandmng_color_labels[6];
lv_obj_t *nandmng_format_label;
lv_obj_t *nandmng_ftype_label;
lv_obj_t *label_status_obj;
lv_obj_t *label_nand_obj;
lv_obj_t *btn_toggle_emu_obj;
lv_obj_t *btn_emuenabled_obj;

lv_style_t btn_custom_rel, btn_custom_pr, btn_custom_pr2, btn_moon_pr;
//===============================================

static bool disp_init_done = false;
static bool do_auto_reload = false;

lv_style_t hint_small_style;
lv_style_t hint_small_style_white;
lv_style_t monospace_text;

lv_obj_t *payload_list;
lv_obj_t *autorcm_btn;
lv_obj_t *close_btn;

//==========================
//  ASAP: For filebrowser.
//==========================
void (*nyx_vol_up_action)(void) = NULL;
void (*nyx_vol_down_action)(void) = NULL;
void (*nyx_jc_plus_action)(void) = NULL;
void (*nyx_jc_minus_action)(void) = NULL;
void (*nyx_jc_a_action)(void) = NULL;
void (*nyx_jc_b_action)(void) = NULL;
void (*nyx_jc_b_long_action)(void) = NULL;
void (*nyx_jc_x_action)(void) = NULL;
void (*nyx_jc_y_action)(void) = NULL;
void (*nyx_jc_l_action)(void) = NULL;
void (*nyx_jc_zl_action)(void) = NULL;
void (*nyx_jc_r_action)(void) = NULL;
void (*nyx_jc_zr_action)(void) = NULL;
void (*nyx_jc_r3_action)(void) = NULL;
void (*nyx_jc_dpad_action)(int dir) = NULL;
bool nyx_jc_dpad_mode = false;
bool nyx_jc_kb_repeat = false;
//==========================

const lv_img_dsc_t *icon_switch;
const lv_img_dsc_t *icon_payload;
lv_img_dsc_t *icon_lakka;

const lv_img_dsc_t *hekate_bg;

lv_style_t btn_transp_rel, btn_transp_pr, btn_transp_tgl_rel, btn_transp_tgl_pr, btn_transp_ina;
lv_style_t ddlist_transp_bg, ddlist_transp_sel;

lv_style_t mbox_darken;

char *text_color;

// hocate
volatile u32 _fps_frames = 0;

typedef struct _jc_lv_driver_t
{
	lv_indev_t *indev_jc;
	lv_indev_t *indev_touch;
// LV_INDEV_READ_PERIOD * JC_CAL_MAX_STEPS = 264 ms.
#define JC_CAL_MAX_STEPS 8
	u32 calibration_step;
	u16 cx_max;
	u16 cx_min;
	u16 cy_max;
	u16 cy_min;
	s16 pos_x;
	s16 pos_y;
	s16 pos_last_x;
	s16 pos_last_y;
	lv_obj_t *cursor;
	u32 cursor_timeout;
	bool cursor_hidden;
	u32 console_timeout;
} jc_lv_driver_t;

static jc_lv_driver_t jc_drv_ctx;

gui_status_bar_ctx status_bar;

static void _nyx_disp_init()
{
	vic_surface_t vic_sfc;
	vic_sfc.src_buf  = NYX_FB2_ADDRESS;
	vic_sfc.dst_buf  = NYX_FB_ADDRESS;
	vic_sfc.width    = 1280;
	vic_sfc.height   = 720;
	vic_sfc.pix_fmt  = VIC_PIX_FORMAT_X8R8G8B8;
	vic_sfc.rotation = VIC_ROTATION_270;

	// Set hardware rotation via VIC.
	vic_init();
	vic_set_surface(&vic_sfc);

	// Turn off backlight to hide the transition.
	display_backlight_brightness(0, 1000);

	// Rotate and copy the first frame.
	vic_compose();
	vic_wait_idle();

	// Switch to new window configuration.
	display_init_window_a_pitch_vic();

	// Enable logging on window D.
	display_init_window_d_console();

	// Switch back the backlight.
	display_backlight_brightness(h_cfg.backlight - 20, 1000);
}

static void _save_log_to_bmp(char *fname)
{
	u32 *fb_ptr = (u32 *)LOG_FB_ADDRESS;

	// Check if there's log written.
	bool log_changed = false;
	for (u32 i = 0; i < 0xCD000; i++)
	{
		if (fb_ptr[i] != 0)
		{
			log_changed = true;
			break;
		}
	}

	if (!log_changed)
		return;

	const u32 file_size = LOG_FB_SZ + 0x36;
	u8 *bitmap = malloc(file_size);

	// Reconstruct FB for bottom-top, landscape bmp. Rotation: 656x1280 -> 1280x656.
	u32 *fb = malloc(LOG_FB_SZ);
	for (int x = 1279; x > - 1; x--)
	{
		for (int y = 655; y > -1; y--)
			fb[y * 1280 + x] = *fb_ptr++;
	}

	manual_system_maintenance(true);

	memcpy(bitmap + 0x36, fb, LOG_FB_SZ);

	typedef struct _bmp_t
	{
		u16 magic;
		u32 size;
		u32 rsvd;
		u32 data_off;
		u32 hdr_size;
		u32 width;
		u32 height;
		u16 planes;
		u16 pxl_bits;
		u32 comp;
		u32 img_size;
		u32 res_h;
		u32 res_v;
		u64 rsvd2;
	} __attribute__((packed)) bmp_t;

	bmp_t *bmp = (bmp_t *)bitmap;

	bmp->magic    = 0x4D42;
	bmp->size     = file_size;
	bmp->rsvd     = 0;
	bmp->data_off = 0x36;
	bmp->hdr_size = 40;
	bmp->width    = 1280;
	bmp->height   = 656;
	bmp->planes   = 1;
	bmp->pxl_bits = 32;
	bmp->comp     = 0;
	bmp->img_size = LOG_FB_SZ;
	bmp->res_h    = 2834;
	bmp->res_v    = 2834;
	bmp->rsvd2    = 0;

	char path[0x80];
	strcpy(path, "backup/screenshots");
	s_printf(path + strlen(path), "/nyx%s_log.bmp", fname);
	sd_save_to_file(bitmap, file_size, path);

	free(bitmap);
	free(fb);
}

static void _save_fb_to_bmp()
{
	// Disallow screenshots if less than 2s passed.
	static u32 timer = 0;
	if (get_tmr_ms() < timer)
		return;

	if (do_auto_reload)
		goto exit;

	// Invalidate data.
	bpmp_mmu_maintenance(BPMP_MMU_MAINT_INVALID_WAY, false);

	const u32 file_size = NYX_FB_SZ + 0x36;
	u8 *bitmap = malloc(file_size);
	u32 *fb = malloc(NYX_FB_SZ);
	u32 *fb_ptr = (u32 *)NYX_FB2_ADDRESS;
	u32 line_bytes = 1280 * sizeof(u32);

	// Reconstruct FB for bottom-top, landscape bmp. No rotation.
	for (int y = 719; y > -1; y--)
	{
		memcpy(&fb[y * 1280], fb_ptr, line_bytes);
		fb_ptr += line_bytes / sizeof(u32);
	}

	// Create notification box.
	lv_obj_t * mbox = lv_mbox_create(lv_layer_top(), NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_mbox_set_text(mbox, SYMBOL_CAMERA"  #FFBA00 스크린샷 저장 중...#");
	lv_obj_set_width(mbox, LV_DPI * 4);
	lv_obj_set_top(mbox, true);
	lv_obj_align(mbox, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

	// Capture effect.
	display_backlight_brightness(255, 100);
	msleep(150);
	display_backlight_brightness(h_cfg.backlight - 20, 100);

	manual_system_maintenance(true);

	memcpy(bitmap + 0x36, fb, NYX_FB_SZ);

	typedef struct _bmp_t
	{
		u16 magic;
		u32 size;
		u32 rsvd;
		u32 data_off;
		u32 hdr_size;
		u32 width;
		u32 height;
		u16 planes;
		u16 pxl_bits;
		u32 comp;
		u32 img_size;
		u32 res_h;
		u32 res_v;
		u64 rsvd2;
	} __attribute__((packed)) bmp_t;

	bmp_t *bmp = (bmp_t *)bitmap;

	bmp->magic    = 0x4D42;
	bmp->size     = file_size;
	bmp->rsvd     = 0;
	bmp->data_off = 0x36;
	bmp->hdr_size = 40;
	bmp->width    = 1280;
	bmp->height   = 720;
	bmp->planes   = 1;
	bmp->pxl_bits = 32;
	bmp->comp     = 0;
	bmp->img_size = NYX_FB_SZ;
	bmp->res_h    = 2834;
	bmp->res_v    = 2834;
	bmp->rsvd2    = 0;

	sd_mount();

	char path[0x80];

	strcpy(path, "backup");
	f_mkdir(path);
	strcat(path, "/screenshots");
	f_mkdir(path);

	// Create date/time name.
	char fname[32];
	rtc_time_t time;
	max77620_rtc_get_time_adjusted(&time);
	s_printf(fname, "%04d%02d%02d_%02d%02d%02d", time.year, time.month, time.day, time.hour, time.min, time.sec);
	s_printf(path + strlen(path), "/screenshot_%s.bmp", fname);

	// Save screenshot and log.
	int res = sd_save_to_file(bitmap, file_size, path);
	if (!res)
		_save_log_to_bmp(fname);

	sd_unmount();

	free(bitmap);
	free(fb);

	if (!res)
		lv_mbox_set_text(mbox, SYMBOL_CAMERA"  #96FF00 스크린샷 저장 완료!#");
	else
		lv_mbox_set_text(mbox, SYMBOL_WARNING"  #FF8000 스크린샷 저장 실패!#");
	manual_system_maintenance(true);
	lv_mbox_start_auto_close(mbox, 4000);

exit:
	// Set timer to 2s.
	timer = get_tmr_ms() + 2000;
}

static void _disp_fb_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const lv_color_t *color_p)
{
	// Draw to intermediate non-rotated framebuffer.
	gfx_set_rect_pitch((u32 *)NYX_FB2_ADDRESS, (u32 *)color_p, 1280, x1, y1, x2, y2);

	// Rotate and copy to visible framebuffer.
	if (disp_init_done) {
		vic_compose();
		_fps_frames++;
	}

	// Check if display init was done. If it's the first big draw, init.
	if (!disp_init_done && ((x2 - x1 + 1) > 600))
	{
		disp_init_done = true;
		_nyx_disp_init();
	}

	lv_flush_ready();
}

static touch_event_t touchpad;
static bool touch_enabled;
static bool console_enabled = false;

static bool _fts_touch_read(lv_indev_data_t *data)
{
	if (!touch_enabled)
		return false;

	int res = touch_poll(&touchpad);

	// Take a screenshot if 3rd finger.
	if (touchpad.finger > 2)
	{
		_save_fb_to_bmp();

		data->state = LV_INDEV_STATE_REL;
		return false;
	}

	if (console_enabled)
	{
		// If no event, keep last debug message.
		if (res)
			return false;

		// Print input debugging in console.
		gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy, &gfx_con.savedcol);
		gfx_con_setpos(32, 638, GFX_COL_AUTO);
		gfx_con.fntsz = 8;
		gfx_printf("x: %4d, y: %4d | z: %3d | ", touchpad.x, touchpad.y, touchpad.z);
		gfx_printf("0: %02X, 1: %02X, 2: %02X, ", touchpad.raw[0], touchpad.raw[1], touchpad.raw[2]);
		gfx_printf("3: %02X, 4: %02X, 5: %02X, 6: %02X",
			touchpad.raw[3], touchpad.raw[4], touchpad.raw[5], touchpad.raw[6]);
		gfx_con_setpos(gfx_con.savedx, gfx_con.savedy, gfx_con.savedcol);
		gfx_con.fntsz = 16;

		return false;
	}

	// Always set touch points.
	data->point.x = touchpad.x;
	data->point.y = touchpad.y;

	// Decide touch enable.
	if (touchpad.touch)
		data->state = LV_INDEV_STATE_PR;
	else
		data->state = LV_INDEV_STATE_REL;

	return false; // No buffering so no more data read.
}

//=====================================
//  ASAP: Filebrowser button mapping.
//=====================================
static bool _jc_virt_mouse_read(lv_indev_data_t *data)
{
	// Volume button actions.
	static bool vol_up_last = false;
	static bool vol_down_last = false;

	u32 vol = btn_read_vol();
	bool vol_up = (vol & BTN_VOL_UP) != 0;
	bool vol_down = (vol & BTN_VOL_DOWN) != 0;

	if (vol_up && !vol_up_last && nyx_vol_up_action)
		nyx_vol_up_action();

	if (vol_down && !vol_down_last && nyx_vol_down_action)
		nyx_vol_down_action();

	vol_up_last = vol_up;
	vol_down_last = vol_down;

	// Poll Joy-Con.
	jc_gamepad_rpt_t *jc_pad = joycon_poll();

	if (!jc_pad)
	{
		data->state = LV_INDEV_STATE_REL;
		return false;
	}

	// Take a screenshot if Capture button is pressed.
	if (jc_pad->cap)
	{
		_save_fb_to_bmp();

		data->state = LV_INDEV_STATE_REL;
		return false;
	}

	// Calibrate left stick.
	if (jc_drv_ctx.calibration_step != JC_CAL_MAX_STEPS)
	{
		if (0) // n_cfg.jc_force_right
		{
			if (jc_pad->conn_r
				&& jc_pad->rstick_x > 0x400 && jc_pad->rstick_y > 0x400
				&& jc_pad->rstick_x < 0xC00 && jc_pad->rstick_y < 0xC00)
			{
				jc_drv_ctx.calibration_step++;
				jc_drv_ctx.cx_max = jc_pad->rstick_x + 0x96;
				jc_drv_ctx.cx_min = jc_pad->rstick_x - 0x96;
				jc_drv_ctx.cy_max = jc_pad->rstick_y + 0x96;
				jc_drv_ctx.cy_min = jc_pad->rstick_y - 0x96;
				jc_drv_ctx.cursor_timeout = 0;
			}
		}
		else if (jc_pad->conn_l
			     && jc_pad->lstick_x > 0x400 && jc_pad->lstick_y > 0x400
			     && jc_pad->lstick_x < 0xC00 && jc_pad->lstick_y < 0xC00)
		{
			jc_drv_ctx.calibration_step++;
			jc_drv_ctx.cx_max = jc_pad->lstick_x + 0x96;
			jc_drv_ctx.cx_min = jc_pad->lstick_x - 0x96;
			jc_drv_ctx.cy_max = jc_pad->lstick_y + 0x96;
			jc_drv_ctx.cy_min = jc_pad->lstick_y - 0x96;
			jc_drv_ctx.cursor_timeout = 0;
		}

		if (jc_drv_ctx.calibration_step != JC_CAL_MAX_STEPS)
		{
			if ((jc_pad->plus && !nyx_jc_plus_action) || (jc_pad->minus && !nyx_jc_minus_action))
				goto handle_console;

			if (console_enabled)
				goto console;

			data->state = LV_INDEV_STATE_REL;

			return false;
		}
	}

	// Re-calibrate on disconnection.
	if (0 && !jc_pad->conn_r) // n_cfg.jc_force_right
		jc_drv_ctx.calibration_step = 0;
	else if (!0 && !jc_pad->conn_l) // n_cfg.jc_force_right
		jc_drv_ctx.calibration_step = 0;

	// Set button presses.
	if (jc_pad->a || jc_pad->zl || jc_pad->zr)
		data->state = LV_INDEV_STATE_PR;
	else
		data->state = LV_INDEV_STATE_REL;

	// Enable console.
	if ((jc_pad->plus && !nyx_jc_plus_action) || (jc_pad->minus && !nyx_jc_minus_action))
	{
handle_console:
		if (((u32)get_tmr_ms() - jc_drv_ctx.console_timeout) > 1000)
		{
			if (!console_enabled)
			{
				display_window_d_console_enable();
				console_enabled = true;
				gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy, &gfx_con.savedcol);
				gfx_con_setpos(964, 630, GFX_COL_AUTO);
				gfx_printf("Press -/+ to close");
				gfx_con_setpos(gfx_con.savedx, gfx_con.savedy, gfx_con.savedcol);
			}
			else
			{
				display_window_d_console_disable();
				console_enabled = false;
			}

			jc_drv_ctx.console_timeout = get_tmr_ms();
		}

		data->state = LV_INDEV_STATE_REL;

		return false;
	}

	if (console_enabled)
	{
console:
		// Print input debugging in console.
		gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy, &gfx_con.savedcol);
		gfx_con_setpos(32, 630, GFX_COL_AUTO);
		gfx_con.fntsz = 8;
		gfx_printf("x: %4X, y: %4X | rx: %4X, ry: %4X | b: %06X | c: %d (%d), %d (%d)",
			jc_pad->lstick_x, jc_pad->lstick_y, jc_pad->rstick_x, jc_pad->rstick_y,
			jc_pad->buttons, jc_pad->batt_info_l, jc_pad->batt_chrg_l,
			jc_pad->batt_info_r, jc_pad->batt_chrg_r);
		gfx_con_setpos(gfx_con.savedx, gfx_con.savedy, gfx_con.savedcol);
		gfx_con.fntsz = 16;

		data->state = LV_INDEV_STATE_REL;

		return false;
	}

	// Calculate new cursor position.
	if (!0) // n_cfg.jc_force_right
	{
		// Left stick X.
		if (jc_pad->lstick_x <= jc_drv_ctx.cx_max && jc_pad->lstick_x >= jc_drv_ctx.cx_min)
			jc_drv_ctx.pos_x += 0;
		else if (jc_pad->lstick_x > jc_drv_ctx.cx_max)
			jc_drv_ctx.pos_x += ((jc_pad->lstick_x - jc_drv_ctx.cx_max) / 30);
		else
			jc_drv_ctx.pos_x -= ((jc_drv_ctx.cx_min - jc_pad->lstick_x) / 30);

		// Left stick Y.
		if (jc_pad->lstick_y <= jc_drv_ctx.cy_max && jc_pad->lstick_y >= jc_drv_ctx.cy_min)
			jc_drv_ctx.pos_y += 0;
		else if (jc_pad->lstick_y > jc_drv_ctx.cy_max)
		{
			s16 val = (jc_pad->lstick_y - jc_drv_ctx.cy_max) / 30;
			// Hoag has inverted Y axis.
			if (jc_pad->sio_mode)
				val *= -1;
			jc_drv_ctx.pos_y -= val;
		}
		else
		{
			s16 val = (jc_drv_ctx.cy_min - jc_pad->lstick_y) / 30;
			// Hoag has inverted Y axis.
			if (jc_pad->sio_mode)
				val *= -1;
			jc_drv_ctx.pos_y += val;
		}
	}
	else
	{
		// Right stick X.
		if (jc_pad->rstick_x <= jc_drv_ctx.cx_max && jc_pad->rstick_x >= jc_drv_ctx.cx_min)
			jc_drv_ctx.pos_x += 0;
		else if (jc_pad->rstick_x > jc_drv_ctx.cx_max)
			jc_drv_ctx.pos_x += ((jc_pad->rstick_x - jc_drv_ctx.cx_max) / 30);
		else
			jc_drv_ctx.pos_x -= ((jc_drv_ctx.cx_min - jc_pad->rstick_x) / 30);

		// Right stick Y.
		if (jc_pad->rstick_y <= jc_drv_ctx.cy_max && jc_pad->rstick_y >= jc_drv_ctx.cy_min)
			jc_drv_ctx.pos_y += 0;
		else if (jc_pad->rstick_y > jc_drv_ctx.cy_max)
		{
			s16 val = (jc_pad->rstick_y - jc_drv_ctx.cy_max) / 30;
			// Hoag has inverted Y axis.
			if (jc_pad->sio_mode)
				val *= -1;
			jc_drv_ctx.pos_y -= val;
		}
		else
		{
			s16 val = (jc_drv_ctx.cy_min - jc_pad->rstick_y) / 30;
			// Hoag has inverted Y axis.
			if (jc_pad->sio_mode)
				val *= -1;
			jc_drv_ctx.pos_y += val;
		}
	}

	// Ensure value inside screen limits.
	if (jc_drv_ctx.pos_x < 0)
		jc_drv_ctx.pos_x = 0;
	else if (jc_drv_ctx.pos_x > 1279)
		jc_drv_ctx.pos_x = 1279;

	if (jc_drv_ctx.pos_y < 0)
		jc_drv_ctx.pos_y = 0;
	else if (jc_drv_ctx.pos_y > 719)
		jc_drv_ctx.pos_y = 719;

	// Set cursor position.
	data->point.x = jc_drv_ctx.pos_x;
	data->point.y = jc_drv_ctx.pos_y;

	// Auto hide cursor.
	if (jc_drv_ctx.pos_x != jc_drv_ctx.pos_last_x || jc_drv_ctx.pos_y != jc_drv_ctx.pos_last_y)
	{
		nyx_jc_dpad_mode = false;
		jc_drv_ctx.pos_last_x = jc_drv_ctx.pos_x;
		jc_drv_ctx.pos_last_y = jc_drv_ctx.pos_y;

		jc_drv_ctx.cursor_hidden = false;
		jc_drv_ctx.cursor_timeout = get_tmr_ms();
		lv_indev_set_cursor(jc_drv_ctx.indev_jc, jc_drv_ctx.cursor);

		// Un hide cursor.
		lv_obj_set_opa_scale_enable(jc_drv_ctx.cursor, false);
	}
	else
	{
		if (!jc_drv_ctx.cursor_hidden)
		{
			if (((u32)get_tmr_ms() - jc_drv_ctx.cursor_timeout) > 3000)
			{
				// Remove cursor and hide it.
				lv_indev_set_cursor(jc_drv_ctx.indev_jc, NULL);
				lv_obj_set_opa_scale_enable(jc_drv_ctx.cursor, true);
				lv_obj_set_opa_scale(jc_drv_ctx.cursor, LV_OPA_TRANSP);

				jc_drv_ctx.cursor_hidden = true;
			}
		}
		else
			data->state = LV_INDEV_STATE_REL; // Ensure that no clicks are allowed.
	}

	if (jc_pad->b && close_btn && !nyx_jc_b_action)
	{
		lv_action_t close_btn_action = lv_btn_get_action(close_btn, LV_BTN_ACTION_CLICK);
		close_btn_action(close_btn);
		close_btn = NULL;
	}

	// + button action
	static bool jc_plus_last = false;
	if (jc_pad->plus && !jc_plus_last && nyx_jc_plus_action)
		nyx_jc_plus_action();
	jc_plus_last = jc_pad->plus;

	// - button action
	static bool jc_minus_last = false;
	if (jc_pad->minus && !jc_minus_last && nyx_jc_minus_action)
		nyx_jc_minus_action();
	jc_minus_last = jc_pad->minus;

	// A button action
	static bool jc_a_last = false;
	if (jc_pad->a && !jc_a_last && nyx_jc_a_action)
		nyx_jc_a_action();
	jc_a_last = jc_pad->a;

	// B button action
	static bool jc_b_last = false;
	static bool jc_b_long_fired = false;
	static u32 jc_b_press_time = 0;
	static u32 jc_b_repeat_timeout = 0;

	if (jc_pad->b)
	{
		u32 now = get_tmr_ms();

		if (!jc_b_last)
		{
			jc_b_press_time = now;
			jc_b_long_fired = false;

			if (nyx_jc_b_action)
				nyx_jc_b_action();

			jc_b_repeat_timeout = now + 300;
		}
		else
		{
			if (!jc_b_long_fired && nyx_jc_b_long_action &&
				(u32)(now - jc_b_press_time) >= 1000)
			{
				jc_b_long_fired = true;
				nyx_jc_b_long_action();
				close_btn = NULL;
			}
			else if (nyx_jc_kb_repeat && nyx_jc_b_action &&
				(s32)(now - jc_b_repeat_timeout) >= 0)
			{
				nyx_jc_b_action();
				jc_b_repeat_timeout = now + 80;
			}
		}
	}
	else
	{
		jc_b_press_time = 0;
		jc_b_repeat_timeout = 0;
		jc_b_long_fired = false;
	}

	jc_b_last = jc_pad->b;

	// X button action
	static bool jc_x_last = false;
	static u32 jc_x_repeat_timeout = 0;

	if (jc_pad->x && nyx_jc_x_action)
	{
		u32 now = get_tmr_ms();

		if (!jc_x_last)
		{
			nyx_jc_x_action();
			jc_x_repeat_timeout = now + 300;
		}
		else if (nyx_jc_kb_repeat &&
			(s32)(now - jc_x_repeat_timeout) >= 0)
		{
			nyx_jc_x_action();
			jc_x_repeat_timeout = now + 80;
		}
	}

	if (!jc_pad->x)
		jc_x_repeat_timeout = 0;

	jc_x_last = jc_pad->x;

	// Y button action
	static bool jc_y_last = false;
	if (jc_pad->y && !jc_y_last && nyx_jc_y_action)
		nyx_jc_y_action();
	jc_y_last = jc_pad->y;

	// L button action
	static bool jc_l_last = false;
	static u32 jc_l_repeat_timeout = 0;

	if (jc_pad->l && nyx_jc_l_action)
	{
		u32 now = get_tmr_ms();

		if (!jc_l_last)
		{
			nyx_jc_l_action();
			jc_l_repeat_timeout = now + 300;
		}
		else if (nyx_jc_kb_repeat &&
			(s32)(now - jc_l_repeat_timeout) >= 0)
		{
			nyx_jc_l_action();
			jc_l_repeat_timeout = now + 80;
		}
	}

	if (!jc_pad->l)
		jc_l_repeat_timeout = 0;

	jc_l_last = jc_pad->l;

	// ZL button action
	static bool jc_zl_last = false;
	static u32 jc_zl_repeat_timeout = 0;

	if (jc_pad->zl && nyx_jc_zl_action)
	{
		u32 now = get_tmr_ms();

		if (!jc_zl_last)
		{
			nyx_jc_zl_action();
			jc_zl_repeat_timeout = now + 300;
		}
		else if (nyx_jc_kb_repeat &&
			(s32)(now - jc_zl_repeat_timeout) >= 0)
		{
			nyx_jc_zl_action();
			jc_zl_repeat_timeout = now + 80;
		}
	}

	if (!jc_pad->zl)
		jc_zl_repeat_timeout = 0;

	jc_zl_last = jc_pad->zl;

	// R button action
	static bool jc_r_last = false;
	static u32 jc_r_repeat_timeout = 0;

	if (jc_pad->r && nyx_jc_r_action)
	{
		u32 now = get_tmr_ms();

		if (!jc_r_last)
		{
			nyx_jc_r_action();
			jc_r_repeat_timeout = now + 300;
		}
		else if (nyx_jc_kb_repeat &&
			(s32)(now - jc_r_repeat_timeout) >= 0)
		{
			nyx_jc_r_action();
			jc_r_repeat_timeout = now + 80;
		}
	}

	if (!jc_pad->r)
		jc_r_repeat_timeout = 0;

	jc_r_last = jc_pad->r;

	// ZR button action
	static bool jc_zr_last = false;
	static u32 jc_zr_repeat_timeout = 0;

	if (jc_pad->zr && nyx_jc_zr_action)
	{
		u32 now = get_tmr_ms();

		if (!jc_zr_last)
		{
			nyx_jc_zr_action();
			jc_zr_repeat_timeout = now + 300;
		}
		else if (nyx_jc_kb_repeat &&
			(s32)(now - jc_zr_repeat_timeout) >= 0)
		{
			nyx_jc_zr_action();
			jc_zr_repeat_timeout = now + 80;
		}
	}

	if (!jc_pad->zr)
		jc_zr_repeat_timeout = 0;

	jc_zr_last = jc_pad->zr;

	// RS button action
	static bool jc_r3_last = false;
	if (jc_pad->r3 && !jc_r3_last && nyx_jc_r3_action)
		nyx_jc_r3_action();
	jc_r3_last = jc_pad->r3;

	// D-pad action. Fires on press and repeats while held.
	if (nyx_jc_dpad_action)
	{
		static int dpad_last = -1;
		static bool dpad_pressed = false;
		static u32 dpad_repeat_timeout = 0;

		int dir = -1;

		if (jc_pad->left)
			dir = NYX_DPAD_LEFT;
		else if (jc_pad->right)
			dir = NYX_DPAD_RIGHT;
		else if (jc_pad->up)
			dir = NYX_DPAD_UP;
		else if (jc_pad->down)
			dir = NYX_DPAD_DOWN;

		u32 now = get_tmr_ms();

		if (dir == -1)
		{
			dpad_pressed = false;
			dpad_last = -1;
			dpad_repeat_timeout = 0;
		}
		else if (!dpad_pressed || dir != dpad_last)
		{
			// First press or direction changed.
			nyx_jc_dpad_mode = true;
			nyx_jc_dpad_action(dir);

			dpad_pressed = true;
			dpad_last = dir;

			// Wait before starting auto-repeat.
			dpad_repeat_timeout = now + 300;
		}
		else if ((s32)(now - dpad_repeat_timeout) >= 0)
		{
			// Auto-repeat while held.
			nyx_jc_dpad_mode = true;
			nyx_jc_dpad_action(dir);

			dpad_repeat_timeout = now + 80;
		}
	}

	return false; // No buffering so no more data read.
}
//=====================================

typedef struct _system_maintenance_tasks_t
{
	union
	{
		lv_task_t *tasks[2];
		struct
		{
			lv_task_t *status_bar;
			lv_task_t *dram_periodic_comp;
		} task;
	};
} system_maintenance_tasks_t;

static system_maintenance_tasks_t system_tasks;

void manual_system_maintenance(bool refresh)
{
	for (u32 task_idx = 0; task_idx < (sizeof(system_maintenance_tasks_t) / sizeof(lv_task_t *)); task_idx++)
	{
		lv_task_t *task = system_tasks.tasks[task_idx];
		if (task && (lv_tick_elaps(task->last_run) >= task->period))
		{
			task->last_run = lv_tick_get();
			task->task(task->param);
		}
	}
	if (refresh)
		lv_refr_now();
}

lv_img_dsc_t *bmp_to_lvimg_obj(const char *path)
{
	u32 fsize;
	u8 *bitmap = sd_file_read(path, &fsize);
	if (!bitmap)
		return NULL;

	struct _bmp_data
	{
		u32 size;
		u32 size_x;
		u32 size_y;
		u32 offset;
	};

	struct _bmp_data bmpData;

	// Get values manually to avoid unaligned access.
	bmpData.size = bitmap[2] | bitmap[3] << 8 |
		bitmap[4] << 16 | bitmap[5] << 24;
	bmpData.offset = bitmap[10] | bitmap[11] << 8 |
		bitmap[12] << 16 | bitmap[13] << 24;
	bmpData.size_x = bitmap[18] | bitmap[19] << 8 |
		bitmap[20] << 16 | bitmap[21] << 24;
	bmpData.size_y = bitmap[22] | bitmap[23] << 8 |
		bitmap[24] << 16 | bitmap[25] << 24;
	// Sanity check.
	if (bitmap[0] == 'B' &&
		bitmap[1] == 'M' &&
		bitmap[28] == 32 && // Only 32 bit BMPs allowed.
		bmpData.size <= fsize)
	{
		// Check if non-default Bottom-Top.
		bool flipped = false;
		if (bmpData.size_y & 0x80000000)
		{
			bmpData.size_y = ~(bmpData.size_y) + 1;
			flipped = true;
		}

		lv_img_dsc_t *img_desc = (lv_img_dsc_t *)bitmap;
		uptr offset_copy = ALIGN((uptr)bitmap + sizeof(lv_img_dsc_t), 0x10);

		img_desc->header.always_zero = 0;
		img_desc->header.w = bmpData.size_x;
		img_desc->header.h = bmpData.size_y;
		img_desc->header.cf = (bitmap[28] == 32) ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR; // Only LV_IMG_CF_TRUE_COLOR_ALPHA is actually allowed.
		img_desc->data_size = bmpData.size - bmpData.offset;
		img_desc->data = (u8 *)offset_copy;

		u32 *tmp = malloc(bmpData.size);
		u32 *tmp2 = (u32 *)offset_copy;

		// Copy the unaligned data to an aligned buffer.
		memcpy((u8 *)tmp, bitmap + bmpData.offset, img_desc->data_size);
		u32 j = 0;

		if (!flipped)
		{
			for (u32 y = 0; y < bmpData.size_y; y++)
			{
				for (u32 x = 0; x < bmpData.size_x; x++)
					tmp2[j++] = tmp[(bmpData.size_y - 1 - y ) * bmpData.size_x + x];
			}
		}
		else
		{
			for (u32 y = 0; y < bmpData.size_y; y++)
			{
				for (u32 x = 0; x < bmpData.size_x; x++)
					tmp2[j++] = tmp[y * bmpData.size_x + x];
			}
		}

		free(tmp);
	}
	else
	{
		free(bitmap);
		return NULL;
	}

	return (lv_img_dsc_t *)bitmap;
}

//===================================
//  ASAP: Filebrowser Image Viewer.
//===================================

// Minimal PNG to LVGL decoder.
typedef struct
{
	const u8 *src;
	u32 src_size;
	u32 src_pos;
	u32 bitbuf;
	u32 bitcnt;
} _png_bits_t;

#define PNG_HUFF_FAST_BITS 9
#define PNG_HUFF_FAST_SIZE (1U << PNG_HUFF_FAST_BITS)

typedef struct
{
	u16 count[16];
	u16 symbol[288];

	u16 fast_sym[PNG_HUFF_FAST_SIZE];
	u8  fast_len[PNG_HUFF_FAST_SIZE];
} _png_huff_t;

static u32 _png_be32(const u8 *p)
{
	return ((u32)p[0] << 24) | ((u32)p[1] << 16) |
	       ((u32)p[2] << 8)  | ((u32)p[3]);
}

static inline int _png_bits_get( _png_bits_t *b, u32 n, u32 *out)
{
	if (!n)
	{
		*out = 0;
		return 0;
	}

	while (b->bitcnt < n)
	{
		if (b->src_pos >= b->src_size)
			return -1;

		b->bitbuf |= (u32)b->src[b->src_pos++] <<
			b->bitcnt;

		b->bitcnt += 8;
	}

	*out = b->bitbuf & ((1U << n) - 1);

	b->bitbuf >>= n;
	b->bitcnt -= n;

	return 0;
}

static void _png_bits_align(_png_bits_t *b)
{
	b->bitbuf = 0;
	b->bitcnt = 0;
}

static inline u32 _png_bit_reverse(u32 code, u32 len)
{
	u32 rev = 0;

	while (len--)
	{
		rev = (rev << 1) | (code & 1);
		code >>= 1;
	}

	return rev;
}

static int _png_huff_build(_png_huff_t *h, const u8 *lens, u32 num)
{
	u16 offs[16];
	u16 next[16];
	u32 i;
	u32 sum = 0;
	u32 code = 0;

	memset(h, 0, sizeof(*h));

	for (i = 0; i < num; i++)
	{
		if (lens[i] > 15)
			return -1;

		h->count[lens[i]]++;
	}

	h->count[0] = 0;

	for (i = 1; i <= 15; i++)
	{
		offs[i] = sum;
		sum += h->count[i];
	}

	if (sum > 288)
		return -1;

	for (i = 0; i < num; i++)
	{
		if (lens[i])
			h->symbol[offs[lens[i]]++] = i;
	}

	// Build canonical Huffman codes.
	next[0] = 0;

	for (i = 1; i <= 15; i++)
	{
		code = (code + h->count[i - 1]) << 1;

		next[i] = (u16)code;
	}

	// Build 9-bit LSB-first Huffman lookup table.
	for (i = 0; i < num; i++)
	{
		u32 len = lens[i];

		if (!len)
			continue;

		u32 c = next[len]++;

		if (len <= PNG_HUFF_FAST_BITS)
		{
			u32 rev = _png_bit_reverse(c, len);

			u32 fill = 1U << (PNG_HUFF_FAST_BITS - len);

			for (u32 j = 0; j < fill; j++)
			{
				u32 idx = rev | (j << len);

				h->fast_sym[idx] = (u16)i;
				h->fast_len[idx] = (u8)len;
			}
		}
	}

	return 0;
}

static inline int _png_huff_decode( _png_bits_t *b, const _png_huff_t *h)
{
	// Use the 9-bit fast Huffman path when possible.
	while (b->bitcnt < PNG_HUFF_FAST_BITS && b->src_pos < b->src_size)
	{
		b->bitbuf |= (u32)b->src[b->src_pos++] <<
			b->bitcnt;

		b->bitcnt += 8;
	}

	if (b->bitcnt >= PNG_HUFF_FAST_BITS)
	{
		u32 idx = b->bitbuf &
			(PNG_HUFF_FAST_SIZE - 1);

		u32 len = h->fast_len[idx];

		if (len)
		{
			int sym = h->fast_sym[idx];

			b->bitbuf >>= len;
			b->bitcnt -= len;

			return sym;
		}
	}

	// Decode longer Huffman codes with the fallback path.
	u32 code = 0;
	u32 first = 0;
	u32 index = 0;

	for (u32 len = 1; len <= 15; len++)
	{
		if (!b->bitcnt)
		{
			if (b->src_pos >= b->src_size)
				return -1;

			b->bitbuf = b->src[b->src_pos++];

			b->bitcnt = 8;
		}

		code |= b->bitbuf & 1;

		b->bitbuf >>= 1;
		b->bitcnt--;

		u32 count = h->count[len];

		if (code < first + count)
			return h->symbol[
				index + (code - first)];

		index += count;
		first = (first + count) << 1;
		code <<= 1;
	}

	return -1;
}

static int _png_inflate_codes( _png_bits_t *b, const _png_huff_t *lit, const _png_huff_t *dist, u8 *out, u32 out_size, u32 *out_pos)
{
	static const u16 len_base[29] =
	{
		3, 4, 5, 6, 7, 8, 9, 10,
		11, 13, 15, 17,
		19, 23, 27, 31,
		35, 43, 51, 59,
		67, 83, 99, 115,
		131, 163, 195, 227,
		258
	};

	static const u8 len_extra[29] =
	{
		0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 1,
		2, 2, 2, 2,
		3, 3, 3, 3,
		4, 4, 4, 4,
		5, 5, 5, 5,
		0
	};

	static const u16 dist_base[30] =
	{
		1, 2, 3, 4,
		5, 7,
		9, 13,
		17, 25,
		33, 49,
		65, 97,
		129, 193,
		257, 385,
		513, 769,
		1025, 1537,
		2049, 3073,
		4097, 6145,
		8193, 12289,
		16385, 24577
	};

	static const u8 dist_extra[30] =
	{
		0, 0, 0, 0,
		1, 1,
		2, 2,
		3, 3,
		4, 4,
		5, 5,
		6, 6,
		7, 7,
		8, 8,
		9, 9,
		10, 10,
		11, 11,
		12, 12,
		13, 13
	};

	for (;;)
	{
		int sym = _png_huff_decode(b, lit);

		if (sym < 0)
			return -1;

		if (sym < 256)
		{
			if (*out_pos >= out_size)
				return -1;

			out[(*out_pos)++] = (u8)sym;
			continue;
		}

		if (sym == 256)
			return 0;

		if (sym < 257 || sym > 285)
			return -1;

		u32 li = (u32)sym - 257;
		u32 length = len_base[li];
		u32 extra;

		if (len_extra[li])
		{
			if (_png_bits_get(b, len_extra[li], &extra))
				return -1;

			length += extra;
		}

		int dsym = _png_huff_decode(b, dist);

		if (dsym < 0 || dsym > 29)
			return -1;

		u32 distance = dist_base[dsym];

		if (dist_extra[dsym])
		{
			if (_png_bits_get(b, dist_extra[dsym], &extra))
				return -1;

			distance += extra;
		}

		if (!distance || distance > *out_pos)
			return -1;

		if (length > out_size - *out_pos)
			return -1;

		u8 *dst = out + *out_pos;
		const u8 *src = dst - distance;

		if (distance >= length)
		{
			memcpy(dst, src, length);
		}
		else
		{
			for (u32 i = 0; i < length; i++)
				dst[i] = src[i];
		}

		*out_pos += length;
	}
}

static int _png_inflate_fixed( _png_bits_t *b, u8 *out, u32 out_size, u32 *out_pos)
{
	u8 ll[288];
	u8 dl[32];

	for (u32 i = 0; i <= 143; i++)
		ll[i] = 8;

	for (u32 i = 144; i <= 255; i++)
		ll[i] = 9;

	for (u32 i = 256; i <= 279; i++)
		ll[i] = 7;

	for (u32 i = 280; i <= 287; i++)
		ll[i] = 8;

	for (u32 i = 0; i < 32; i++)
		dl[i] = 5;

	_png_huff_t lit;
	_png_huff_t dist;

	if (_png_huff_build(&lit, ll, 288))
		return -1;

	if (_png_huff_build(&dist, dl, 32))
		return -1;

	return _png_inflate_codes( b, &lit, &dist, out, out_size, out_pos );
}

static int _png_inflate_dynamic( _png_bits_t *b, u8 *out, u32 out_size, u32 *out_pos)
{
	static const u8 order[19] =
	{
		16, 17, 18, 0, 8, 7, 9, 6,
		10, 5, 11, 4, 12, 3, 13, 2,
		14, 1, 15
	};

	u32 v;

	if (_png_bits_get(b, 5, &v))
		return -1;

	u32 hlit = v + 257;

	if (_png_bits_get(b, 5, &v))
		return -1;

	u32 hdist = v + 1;

	if (_png_bits_get(b, 4, &v))
		return -1;

	u32 hclen = v + 4;

	if (hlit > 286 || hdist > 32)
		return -1;

	u8 clen[19];
	memset(clen, 0, sizeof(clen));

	for (u32 i = 0; i < hclen; i++)
	{
		if (_png_bits_get(b, 3, &v))
			return -1;

		clen[order[i]] = (u8)v;
	}

	_png_huff_t ch;

	if (_png_huff_build(&ch, clen, 19))
		return -1;

	u8 lens[320];
	memset(lens, 0, sizeof(lens));

	u32 total = hlit + hdist;
	u32 n = 0;

	while (n < total)
	{
		int sym = _png_huff_decode(b, &ch);

		if (sym < 0)
			return -1;

		if (sym <= 15)
		{
			lens[n++] = (u8)sym;
		}
		else if (sym == 16)
		{
			if (!n)
				return -1;

			if (_png_bits_get(b, 2, &v))
				return -1;

			u32 repeat = v + 3;
			u8 prev = lens[n - 1];

			if (repeat > total - n)
				return -1;

			while (repeat--)
				lens[n++] = prev;
		}
		else if (sym == 17)
		{
			if (_png_bits_get(b, 3, &v))
				return -1;

			u32 repeat = v + 3;

			if (repeat > total - n)
				return -1;

			while (repeat--)
				lens[n++] = 0;
		}
		else if (sym == 18)
		{
			if (_png_bits_get(b, 7, &v))
				return -1;

			u32 repeat = v + 11;

			if (repeat > total - n)
				return -1;

			while (repeat--)
				lens[n++] = 0;
		}
		else
		{
			return -1;
		}
	}

	_png_huff_t lit;
	_png_huff_t dist;

	if (_png_huff_build(&lit, lens, hlit))
		return -1;

	if (_png_huff_build(&dist, lens + hlit, hdist))
		return -1;

	return _png_inflate_codes( b, &lit, &dist, out, out_size, out_pos );
}

static int _png_zlib_inflate( const u8 *src, u32 src_size, u8 *out, u32 out_size)
{
	if (src_size < 6)
		return -1;

	u8 cmf = src[0];
	u8 flg = src[1];

	if ((cmf & 0x0F) != 8)
		return -1;

	if ((((u32)cmf << 8) | flg) % 31)
		return -1;

	// Preset dictionary not supported.
	if (flg & 0x20)
		return -1;

	// Skip Adler32 validation.
	_png_bits_t b;
	memset(&b, 0, sizeof(b));

	b.src = src + 2;
	b.src_size = src_size - 6;

	u32 out_pos = 0;
	u32 final = 0;

	do
	{
		u32 type;

		if (_png_bits_get(&b, 1, &final))
			return -1;

		if (_png_bits_get(&b, 2, &type))
			return -1;

		if (type == 0)
		{
			_png_bits_align(&b);

			if (b.src_pos + 4 > b.src_size)
				return -1;

			u32 len = (u32)b.src[b.src_pos] |
				((u32)b.src[b.src_pos + 1] << 8);

			u32 nlen = (u32)b.src[b.src_pos + 2] |
				((u32)b.src[b.src_pos + 3] << 8);

			b.src_pos += 4;

			if ((len ^ 0xFFFFU) != nlen)
				return -1;

			if (len > b.src_size - b.src_pos)
				return -1;

			if (len > out_size - out_pos)
				return -1;

			memcpy(out + out_pos, b.src + b.src_pos, len);

			b.src_pos += len;
			out_pos += len;
		}
		else if (type == 1)
		{
			if (_png_inflate_fixed( &b, out, out_size, &out_pos))
				return -1;
		}
		else if (type == 2)
		{
			if (_png_inflate_dynamic( &b, out, out_size, &out_pos))
				return -1;
		}
		else
		{
			return -1;
		}
	}
	while (!final);

	return out_pos == out_size ? 0 : -1;
}

static inline u8 _png_paeth(u8 a, u8 b, u8 c)
{
	int p  = (int)a + (int)b - (int)c;
	int pa = p - (int)a;
	int pb = p - (int)b;
	int pc = p - (int)c;

	if (pa < 0)
		pa = -pa;

	if (pb < 0)
		pb = -pb;

	if (pc < 0)
		pc = -pc;

	if (pa <= pb && pa <= pc)
		return a;

	if (pb <= pc)
		return b;

	return c;
}

static int _png_unfilter( u8 *dst, const u8 *src, u32 stride, u32 height, u32 filter_bpp)
{
	for (u32 y = 0; y < height; y++)
	{
		u8 filter = *src++;

		u8 *row = dst + y * stride;

		u8 *prev = y ? row - stride : NULL;

		switch (filter)
		{
		case 0:
			memcpy(row, src, stride);
			break;

		case 1:
		{
			u32 x = 0;

			for (; x < filter_bpp && x < stride; x++)
				row[x] = src[x];

			for (; x < stride; x++)
				row[x] = src[x] +
					row[x - filter_bpp];

			break;
		}

		case 2:
			if (prev)
			{
				for (u32 x = 0; x < stride; x++)
					row[x] = src[x] +
						prev[x];
			}
			else
			{
				memcpy(row, src, stride);
			}
			break;

		case 3:
		{
			u32 x = 0;

			if (prev)
			{
				for (; x < filter_bpp && x < stride; x++)
				{
					row[x] = src[x] +
						(u8)(prev[x] >> 1);
				}

				for (; x < stride; x++)
				{
					row[x] = src[x] +
						(u8)( ((u32)row[x - filter_bpp] + prev[x]) >> 1);
				}
			}
			else
			{
				for (; x < filter_bpp && x < stride; x++)
					row[x] = src[x];

				for (; x < stride; x++)
				{
					row[x] = src[x] +
						(u8)( (u32)row[x - filter_bpp] >> 1);
				}
			}

			break;
		}

		case 4:
		{
			u32 x = 0;

			if (prev)
			{
				// Paeth(0, b, 0) = b.
				for (; x < filter_bpp && x < stride; x++)
				{
					row[x] = src[x] +
						prev[x];
				}

				for (; x < stride; x++)
				{
					row[x] = src[x] +
						_png_paeth( row[x - filter_bpp], prev[x], prev[x - filter_bpp]);
				}
			}
			else
			{
				// Without a previous row, Paeth(a, 0, 0) = a.
				for (; x < filter_bpp && x < stride; x++)
					row[x] = src[x];

				for (; x < stride; x++)
				{
					row[x] = src[x] +
						row[x - filter_bpp];
				}
			}

			break;
		}

		default:
			return -1;
		}

		src += stride;
	}

	return 0;
}

static const u8 _png_adam7_x_start[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const u8 _png_adam7_y_start[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const u8 _png_adam7_x_step[7]  = { 8, 8, 4, 4, 2, 2, 1 };
static const u8 _png_adam7_y_step[7]  = { 8, 8, 8, 4, 4, 2, 2 };

static u32 _png_pass_size(u32 size, u32 start, u32 step)
{
	if (size <= start)
		return 0;

	return (size - start + step - 1) / step;
}

static bool _png_valid_format(u8 color_type, u8 bit_depth)
{
	switch (color_type)
	{
	case 0:
		return bit_depth == 1 || bit_depth == 2 || bit_depth == 4 || bit_depth == 8 || bit_depth == 16;
	case 2:
	case 4:
	case 6:
		return bit_depth == 8 || bit_depth == 16;
	case 3:
		return bit_depth == 1 || bit_depth == 2 || bit_depth == 4 || bit_depth == 8;
	default:
		return false;
	}
}

static u32 _png_channels(u8 color_type)
{
	switch (color_type)
	{
	case 0: return 1;
	case 2: return 3;
	case 3: return 1;
	case 4: return 2;
	case 6: return 4;
	default: return 0;
	}
}

static u32 _png_filter_bpp(u8 color_type, u8 bit_depth)
{
	u32 channels = _png_channels(color_type);
	u32 bits = channels * bit_depth;

	u32 bytes = (bits + 7) >> 3;

	if (!bytes)
		bytes = 1;

	return bytes;
}

static u32 _png_rowbytes( u32 width, u8 color_type, u8 bit_depth)
{
	u32 channels = _png_channels(color_type);

	return (width * channels * bit_depth + 7) >> 3;
}

static u32 _png_sample( const u8 *row, u32 sample, u8 bit_depth)
{
	switch (bit_depth)
	{
	case 1:
		return (row[sample >> 3] >> (7 - (sample & 7))) & 1;

	case 2:
		return (row[sample >> 2] >> (6 - ((sample & 3) << 1))) & 3;

	case 4:
		return (row[sample >> 1] >> (4 - ((sample & 1) << 2))) & 15;

	case 8:
		return row[sample];

	case 16:
		return
			((u32)row[sample * 2] << 8) | row[sample * 2 + 1];

	default:
		return 0;
	}
}

static u8 _png_sample_to_u8( u32 value, u8 bit_depth)
{
	switch (bit_depth)
	{
	case 1:
		return value ? 255 : 0;

	case 2:
		return (u8)(value * 85);

	case 4:
		return (u8)(value * 17);

	case 8:
		return (u8)value;

	case 16:
		return (u8)(value >> 8);

	default:
		return 0;
	}
}

static void _png_pixel_to_bgra(
	u8 *dst,
	const u8 *row,
	u32 x,
	u8 color_type,
	u8 bit_depth,
	const u8 *palette,
	const u8 *palette_alpha,
	u32 palette_count,
	bool trns_gray_valid,
	u16 trns_gray,
	bool trns_rgb_valid,
	u16 trns_r,
	u16 trns_g,
	u16 trns_b)
{
	u8 r = 0;
	u8 g = 0;
	u8 b = 0;
	u8 a = 255;

	if (color_type == 0)
	{
		u32 gray = _png_sample(row, x, bit_depth);

		u8 gray8 = _png_sample_to_u8(gray, bit_depth);

		r = gray8;
		g = gray8;
		b = gray8;

		if (trns_gray_valid && gray == trns_gray)
			a = 0;
	}
	else if (color_type == 2)
	{
		u32 base = x * 3;

		u32 rv = _png_sample(row, base, bit_depth);

		u32 gv = _png_sample(row, base + 1, bit_depth);

		u32 bv = _png_sample(row, base + 2, bit_depth);

		r = _png_sample_to_u8(rv, bit_depth);
		g = _png_sample_to_u8(gv, bit_depth);
		b = _png_sample_to_u8(bv, bit_depth);

		if (trns_rgb_valid && rv == trns_r && gv == trns_g && bv == trns_b)
			a = 0;
	}
	else if (color_type == 3)
	{
		u32 idx = _png_sample(row, x, bit_depth);

		if (idx < palette_count)
		{
			r = palette[idx * 3];
			g = palette[idx * 3 + 1];
			b = palette[idx * 3 + 2];
			a = palette_alpha[idx];
		}
		else
		{
			a = 0;
		}
	}
	else if (color_type == 4)
	{
		u32 base = x * 2;

		u32 gray = _png_sample(row, base, bit_depth);

		u32 alpha = _png_sample(row, base + 1, bit_depth);

		u8 gray8 = _png_sample_to_u8(gray, bit_depth);

		r = gray8;
		g = gray8;
		b = gray8;

		a = _png_sample_to_u8( alpha, bit_depth);
	}
	else if (color_type == 6)
	{
		u32 base = x * 4;

		r = _png_sample_to_u8( _png_sample(row, base, bit_depth), bit_depth);

		g = _png_sample_to_u8( _png_sample(row, base + 1, bit_depth), bit_depth);

		b = _png_sample_to_u8( _png_sample(row, base + 2, bit_depth), bit_depth);

		a = _png_sample_to_u8( _png_sample(row, base + 3, bit_depth), bit_depth);
	}

	dst[0] = b;
	dst[1] = g;
	dst[2] = r;
	dst[3] = a;
}

static inline void _png_pixel_to_bgra_8_fast(
	u8 *dst,
	const u8 *row,
	u32 x,
	u8 color_type,
	const u8 *palette,
	const u8 *palette_alpha,
	u32 palette_count,
	bool trns_gray_valid,
	u16 trns_gray,
	bool trns_rgb_valid,
	u16 trns_r,
	u16 trns_g,
	u16 trns_b)
{
	if (color_type == 6)
	{
		const u8 *p = row + x * 4;

		dst[0] = p[2];
		dst[1] = p[1];
		dst[2] = p[0];
		dst[3] = p[3];
		return;
	}

	if (color_type == 2)
	{
		const u8 *p = row + x * 3;

		dst[0] = p[2];
		dst[1] = p[1];
		dst[2] = p[0];

		if (trns_rgb_valid && p[0] == (u8)trns_r && p[1] == (u8)trns_g && p[2] == (u8)trns_b)
		{
			dst[3] = 0;
		}
		else
		{
			dst[3] = 255;
		}

		return;
	}

	if (color_type == 0)
	{
		u8 g = row[x];

		dst[0] = g;
		dst[1] = g;
		dst[2] = g;

		dst[3] = (trns_gray_valid &&
			 g == (u8)trns_gray) ?
			0 : 255;

		return;
	}

	if (color_type == 4)
	{
		const u8 *p = row + x * 2;

		dst[0] = p[0];
		dst[1] = p[0];
		dst[2] = p[0];
		dst[3] = p[1];
		return;
	}

	if (color_type == 3)
	{
		u32 idx = row[x];

		if (idx < palette_count)
		{
			dst[0] = palette[idx * 3 + 2];
			dst[1] = palette[idx * 3 + 1];
			dst[2] = palette[idx * 3];
			dst[3] = palette_alpha[idx];
		}
		else
		{
			dst[0] = 0;
			dst[1] = 0;
			dst[2] = 0;
			dst[3] = 0;
		}
	}
}

static void _png_fit_size( u32 src_w, u32 src_h, u32 max_w, u32 max_h, u32 *dst_w, u32 *dst_h)
{
	*dst_w = src_w;
	*dst_h = src_h;

	// Never upscale.
	if (src_w <= max_w && src_h <= max_h)
		return;

	u64 w_by_h = (u64)src_w * max_h / src_h;

	if (w_by_h <= max_w)
	{
		*dst_w = (u32)w_by_h;
		*dst_h = max_h;
	}
	else
	{
		*dst_w = max_w;
		*dst_h = (u32)((u64)src_h * max_w / src_w);
	}

	if (!*dst_w)
		*dst_w = 1;

	if (!*dst_h)
		*dst_h = 1;
}

lv_img_dsc_t *png_to_lvimg_obj(const char *path)
{
	u32 fsize;
	u8 *png = sd_file_read(path, &fsize);

	if (!png)
		return NULL;

	static const u8 sig[8] =
	{
		0x89, 'P', 'N', 'G',
		0x0D, 0x0A, 0x1A, 0x0A
	};

	if (fsize < 33 || memcmp(png, sig, 8))
	{
		free(png);
		return NULL;
	}

	u32 width = 0;
	u32 height = 0;

	u8 bit_depth = 0;
	u8 color_type = 0;
	u8 compression = 0;
	u8 filter_method = 0;
	u8 interlace = 0;

	bool got_ihdr = false;

	u8 palette[256 * 3];
	u8 palette_alpha[256];

	memset(palette, 0, sizeof(palette));
	memset(palette_alpha, 0xFF, sizeof(palette_alpha));

	u32 palette_count = 0;

	bool trns_gray_valid = false;
	u16 trns_gray = 0;

	bool trns_rgb_valid = false;
	u16 trns_r = 0;
	u16 trns_g = 0;
	u16 trns_b = 0;

	u32 idat_size = 0;
	u32 pos = 8;

	// Parse PNG chunks.
	while (pos + 12 <= fsize)
	{
		u32 len = _png_be32(png + pos);
		pos += 4;

		if (pos > fsize || len > fsize - pos || fsize - pos - len < 8)
		{
			free(png);
			return NULL;
		}

		const u8 *type = png + pos;
		pos += 4;

		const u8 *data = png + pos;

		if (!memcmp(type, "IHDR", 4))
		{
			if (len != 13 || got_ihdr)
			{
				free(png);
				return NULL;
			}

			width = _png_be32(data);
			height = _png_be32(data + 4);

			bit_depth = data[8];
			color_type = data[9];
			compression = data[10];
			filter_method = data[11];
			interlace = data[12];

			got_ihdr = true;
		}
		else if (!memcmp(type, "PLTE", 4))
		{
			if (!len || (len % 3) || len > sizeof(palette))
			{
				free(png);
				return NULL;
			}

			palette_count = len / 3;
			memcpy(palette, data, len);
		}
		else if (!memcmp(type, "tRNS", 4))
		{
			if (!got_ihdr)
			{
				free(png);
				return NULL;
			}

			if (color_type == 0)
			{
				if (len != 2)
				{
					free(png);
					return NULL;
				}

				trns_gray = ((u16)data[0] << 8) |
					data[1];

				trns_gray_valid = true;
			}
			else if (color_type == 2)
			{
				if (len != 6)
				{
					free(png);
					return NULL;
				}

				trns_r = ((u16)data[0] << 8) |
					data[1];

				trns_g = ((u16)data[2] << 8) |
					data[3];

				trns_b = ((u16)data[4] << 8) |
					data[5];

				trns_rgb_valid = true;
			}
			else if (color_type == 3)
			{
				if (len > 256)
				{
					free(png);
					return NULL;
				}

				for (u32 i = 0; i < len; i++)
					palette_alpha[i] = data[i];
			}
		}
		else if (!memcmp(type, "IDAT", 4))
		{
			if (idat_size > 0xFFFFFFFFU - len)
			{
				free(png);
				return NULL;
			}

			idat_size += len;
		}
		else if (!memcmp(type, "IEND", 4))
		{
			break;
		}

		pos += len + 4;
	}

	// Validate PNG format.
	if (!got_ihdr || !width || !height || !idat_size || compression != 0 || filter_method != 0 || (interlace != 0 && interlace != 1))
	{
		free(png);
		return NULL;
	}

	// Validate color type and bit depth.
	if (!_png_valid_format(color_type, bit_depth))
	{
		free(png);
		return NULL;
	}

	if (color_type == 3)
	{
		if (!palette_count)
		{
			free(png);
			return NULL;
		}

		// Validate palette size against bit depth.
		if (palette_count > (1U << bit_depth))
		{
			free(png);
			return NULL;
		}
	}

	u32 channels = _png_channels(color_type);

	if (!channels)
	{
		free(png);
		return NULL;
	}

	u32 filter_bpp = _png_filter_bpp(color_type, bit_depth);

	// Calculate decompressed scanline size.
	u32 raw_size = 0;

	if (!interlace)
	{
		if (width > 0xFFFFFFFFU / (channels * (u32)bit_depth))
		{
			free(png);
			return NULL;
		}

		u32 stride = _png_rowbytes(
				width,
				color_type,
				bit_depth);

		if (!stride || height > 0xFFFFFFFFU / (stride + 1))
		{
			free(png);
			return NULL;
		}

		raw_size = (stride + 1) * height;
	}
	else
	{

		for (u32 pass = 0; pass < 7; pass++)
		{
			u32 pw = _png_pass_size(
					width,
					_png_adam7_x_start[pass],
					_png_adam7_x_step[pass]);

			u32 ph = _png_pass_size(
					height,
					_png_adam7_y_start[pass],
					_png_adam7_y_step[pass]);

			if (!pw || !ph)
				continue;

			u32 stride = _png_rowbytes(
					pw,
					color_type,
					bit_depth);

			if (!stride || ph > 0xFFFFFFFFU / (stride + 1))
			{
				free(png);
				return NULL;
			}

			u32 pass_size = (stride + 1) * ph;

			if (raw_size > 0xFFFFFFFFU - pass_size)
			{
				free(png);
				return NULL;
			}

			raw_size += pass_size;
		}
	}

	if (!raw_size)
	{
		free(png);
		return NULL;
	}

	// Join IDAT chunks.
	u8 *idat = malloc(idat_size);

	if (!idat)
	{
		free(png);
		return NULL;
	}

	u32 idat_pos = 0;
	pos = 8;

	while (pos + 12 <= fsize)
	{
		u32 len = _png_be32(png + pos);
		pos += 4;

		if (pos > fsize || len > fsize - pos || fsize - pos - len < 8)
		{
			free(idat);
			free(png);
			return NULL;
		}

		const u8 *type = png + pos;
		pos += 4;

		if (!memcmp(type, "IDAT", 4))
		{
			if (len > idat_size - idat_pos)
			{
				free(idat);
				free(png);
				return NULL;
			}

			memcpy( idat + idat_pos, png + pos, len);

			idat_pos += len;
		}

		if (!memcmp(type, "IEND", 4))
			break;

		pos += len + 4;
	}

	free(png);

	if (idat_pos != idat_size)
	{
		free(idat);
		return NULL;
	}

	// Inflate IDAT data.
	u8 *raw = malloc(raw_size);

	if (!raw)
	{
		free(idat);
		return NULL;
	}

	if (_png_zlib_inflate( idat, idat_size, raw, raw_size))
	{
		free(raw);
		free(idat);
		return NULL;
	}

	free(idat);

	// Decode once at enough resolution for normal and rotated display.
	u32 out_width;
	u32 out_height;

	_png_fit_size( width, height, LV_HOR_RES, LV_HOR_RES, &out_width, &out_height);

	if (out_width > 0xFFFFFFFFU / 4)
	{
		free(raw);
		return NULL;
	}

	u32 lv_stride = out_width * 4;

	if (out_height > 0xFFFFFFFFU / lv_stride)
	{
		free(raw);
		return NULL;
	}

	u32 lv_data_size = lv_stride * out_height;

	if (lv_data_size > 0xFFFFFFFFU - sizeof(lv_img_dsc_t) - 0x10)
	{
		free(raw);
		return NULL;
	}

	u32 alloc_size = sizeof(lv_img_dsc_t) +
		0x10 + lv_data_size;

	u8 *mem = malloc(alloc_size);

	if (!mem)
	{
		free(raw);
		return NULL;
	}

	memset( mem, 0, sizeof(lv_img_dsc_t) + 0x10);

	lv_img_dsc_t *img_desc = (lv_img_dsc_t *)mem;

	uptr data_addr = ALIGN(
			(uptr)mem + sizeof(lv_img_dsc_t),
			0x10);

	u8 *dst = (u8 *)data_addr;

	memset(dst, 0, lv_data_size);

	img_desc->header.always_zero = 0;
	img_desc->header.w = out_width;
	img_desc->header.h = out_height;
	img_desc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;

	img_desc->data_size = lv_data_size;

	img_desc->data = dst;

	// Decode non-interlaced rows directly into the LVGL buffer.
	if (!interlace)
	{
		u32 stride = _png_rowbytes(
				width,
				color_type,
				bit_depth);

		if (!stride || stride > 0xFFFFFFFFU / 2)
		{
			free(raw);
			free(mem);
			return NULL;
		}

		u8 *row_mem = malloc(stride * 2);

		if (!row_mem)
		{
			free(raw);
			free(mem);
			return NULL;
		}

		u8 *prev_row = row_mem;
		u8 *cur_row  = row_mem + stride;

		memset(prev_row, 0, stride);

		const u8 *src = raw;

		// Precalculate horizontal source coordinates.
		u32 *x_map = malloc(out_width * sizeof(u32));

		if (!x_map)
		{
			free(row_mem);
			free(raw);
			free(mem);
			return NULL;
		}

		for (u32 dx = 0; dx < out_width; dx++)
		{
			u32 sx = (u32)(
					((u64)dx * width) / out_width);

			if (sx >= width)
				sx = width - 1;

			x_map[dx] = sx;
		}

		u32 next_dst_y = 0;

		for (u32 y = 0; y < height; y++)
		{
			u8 filter = *src++;

			switch (filter)
			{
			case 0:
				memcpy(cur_row, src, stride);
				break;

			case 1:
			{
				u32 i = 0;

				// First pixel has no left neighbor.
				for (; i < filter_bpp && i < stride; i++)
					cur_row[i] = src[i];

				// Remaining bytes have a left neighbor.
				for (; i < stride; i++)
					cur_row[i] = src[i] +
						cur_row[i - filter_bpp];

				break;
			}

			case 2:
				for (u32 i = 0; i < stride; i++)
				{
					cur_row[i] = src[i] +
						prev_row[i];
				}
				break;

			case 3:
			{
				u32 i = 0;

				// First pixel uses left = 0.
				for (; i < filter_bpp && i < stride; i++)
				{
					cur_row[i] = src[i] +
						(u8)(prev_row[i] >> 1);
				}

				// Decode remaining pixels.
				for (; i < stride; i++)
				{
					cur_row[i] = src[i] +
						(u8)( ((u32)cur_row[i - filter_bpp] + prev_row[i]) >> 1);
				}

				break;
			}

			case 4:
			{
				u32 i = 0;

				// First pixel uses left = upper-left = 0.
				for (; i < filter_bpp && i < stride; i++)
				{
					cur_row[i] = src[i] +
						prev_row[i];
				}

				// Decode remaining pixels.
				for (; i < stride; i++)
				{
					cur_row[i] = src[i] +
						_png_paeth( cur_row[i - filter_bpp], prev_row[i], prev_row[i - filter_bpp]);
				}

				break;
			}

			default:
				free(x_map);
				free(row_mem);
				free(raw);
				free(mem);
				return NULL;
			}

			while (next_dst_y < out_height)
			{
				u32 src_y = (u32)(
						((u64)next_dst_y * height) /
						out_height);

				if (src_y != y)
					break;

				u8 *dst_row = dst +
					next_dst_y * out_width * 4;

				u8 *d = dst_row;

				// Use the fast path for 8-bit RGB/RGBA.
				if (bit_depth == 8)
				{
					if (color_type == 6)
					{
						for (u32 dx = 0; dx < out_width; dx++)
						{
							const u8 *p = cur_row +
								x_map[dx] * 4;

							d[0] = p[2];
							d[1] = p[1];
							d[2] = p[0];
							d[3] = p[3];

							d += 4;
						}
					}
					else if (color_type == 2 && !trns_rgb_valid)
					{
						for (u32 dx = 0; dx < out_width; dx++)
						{
							const u8 *p = cur_row +
								x_map[dx] * 3;

							d[0] = p[2];
							d[1] = p[1];
							d[2] = p[0];
							d[3] = 255;

							d += 4;
						}
					}
					else
					{
						for (u32 dx = 0; dx < out_width; dx++)
						{
							_png_pixel_to_bgra_8_fast(
								d,
								cur_row,
								x_map[dx],
								color_type,
								palette,
								palette_alpha,
								palette_count,
								trns_gray_valid,
								trns_gray,
								trns_rgb_valid,
								trns_r,
								trns_g,
								trns_b);

							d += 4;
						}
					}
				}
				else
				{
					// Use the generic path for 1/2/4/16-bit PNG.
					for (u32 dx = 0; dx < out_width; dx++)
					{
						_png_pixel_to_bgra(
							d,
							cur_row,
							x_map[dx],
							color_type,
							bit_depth,
							palette,
							palette_alpha,
							palette_count,
							trns_gray_valid,
							trns_gray,
							trns_rgb_valid,
							trns_r,
							trns_g,
							trns_b);

						d += 4;
					}
				}

				next_dst_y++;
			}

			src += stride;

			u8 *tmp = prev_row;
			prev_row = cur_row;
			cur_row = tmp;
		}

		free(x_map);
		free(row_mem);

		if (next_dst_y != out_height)
		{
			free(raw);
			free(mem);
			return NULL;
		}
	}
	else
	{
		// Decode Adam7 only when no resize is required.
		if (out_width != width || out_height != height)
		{
			free(raw);
			free(mem);
			return NULL;
		}

		u32 raw_pos = 0;

		for (u32 pass = 0; pass < 7; pass++)
		{
			u32 pw = _png_pass_size(
					width,
					_png_adam7_x_start[pass],
					_png_adam7_x_step[pass]);

			u32 ph = _png_pass_size(
					height,
					_png_adam7_y_start[pass],
					_png_adam7_y_step[pass]);

			if (!pw || !ph)
				continue;

			u32 stride = _png_rowbytes(
					pw,
					color_type,
					bit_depth);

			if (!stride || ph > 0xFFFFFFFFU / stride)
			{
				free(raw);
				free(mem);
				return NULL;
			}

			u32 pixel_size = stride * ph;

			if (ph > 0xFFFFFFFFU / (stride + 1))
			{
				free(raw);
				free(mem);
				return NULL;
			}

			u32 pass_raw_size = (stride + 1) * ph;

			if (raw_pos > raw_size || pass_raw_size > raw_size - raw_pos)
			{
				free(raw);
				free(mem);
				return NULL;
			}

			u8 *pixels = malloc(pixel_size);

			if (!pixels)
			{
				free(raw);
				free(mem);
				return NULL;
			}

			if (_png_unfilter( pixels, raw + raw_pos, stride, ph, filter_bpp))
			{
				free(pixels);
				free(raw);
				free(mem);
				return NULL;
			}

			raw_pos += pass_raw_size;

			for (u32 py = 0; py < ph; py++)
			{
				u32 y = _png_adam7_y_start[pass] +
					py * _png_adam7_y_step[pass];

				if (y >= height)
					continue;

				const u8 *row = pixels +
					py * stride;

				for (u32 px = 0; px < pw; px++)
				{
					u32 x = _png_adam7_x_start[pass] +
						px * _png_adam7_x_step[pass];

					if (x >= width)
						continue;

					u8 *dst_pixel = dst +
						((y * out_width + x) * 4);

					_png_pixel_to_bgra(
						dst_pixel,
						row,
						px,
						color_type,
						bit_depth,
						palette,
						palette_alpha,
						palette_count,
						trns_gray_valid,
						trns_gray,
						trns_rgb_valid,
						trns_r,
						trns_g,
						trns_b);
				}
			}

			free(pixels);
		}

		if (raw_pos != raw_size)
		{
			free(raw);
			free(mem);
			return NULL;
		}
	}

	free(raw);

	return img_desc;
}

// Minimal Baseline JPEG to LVGL decoder.
typedef struct
{
	const u8 *data;
	u32 size;
	u32 pos;
	u32 bitbuf;
	u32 bitcnt;
} _jpg_bits_t;

typedef struct
{
	u8 bits[17];
	u8 vals[256];
	u16 count;
	u16 first_code[17];
	u16 first_idx[17];
} _jpg_huff_t;

typedef struct
{
	u8 id;
	u8 h;
	u8 v;
	u8 tq;
	u8 td;
	u8 ta;
	s16 dc_pred;
} _jpg_comp_t;

typedef struct
{
	const u8 *jpg;
	u32 size;

	u32 width;
	u32 height;

	u8 ncomp;
	_jpg_comp_t comp[3];

	u16 qt[4][64];
	bool qt_valid[4];

	_jpg_huff_t huff_dc[4];
	_jpg_huff_t huff_ac[4];
	bool huff_dc_valid[4];
	bool huff_ac_valid[4];

	u8 max_h;
	u8 max_v;

	const u8 *scan;
	u32 scan_size;

	u16 restart_interval;
} _jpg_ctx_t;

static const u8 _jpg_zigzag[64] =
{
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

static u16 _jpg_be16(const u8 *p)
{
	return ((u16)p[0] << 8) | p[1];
}

static int _jpg_get_byte(_jpg_bits_t *b)
{
	if (b->pos >= b->size)
		return -1;

	int v = b->data[b->pos++];

	if (v == 0xFF)
	{
		if (b->pos >= b->size)
			return -1;

		int n = b->data[b->pos];

		if (n == 0x00)
		{
			b->pos++;
			return 0xFF;
		}

		// Marker encountered.
		return -1;
	}

	return v;
}

static int _jpg_get_bits(_jpg_bits_t *b, u32 n)
{
	while (b->bitcnt < n)
	{
		int c = _jpg_get_byte(b);

		if (c < 0)
			return -1;

		b->bitbuf = (b->bitbuf << 8) | (u32)c;
		b->bitcnt += 8;
	}

	b->bitcnt -= n;

	return (b->bitbuf >> b->bitcnt) & ((1U << n) - 1);
}

static int _jpg_receive_extend(_jpg_bits_t *b, int n)
{
	if (!n)
		return 0;

	int v = _jpg_get_bits(b, n);

	if (v < 0)
		return 0x7FFFFFFF;

	int vt = 1 << (n - 1);

	if (v < vt)
		v -= (1 << n) - 1;

	return v;
}

static int _jpg_huff_decode(_jpg_bits_t *b, const _jpg_huff_t *h)
{
	u32 code = 0;

	for (u32 len = 1; len <= 16; len++)
	{
		int bit = _jpg_get_bits(b, 1);
		if (bit < 0)
			return -1;

		code = (code << 1) | bit;

		u32 first = h->first_code[len];
		u32 count = h->bits[len];

		if (code >= first && code < first + count)
		{
			u32 idx = h->first_idx[len] + code - first;

			if (idx >= h->count)
				return -1;

			return h->vals[idx];
		}
	}

	return -1;
}

static int _jpg_parse_dqt( _jpg_ctx_t *j, const u8 *p, u32 len)
{
	while (len)
	{
		u8 info = *p++;
		len--;

		u8 pq = info >> 4;
		u8 tq = info & 0x0F;

		if (tq > 3 || pq > 1)
			return -1;

		u32 need = pq ? 128 : 64;

		if (len < need)
			return -1;

		for (u32 i = 0; i < 64; i++)
		{
			u16 v;

			if (pq)
			{
				v = _jpg_be16(p);
				p += 2;
			}
			else
			{
				v = *p++;
			}

			j->qt[tq][_jpg_zigzag[i]] = v;
		}

		j->qt_valid[tq] = true;

		len -= need;
	}

	return 0;
}

static int _jpg_parse_dht(_jpg_ctx_t *j, const u8 *p, u32 len)
{
	while (len)
	{
		if (len < 17)
			return -1;

		u8 info = *p++;
		len--;

		u8 tc = info >> 4;
		u8 th = info & 0x0F;

		if (tc > 1 || th > 3)
			return -1;

		_jpg_huff_t *h = tc ? &j->huff_ac[th] : &j->huff_dc[th];

		memset(h, 0, sizeof(*h));

		u32 count = 0;

		for (u32 i = 1; i <= 16; i++)
		{
			h->bits[i] = *p++;
			count += h->bits[i];
		}

		len -= 16;

		if (count > 256 || len < count)
			return -1;

		h->count = count;

		u32 code = 0;
		u32 idx = 0;

		for (u32 i = 1; i <= 16; i++)
		{
			h->first_code[i] = (u16)code;
			h->first_idx[i] = (u16)idx;

			idx += h->bits[i];
			code = (code + h->bits[i]) << 1;
		}

		memcpy(h->vals, p, count);

		p += count;
		len -= count;

		if (tc)
			j->huff_ac_valid[th] = true;
		else
			j->huff_dc_valid[th] = true;
	}

	return 0;
}

static int _jpg_parse_sof0( _jpg_ctx_t *j, const u8 *p, u32 len)
{
	if (len < 6)
		return -1;

	if (p[0] != 8)
		return -1;

	j->height = _jpg_be16(p + 1);
	j->width  = _jpg_be16(p + 3);
	j->ncomp  = p[5];

	if (!j->width || !j->height)
		return -1;

	if (j->ncomp != 1 && j->ncomp != 3)
		return -1;

	if (len < 6U + (u32)j->ncomp * 3U)
		return -1;

	j->max_h = 0;
	j->max_v = 0;

	p += 6;

	for (u32 i = 0; i < j->ncomp; i++)
	{
		_jpg_comp_t *c = &j->comp[i];

		c->id = p[0];
		c->h  = p[1] >> 4;
		c->v  = p[1] & 0x0F;
		c->tq = p[2];

		if (!c->h || !c->v || c->h > 4 || c->v > 4 || c->tq > 3)
			return -1;

		if (c->h > j->max_h)
			j->max_h = c->h;

		if (c->v > j->max_v)
			j->max_v = c->v;

		c->dc_pred = 0;

		p += 3;
	}

	return 0;
}

static int _jpg_find_comp( _jpg_ctx_t *j, u8 id)
{
	for (u32 i = 0; i < j->ncomp; i++)
		if (j->comp[i].id == id)
			return i;

	return -1;
}

static int _jpg_parse_sos( _jpg_ctx_t *j, const u8 *p, u32 len)
{
	if (len < 1)
		return -1;

	u8 ns = *p++;

	if (ns != j->ncomp)
		return -1;

	if (len < 1U + (u32)ns * 2U + 3U)
		return -1;

	for (u32 i = 0; i < ns; i++)
	{
		u8 id = p[0];
		u8 ht = p[1];

		int ci = _jpg_find_comp(j, id);

		if (ci < 0)
			return -1;

		j->comp[ci].td = ht >> 4;
		j->comp[ci].ta = ht & 0x0F;

		if (j->comp[ci].td > 3 || j->comp[ci].ta > 3)
			return -1;

		p += 2;
	}

	// Baseline: Ss = 0 Se = 63 Ah/Al = 0
	if (p[0] != 0 || p[1] != 63 || p[2] != 0)
		return -1;

	return 0;
}

static int _jpg_parse( _jpg_ctx_t *j)
{
	const u8 *p = j->jpg;
	const u8 *end = j->jpg + j->size;

	if (j->size < 4 || p[0] != 0xFF || p[1] != 0xD8)
		return -1;

	p += 2;

	bool got_sof = false;

	while (p + 1 < end)
	{
		while (p < end && *p != 0xFF)
			p++;

		if (p >= end)
			break;

		while (p < end && *p == 0xFF)
			p++;

		if (p >= end)
			break;

		u8 marker = *p++;

		if (marker == 0xD9)
			break;

		if (marker >= 0xD0 && marker <= 0xD7)
			continue;

		if (marker == 0x01)
			continue;

		if (p + 2 > end)
			return -1;

		u16 seglen = _jpg_be16(p);
		p += 2;

		if (seglen < 2)
			return -1;

		u32 len = seglen - 2;

		if (p + len > end)
			return -1;

		switch (marker)
		{
		case 0xC0:
			if (_jpg_parse_sof0(j, p, len))
				return -1;

			got_sof = true;
			break;

		case 0xC2:
			// Progressive JPEG.
			return -1;

		case 0xC4:
			if (_jpg_parse_dht(j, p, len))
				return -1;
			break;

		case 0xDB:
			if (_jpg_parse_dqt(j, p, len))
				return -1;
			break;

		case 0xDD:
			if (len != 2)
				return -1;

			j->restart_interval = _jpg_be16(p);
			break;

		case 0xDA:
			if (!got_sof)
				return -1;

			if (_jpg_parse_sos(j, p, len))
				return -1;

			p += len;

			j->scan = p;
			j->scan_size = end - p;

			return 0;

		default:
			break;
		}

		p += len;
	}

	return -1;
}

// Integer IDCT. Kept deliberately simple. JPEG decoding isn't a performance critical path for the file viewer.
static inline u8 _jpg_clamp(int v);
static void _jpg_idct(const s32 *in, u8 *out)
{
#define W1 2841
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

	s32 tmp[64];

	for (u32 i = 0; i < 8; i++)
	{
		const s32 *src = in + i * 8;
		s32 *dst = tmp + i * 8;

		if (!(src[1] | src[2] | src[3] | src[4] | src[5] | src[6] | src[7]))
		{
			s32 dc = src[0] << 3;
			dst[0] = dc;
			dst[1] = dc;
			dst[2] = dc;
			dst[3] = dc;
			dst[4] = dc;
			dst[5] = dc;
			dst[6] = dc;
			dst[7] = dc;
			continue;
		}

		s32 x0 = (src[0] << 11) + 128;
		s32 x1 = src[4] << 11;
		s32 x2 = src[6];
		s32 x3 = src[2];
		s32 x4 = src[1];
		s32 x5 = src[7];
		s32 x6 = src[5];
		s32 x7 = src[3];
		s32 x8;

		x8 = W7 * (x4 + x5);
		x4 = x8 + (W1 - W7) * x4;
		x5 = x8 - (W1 + W7) * x5;

		x8 = W3 * (x6 + x7);
		x6 = x8 - (W3 - W5) * x6;
		x7 = x8 - (W3 + W5) * x7;

		x8 = x0 + x1;
		x0 -= x1;

		x1 = W6 * (x3 + x2);
		x2 = x1 - (W2 + W6) * x2;
		x3 = x1 + (W2 - W6) * x3;

		x1 = x4 + x6;
		x4 -= x6;
		x6 = x5 + x7;
		x5 -= x7;
		x7 = x8 + x3;
		x8 -= x3;
		x3 = x0 + x2;
		x0 -= x2;

		x2 = (181 * (x4 + x5) + 128) >> 8;
		x4 = (181 * (x4 - x5) + 128) >> 8;

		dst[0] = (x7 + x1) >> 8;
		dst[1] = (x3 + x2) >> 8;
		dst[2] = (x0 + x4) >> 8;
		dst[3] = (x8 + x6) >> 8;
		dst[4] = (x8 - x6) >> 8;
		dst[5] = (x0 - x4) >> 8;
		dst[6] = (x3 - x2) >> 8;
		dst[7] = (x7 - x1) >> 8;
	}

	for (u32 x = 0; x < 8; x++)
	{
		s32 *src = tmp + x;

		if (!(src[8] | src[16] | src[24] | src[32] | src[40] | src[48] | src[56]))
		{
			s32 v = ((src[0] + 32) >> 6) + 128;
			u8 c = _jpg_clamp(v);
			out[x] = c;
			out[8 + x] = c;
			out[16 + x] = c;
			out[24 + x] = c;
			out[32 + x] = c;
			out[40 + x] = c;
			out[48 + x] = c;
			out[56 + x] = c;
			continue;
		}

		s32 x0 = (src[0] << 8) + 8192;
		s32 x1 = src[32] << 8;
		s32 x2 = src[48];
		s32 x3 = src[16];
		s32 x4 = src[8];
		s32 x5 = src[56];
		s32 x6 = src[40];
		s32 x7 = src[24];
		s32 x8;

		x8 = W7 * (x4 + x5) + 4;
		x4 = (x8 + (W1 - W7) * x4) >> 3;
		x5 = (x8 - (W1 + W7) * x5) >> 3;

		x8 = W3 * (x6 + x7) + 4;
		x6 = (x8 - (W3 - W5) * x6) >> 3;
		x7 = (x8 - (W3 + W5) * x7) >> 3;

		x8 = x0 + x1;
		x0 -= x1;

		x1 = W6 * (x3 + x2) + 4;
		x2 = (x1 - (W2 + W6) * x2) >> 3;
		x3 = (x1 + (W2 - W6) * x3) >> 3;

		x1 = x4 + x6;
		x4 -= x6;
		x6 = x5 + x7;
		x5 -= x7;
		x7 = x8 + x3;
		x8 -= x3;
		x3 = x0 + x2;
		x0 -= x2;

		x2 = (181 * (x4 + x5) + 128) >> 8;
		x4 = (181 * (x4 - x5) + 128) >> 8;

		out[x] = _jpg_clamp(((x7 + x1) >> 14) + 128);
		out[8 + x] = _jpg_clamp(((x3 + x2) >> 14) + 128);
		out[16 + x] = _jpg_clamp(((x0 + x4) >> 14) + 128);
		out[24 + x] = _jpg_clamp(((x8 + x6) >> 14) + 128);
		out[32 + x] = _jpg_clamp(((x8 - x6) >> 14) + 128);
		out[40 + x] = _jpg_clamp(((x0 - x4) >> 14) + 128);
		out[48 + x] = _jpg_clamp(((x3 - x2) >> 14) + 128);
		out[56 + x] = _jpg_clamp(((x7 - x1) >> 14) + 128);
	}

#undef W1
#undef W2
#undef W3
#undef W5
#undef W6
#undef W7
}

static int _jpg_decode_block(_jpg_ctx_t *j, _jpg_bits_t *bits, _jpg_comp_t *c, u8 *out)
{
	if (!j->qt_valid[c->tq] || !j->huff_dc_valid[c->td] || !j->huff_ac_valid[c->ta])
		return -1;

	s32 block[64];
	block[0] = 0;

	int s = _jpg_huff_decode(bits, &j->huff_dc[c->td]);
	if (s < 0 || s > 11)
		return -1;

	int diff = _jpg_receive_extend(bits, s);
	if (diff == 0x7FFFFFFF)
		return -1;

	c->dc_pred += diff;
	block[0] = (s32)c->dc_pred * j->qt[c->tq][0];

	u32 k = 1;
	bool has_ac = false;
	u32 ac_row_mask = 0;

	while (k < 64)
	{
		int rs = _jpg_huff_decode(bits, &j->huff_ac[c->ta]);
		if (rs < 0)
			return -1;

		if (rs == 0)
			break;

		u32 run = (u32)rs >> 4;
		u32 size = (u32)rs & 0x0F;

		if (!size)
		{
			if (run != 15)
				return -1;

			k += 16;
			if (k > 64)
				return -1;

			continue;
		}

		k += run;
		if (k >= 64)
			return -1;

		int value = _jpg_receive_extend(bits, size);
		if (value == 0x7FFFFFFF)
			return -1;

		u32 dst = _jpg_zigzag[k];

		if (!has_ac)
			memset(&block[1], 0, 63 * sizeof(block[0]));

		block[dst] = (s32)value * j->qt[c->tq][dst];
		has_ac = true;
		ac_row_mask |= 1U << (dst >> 3);

		k++;
	}

	// Entire block is DC-only.
	if (!has_ac)
	{
		s32 value = ((block[0] + 4) >> 3) + 128;
		if (value < 0)
			value = 0;
		else if (value > 255)
			value = 255;
		memset(out, (u8)value, 64);
	}
	// All non-zero AC coefficients are in frequency row 0. Only one horizontal IDCT row is required.
	else if ((ac_row_mask & ~1U) == 0)
	{
		static const s16 c[8][8] =
		{
			{ 181, 251, 237, 213, 181, 142, 98, 50 },
			{ 181, 213, 98, -50, -181, -251, -237, -142 },
			{ 181, 142, -98, -251, -181, 50, 237, 213 },
			{ 181, 50, -237, -142, 181, 213, -98, -251 },
			{ 181, -50, -237, 142, 181, -213, -98, 251 },
			{ 181, -142, -98, 251, -181, -50, 237, -213 },
			{ 181, -213, 98, 50, -181, 251, -237, 142 },
			{ 181, -251, 237, -213, 181, -142, 98, -50 }
		};

		s64 tmp[8];
		s64 dc = (s64)block[0] * 181;

		for (u32 x = 0; x < 8; x++)
		{
			s64 sum = dc;
			if (block[1]) sum += (s64)block[1] * c[x][1];
			if (block[2]) sum += (s64)block[2] * c[x][2];
			if (block[3]) sum += (s64)block[3] * c[x][3];
			if (block[4]) sum += (s64)block[4] * c[x][4];
			if (block[5]) sum += (s64)block[5] * c[x][5];
			if (block[6]) sum += (s64)block[6] * c[x][6];
			if (block[7]) sum += (s64)block[7] * c[x][7];
			tmp[x] = sum;
		}

		for (u32 x = 0; x < 8; x++)
		{
			s32 value = (s32)(((tmp[x] * 181) + (1LL << 17)) >> 18) + 128;
			if (value < 0)
				value = 0;
			else if (value > 255)
				value = 255;
			for (u32 y = 0; y < 8; y++)
				out[y * 8 + x] = (u8)value;
		}
	}
	else
	{
		_jpg_idct(block, out);
	}

	return 0;
}

static int _jpg_skip_restart( _jpg_bits_t *b)
{
	// Discard remaining entropy bits.
	b->bitbuf = 0;
	b->bitcnt = 0;

	u32 p = b->pos;

	while (p < b->size && b->data[p] != 0xFF)
		p++;

	if (p >= b->size)
		return -1;

	while (p < b->size && b->data[p] == 0xFF)
		p++;

	if (p >= b->size)
		return -1;

	u8 marker = b->data[p++];

	if (marker < 0xD0 || marker > 0xD7)
		return -1;

	b->pos = p;

	return 0;
}

static inline u8 _jpg_clamp(int v)
{
	if (v < 0)
		return 0;
	if (v > 255)
		return 255;
	return (u8)v;
}

static s32 _jpg_cr_r[256];
static s32 _jpg_cb_g[256];
static s32 _jpg_cr_g[256];
static s32 _jpg_cb_b[256];

static bool _jpg_color_lut_ready = false;

static void _jpg_color_lut_init(void)
{
	if (_jpg_color_lut_ready)
		return;

	for (u32 i = 0; i < 256; i++)
	{
		int v = (int)i - 128;

		_jpg_cr_r[i] = (91881 * v) >> 16;

		_jpg_cb_g[i] = 22554 * v;

		_jpg_cr_g[i] = 46802 * v;

		_jpg_cb_b[i] = (116130 * v) >> 16;
	}

	_jpg_color_lut_ready = true;
}

static inline void _jpg_store_rgb(u8 *dst, int y, int cb, int cr)
{
	int r = y + _jpg_cr_r[cr];
	int g = y - ((_jpg_cb_g[cb] + _jpg_cr_g[cr]) >> 16);
	int b = y + _jpg_cb_b[cb];

	dst[0] = _jpg_clamp(b);
	dst[1] = _jpg_clamp(g);
	dst[2] = _jpg_clamp(r);
	dst[3] = 0xFF;
}

static int _jpg_decode_image( _jpg_ctx_t *j, u8 *dst, u32 dst_w, u32 dst_h)
{
	_jpg_bits_t bits;

	memset(&bits, 0, sizeof(bits));

	_jpg_color_lut_init();

	bits.data = j->scan;
	bits.size = j->scan_size;

	u32 mcu_w = j->max_h * 8;
	u32 mcu_h = j->max_v * 8;

	u32 mcus_x = (j->width + mcu_w - 1) / mcu_w;

	u32 mcus_y = (j->height + mcu_h - 1) / mcu_h;

	// Precalculate destination -> source coordinates. The old code performed these divisions for every output pixel inside every MCU.
	u16 x_map[1280];
	u16 y_map[1280];

	if (dst_w > 1280 || dst_h > 1280)
		return -1;

	u32 sx = 0;
	u32 x_err = 0;

	for (u32 x = 0; x < dst_w; x++)
	{
		x_map[x] = sx;
		x_err += j->width;
		while (x_err >= dst_w)
		{
			x_err -= dst_w;
			sx++;
		}
	}

	u32 sy = 0;
	u32 y_err = 0;

	for (u32 y = 0; y < dst_h; y++)
	{
		y_map[y] = sy;
		y_err += j->height;
		while (y_err >= dst_h)
		{
			y_err -= dst_h;
			sy++;
		}
	}

	// Maximum baseline sampling we're accepting is 4x4, although normal JPEGs are typically <= 2x2.
	u8 blocks[3][16][64];

	// Component sampling positions are constant for the entire JPEG, so calculate them only once.
	u8 comp_bx[3][32];
	u8 comp_ox[3][32];
	u8 comp_by[3][32];
	u8 comp_row[3][32];

	for (u32 ci = 0; ci < j->ncomp; ci++)
	{
		_jpg_comp_t *c = &j->comp[ci];

		for (u32 px = 0; px < mcu_w; px++)
		{
			u8 sx = (u8)((px * c->h) / j->max_h);
			comp_bx[ci][px] = sx >> 3;
			comp_ox[ci][px] = sx & 7;
		}

		for (u32 py = 0; py < mcu_h; py++)
		{
			u8 sy = (u8)((py * c->v) / j->max_v);
			comp_by[ci][py] = sy >> 3;
			comp_row[ci][py] = (sy & 7) * 8;
		}
	}

	_jpg_comp_t *c0 = &j->comp[0];
	_jpg_comp_t *c1 = j->ncomp == 3 ? &j->comp[1] : NULL;
	_jpg_comp_t *c2 = j->ncomp == 3 ? &j->comp[2] : NULL;

	u8 *bx0 = comp_bx[0];
	u8 *ox0 = comp_ox[0];
	u8 *bx1 = j->ncomp == 3 ? comp_bx[1] : NULL;
	u8 *ox1 = j->ncomp == 3 ? comp_ox[1] : NULL;
	u8 *bx2 = j->ncomp == 3 ? comp_bx[2] : NULL;
	u8 *ox2 = j->ncomp == 3 ? comp_ox[2] : NULL;

	u32 restart_count = 0;

	for (u32 my = 0; my < mcus_y; my++)
	{
		for (u32 mx = 0; mx < mcus_x; mx++)
		{
			// Decode all component blocks belonging to this MCU.
			for (u32 ci = 0; ci < j->ncomp; ci++)
			{
				_jpg_comp_t *c = &j->comp[ci];

				u32 count = c->h * c->v;

				if (count > 16)
					return -1;

				for (u32 bi = 0; bi < count; bi++)
				{
					if (_jpg_decode_block(j, &bits, c, blocks[ci][bi]))
						return -1;
				}
			}

			u32 src_x0 = mx * mcu_w;

			u32 src_y0 = my * mcu_h;

			u32 src_x1 = src_x0 + mcu_w;

			u32 src_y1 = src_y0 + mcu_h;

			if (src_x1 > j->width)
				src_x1 = j->width;

			if (src_y1 > j->height)
				src_y1 = j->height;

			// Destination range belonging to this MCU.
			u32 dx0 = (u32)(
					((u64)src_x0 * dst_w + j->width - 1) /
					j->width);

			u32 dy0 = (u32)(
					((u64)src_y0 * dst_h + j->height - 1) /
					j->height);

			u32 dx1 = (u32)(
					((u64)src_x1 * dst_w + j->width - 1) /
					j->width);

			u32 dy1 = (u32)(
					((u64)src_y1 * dst_h + j->height - 1) /
					j->height);

			if (dx1 > dst_w)
				dx1 = dst_w;

			if (dy1 > dst_h)
				dy1 = dst_h;

			u8 px_map[32];
			u8 py_map[32];

			for (u32 dx = dx0; dx < dx1; dx++)
				px_map[dx - dx0] = (u8)(x_map[dx] - src_x0);

			for (u32 dy = dy0; dy < dy1; dy++)
				py_map[dy - dy0] = (u8)(y_map[dy] - src_y0);

			for (u32 dy = dy0; dy < dy1; dy++)
			{
				u32 py = py_map[dy - dy0];

				u8 *out = dst +
					((dy * dst_w + dx0) * 4);

				if (j->ncomp == 1)
				{
					u32 base = comp_by[0][py] * c0->h;
					u32 row_offset = comp_row[0][py];

					for (u32 dx = dx0; dx < dx1; dx++)
					{
						u32 px = px_map[dx - dx0];

						u32 bi = base + bx0[px];
						u8 y = blocks[0][bi][row_offset + ox0[px]];

						out[0] = y;
						out[1] = y;
						out[2] = y;
						out[3] = 0xFF;

						out += 4;
					}
				}
				else
				{
					// Y/Cb/Cr vertical positions are constant for the whole output row.
					u32 base0 = comp_by[0][py] * c0->h;
					u32 base1 = comp_by[1][py] * c1->h;
					u32 base2 = comp_by[2][py] * c2->h;

					u32 row0 = comp_row[0][py];
					u32 row1 = comp_row[1][py];
					u32 row2 = comp_row[2][py];

					for (u32 dx = dx0; dx < dx1; dx++)
					{
						u32 px = px_map[dx - dx0];

						u32 bi0 = base0 + bx0[px];
						u32 bi1 = base1 + bx1[px];
						u32 bi2 = base2 + bx2[px];

						int y = blocks[0][bi0][row0 + ox0[px]];
						int cb = blocks[1][bi1][row1 + ox1[px]];
						int cr = blocks[2][bi2][row2 + ox2[px]];

						_jpg_store_rgb(out, y, cb, cr);
						out += 4;
					}
				}
			}

			if (j->restart_interval)
			{
				restart_count++;

				if (restart_count == j->restart_interval)
				{
					// A restart marker exists BETWEEN restart intervals, not necessarily after the final MCU.
					bool last_mcu = (mx + 1 == mcus_x) &&
						(my + 1 == mcus_y);

					if (!last_mcu)
					{
						if (_jpg_skip_restart(&bits))
							return -1;

						for (u32 i = 0; i < j->ncomp; i++)
						{
							j->comp[i].dc_pred = 0;
						}
					}

					restart_count = 0;
				}
			}
		}
	}

	return 0;
}

lv_img_dsc_t *jpg_to_lvimg_obj(const char *path)
{
	u32 fsize;

	u8 *jpg = sd_file_read(path, &fsize);

	if (!jpg)
		return NULL;

	_jpg_ctx_t j;

	memset(&j, 0, sizeof(j));

	j.jpg = jpg;
	j.size = fsize;

	if (_jpg_parse(&j))
	{
		free(jpg);
		return NULL;
	}

	// Only grayscale and normal three-component JPEGs.
	if (j.ncomp != 1 && j.ncomp != 3)
	{
		free(jpg);
		return NULL;
	}

	// Make sure every required table exists.
	for (u32 i = 0; i < j.ncomp; i++)
	{
		_jpg_comp_t *c = &j.comp[i];

		if (!j.qt_valid[c->tq] || !j.huff_dc_valid[c->td] || !j.huff_ac_valid[c->ta])
		{
			free(jpg);
			return NULL;
		}
	}

	// Keep enough resolution for both normal and 90/270 degree rotated display. Fit inside 1280x1280 without upscaling.
	u32 out_width = j.width;
	u32 out_height = j.height;

	if (out_width > LV_HOR_RES || out_height > LV_HOR_RES)
	{
		u32 max_dim = out_width > out_height ?
			out_width : out_height;

		u64 scale = ((u64)LV_HOR_RES << 16) /
			max_dim;

		out_width = (u32)(((u64)j.width * scale) >> 16);

		out_height = (u32)(((u64)j.height * scale) >> 16);

		if (!out_width)
			out_width = 1;
		if (!out_height)
			out_height = 1;
	}

	if (out_width > 0xFFFFFFFFU / 4)
	{
		free(jpg);
		return NULL;
	}

	u32 stride = out_width * 4;

	if (out_height > 0xFFFFFFFFU / stride)
	{
		free(jpg);
		return NULL;
	}

	u32 data_size = stride * out_height;

	if (data_size > 0xFFFFFFFFU - sizeof(lv_img_dsc_t) - 0x10)
	{
		free(jpg);
		return NULL;
	}

	u32 alloc_size = sizeof(lv_img_dsc_t) +
		0x10 + data_size;

	u8 *mem = malloc(alloc_size);

	if (!mem)
	{
		free(jpg);
		return NULL;
	}

	lv_img_dsc_t *img = (lv_img_dsc_t *)mem;
	memset(img, 0, sizeof(*img));

	uptr data_addr = ALIGN(
			(uptr)mem + sizeof(lv_img_dsc_t),
			0x10);

	u8 *pixels = (u8 *)data_addr;

	img->header.always_zero = 0;
	img->header.w = out_width;
	img->header.h = out_height;
	img->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;

	img->data_size = data_size;
	img->data = pixels;

	if (_jpg_decode_image( &j, pixels, out_width, out_height))
	{
		free(mem);
		free(jpg);
		return NULL;
	}

	free(jpg);

	return img;
}

//==================================
//  ASAP: PWR, VOL buttons config.
//==================================
const char *gui_pv_btn(gui_pv_btn_t type)
{
	static bool is_aula = false;
	static bool inited = false;

	if (!inited) {
		is_aula = (fuse_read_hw_type() == FUSE_NX_HW_TYPE_AULA);
		inited = true;
	}

	switch (type) {
		case GUI_PV_BTN_0:
			return is_aula ? "Ⓑ" : "Ⓦ";
		case GUI_PV_BTN_1:
			return is_aula ? "Ⓜ" : "Ⓧ";
		case GUI_PV_BTN_2:
			return is_aula ? "Ⓟ" : "Ⓩ";
		case GUI_PV_BTN_3:
			return is_aula ? "#EFEFEF Ⓜ#" : "#EFEFEF Ⓧ##D8D8D8 Ⓨ#";
		case GUI_PV_BTN_4:
			return is_aula ? "#EFEFEF Ⓟ#" : "#D8D8D8 Ⓨ##EFEFEF Ⓩ#";

		default:
			return "?";
	}
}
const char *gui_pv_btn_pair(gui_pv_btn_t a, gui_pv_btn_t b)
{
	static bool is_aula = false;
	static bool inited = false;
	static char buf[16];

	if (!inited) {
		is_aula = (fuse_read_hw_type() == FUSE_NX_HW_TYPE_AULA);
		inited = true;
	}

	if (is_aula) {
		s_printf(buf, "%s%s", gui_pv_btn(a), gui_pv_btn(b));
		return buf;
	}

	if ((a == GUI_PV_BTN_3 && b == GUI_PV_BTN_4) || (a == GUI_PV_BTN_4 && b == GUI_PV_BTN_3)) {
		return "#EFEFEF Ⓧ##CCCCCC Ⓨ##EFEFEF Ⓩ#";
	}

	s_printf(buf, "%s%s", gui_pv_btn(a), gui_pv_btn(b));

	return buf;
}
//==================================

lv_res_t nyx_generic_onoff_toggle(lv_obj_t *btn)
{
	lv_obj_t *label_btn = lv_obj_get_child(btn, NULL);
	lv_obj_t *label_btn2 = lv_obj_get_child(btn, label_btn);

	char label_text[64];
	if (!label_btn2)
	{
		strcpy(label_text, lv_label_get_text(label_btn));
		label_text[strlen(label_text) - 15] = 0;

		if (!(lv_btn_get_state(btn) & LV_BTN_STATE_TGL_REL))
		{
			strcat(label_text, "#D0D0D0    OFF#");
			lv_label_set_text(label_btn, label_text);
		}
		else
		{
			s_printf(label_text, "%s%s%s", label_text, text_color, "    ON #");
			lv_label_set_text(label_btn, label_text);
		}
	}
	else
	{
		if (!(lv_btn_get_state(btn) & LV_BTN_STATE_TGL_REL))
			lv_label_set_text(label_btn, "#D0D0D0 OFF#");
		else
		{
			s_printf(label_text, "%s%s", text_color, " ON #");
			lv_label_set_text(label_btn, label_text);
		}
	}

	return LV_RES_OK;
}

lv_res_t nyx_mbox_action(lv_obj_t *btns, const char *txt)
{
	lv_obj_t *mbox = lv_mbox_get_from_btn(btns);
	lv_obj_t *dark_bg = lv_obj_get_parent(mbox);

	lv_obj_del(dark_bg); // Deletes children also (mbox).

	return LV_RES_INV;
}

bool nyx_emmc_check_battery_enough()
{
	if (h_cfg.devmode)
		return true;

	int batt_volt = 0;

	max17050_get_property(MAX17050_VCELL, &batt_volt);

	if (batt_volt && batt_volt < 3650)
	{
		lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
		lv_obj_set_style(dark_bg, &mbox_darken);
		lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

		static const char * mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
		lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
		lv_mbox_set_recolor_text(mbox, true);

		lv_mbox_set_text(mbox,
			"#008EED 배터리 부족 경고#\n\n"
			"#FFBA00 안내#: 배터리 잔량이 부족하여 작업을 수행할 수 없습니다!\n"
			"전압을 최소 #C7EA46 3650 mV# 이상 올린 후, 다시 시도하세요!");

		lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);
		lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
		lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
		lv_obj_set_top(mbox, true);

		return false;
	}

	return true;
}

static void _nyx_sd_card_issues_warning(void *param)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char * mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);

	lv_mbox_set_text(mbox,
		"#008EED SD 카드 경고#\n\n"
		"#FFBA00 안내#: SD 카드가 1-bit 모드로 초기화 되었습니다!\n"
		"#FF8000 커넥터가 분리되었거나 손상되었을 수 있습니다!#\n\n"
		"#C7EA46 낸드 매니저#의 #C7EA46 Ⓢ#에서 정보를 확인하세요.");

	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);
}

void nyx_window_toggle_buttons(lv_obj_t *win, bool disable)
{
	lv_win_ext_t * ext = lv_obj_get_ext_attr(win);
	lv_obj_t * hbtn;

	hbtn = lv_obj_get_child_back(ext->header, NULL);
	hbtn = lv_obj_get_child_back(ext->header, hbtn); // Skip the title.

	if (disable)
	{
		while (hbtn != NULL)
		{
			lv_obj_set_opa_scale(hbtn, LV_OPA_40);
			lv_obj_set_opa_scale_enable(hbtn, true);
			lv_obj_set_click(hbtn, false);
			hbtn = lv_obj_get_child_back(ext->header, hbtn);
		}
	}
	else
	{
		while (hbtn != NULL)
		{
			lv_obj_set_opa_scale(hbtn, LV_OPA_COVER);
			lv_obj_set_click(hbtn, true);
			hbtn = lv_obj_get_child_back(ext->header, hbtn);
		}
	}
}

lv_res_t nyx_win_close_action(lv_obj_t * btn)
{
	autorcm_btn = NULL;
	close_btn = NULL;

	return lv_win_close_action(btn);
}

//============================
//  ASAP: Nyx common window.
//============================
static lv_obj_t *_nyx_create_window(const char *win_title, lv_action_t close_action)
{
	static lv_style_t win_bg_style;

	lv_style_copy(&win_bg_style, &lv_style_plain);
	win_bg_style.body.main_color = lv_theme_get_current()->bg->body.main_color;
	win_bg_style.body.grad_color = win_bg_style.body.main_color;

	lv_obj_t *win = lv_win_create(lv_scr_act(), NULL);
	lv_win_set_title(win, win_title);
	lv_win_set_style(win, LV_WIN_STYLE_BG, &win_bg_style);
	lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);

	close_btn = lv_win_add_btn(win, NULL, SYMBOL_CLOSE " 닫기", close_action);

	return win;
}
//============================

lv_obj_t *nyx_create_standard_window(const char *win_title, lv_action_t close_action)
{
	static lv_style_t win_bg_style;

	lv_style_copy(&win_bg_style, &lv_style_plain);
	win_bg_style.body.main_color = lv_theme_get_current()->bg->body.main_color;
	win_bg_style.body.grad_color = win_bg_style.body.main_color;

	lv_obj_t *win = lv_win_create(lv_scr_act(), NULL);
	lv_win_set_title(win, win_title);
	lv_win_set_style(win, LV_WIN_STYLE_BG, &win_bg_style);
	lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);

	if (!close_action)
		close_btn = lv_win_add_btn(win, NULL, SYMBOL_CLOSE" 닫기", nyx_win_close_action);
	else
		close_btn = lv_win_add_btn(win, NULL, SYMBOL_CLOSE" 닫기", close_action);

	return win;
}

//===============================================
//  ASAP: DUALNAND MANAGER exit for nyx reload.
//===============================================
lv_obj_t *nyx_create_file_browser_window(const char *path, lv_action_t close_action)
{
	char title[1024 + 16];

	if (path[1] == 0)
		strcpy(title, SYMBOL_DIRECTORY "  sdmc:/");
	else
		s_printf(title, SYMBOL_DIRECTORY "  sdmc:%s", path);

	return nyx_create_standard_window(title, close_action);
}

//===============================================
//  ASAP: DUALNAND MANAGER exit for nyx reload.
//===============================================
static lv_res_t refresh_nandinfo(lv_obj_t *btn)
{
	lv_res_t res = lv_win_close_action(btn);
	close_btn = NULL;
	
	refresh_emu_enabled_label();
	refresh_nand_info_label();

	return res;
}

lv_obj_t *nyx_create_nand_manager_window(const char *win_title)
{
	return _nyx_create_window(win_title, refresh_nandinfo);
}
//===============================================

void reload_nyx(lv_obj_t *obj, bool force)
{
	if (!force)
	{
		sd_mount();

		// Check that Nyx still exists.
		if (f_stat("bootloader/sys/nyx.bin", NULL))
		{
			sd_unmount();

			// Remove lvgl object in case of being invoked from a window.
			if (obj)
				lv_obj_del(obj);

			do_auto_reload = false;

			return;
		}
	}

	b_cfg->boot_cfg = BOOT_CFG_AUTOBOOT_EN;
	b_cfg->autoboot = 0;
	b_cfg->autoboot_list = 0;
	b_cfg->extra_cfg = 0;

	void (*main_ptr)() = (void *)nyx_str->hekate;

	sd_end();

	hw_deinit(false);

	(*main_ptr)();
}

//===================================
//  ASAP: Direct reload nyx action.
//===================================
lv_res_t reload_action(lv_obj_t *btns)
{
	reload_nyx(NULL, false);
	return LV_RES_OK;
}
//===================================

static lv_res_t _removed_sd_action(lv_obj_t *btns, const char *txt)
{
	u32 btnidx = lv_btnm_get_pressed(btns);

	switch (btnidx)
	{
	case 0:
		if (h_cfg.rcm_patched)
			power_set_state(POWER_OFF_REBOOT);
		else
			power_set_state(REBOOT_RCM);
		break;
	case 1:
		power_set_state(POWER_OFF_RESET);
		break;
	case 2:
		sd_end();
		do_auto_reload = false;
		break;
	}

	return nyx_mbox_action(btns, txt);
}

static void _check_sd_card_removed(void *params)
{
	static lv_obj_t *dark_bg = NULL;

	// The following checks if SDMMC_1 is initialized.
	// If yes and card was removed, shows a message box,
	// that will reload Nyx, when the card is inserted again.
	if (!do_auto_reload && sd_get_card_removed())
	{
		dark_bg = lv_obj_create(lv_scr_act(), NULL);
		lv_obj_set_style(dark_bg, &mbox_darken);
		lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

		static const char * mbox_btn_map[] = { "\221Ｒ", "\221종료", "\221확인", "" };
		static const char * mbox_btn_map_rcm_patched[] = { "\221재부팅", "\221종료", "\221확인", "" };
		lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
		lv_mbox_set_recolor_text(mbox, true);
		lv_obj_set_width(mbox, LV_HOR_RES * 6 / 9);

		lv_mbox_set_text(mbox,
						 "\n#008EED 상태 메시지#\n\n"
						 "#FFBA00 안내#: SD 카드가 본체에서 제거되었습니다!\n\n"
						 "#FF8000 경고:#\n#FF8000 일부 기능이 제한됩니다.#\n"
						 "#FF8000 정상 동작을 위해 SD 카드를 재삽입하세요!#");
		lv_mbox_add_btns(mbox, h_cfg.rcm_patched ? mbox_btn_map_rcm_patched : mbox_btn_map, _removed_sd_action);

		lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
		lv_obj_set_top(mbox, true);

		do_auto_reload = true;
	}

	// If in reload state and card was inserted, reload nyx.
	if (do_auto_reload && !sd_get_card_removed())
		reload_nyx(dark_bg, false);
}

lv_task_t *task_emmc_errors;
static void _nyx_emmc_issues_warning(void *params)
{
	if (emmc_get_mode() < EMMC_MMC_HS400)
	{
		// Remove task.
		lv_task_del(task_emmc_errors);

		lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
		lv_obj_set_style(dark_bg, &mbox_darken);
		lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

		static const char * mbox_btn_map[] = { "\251", "\222확인", "\251", "" };
		lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
		lv_mbox_set_recolor_text(mbox, true);

		lv_mbox_set_text(mbox,
			"#008EED eMMC 경고#\n\n"
			"#FFBA00 안내#: eMMC가 슬로우 모드로 초기화 되었습니다!\n"
			"#FF8000 하드웨어에 문제가 있을 수 있습니다!#\n\n"
			"#C7EA46 낸드 매니저#의 #C7EA46 Ⓝ#에서 정보를 확인하세요.");

		lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);
		lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
		lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
		lv_obj_set_top(mbox, true);
	}
}

//==================================================================================
//  ASAP: CFW/OFW/RCM boot, shutdown process - Updated hekate & nyx 6.5.2 & 1.9.2.
//==================================================================================
// Convert DRAM density to 0.5GB units.
static u32 density_to_halfgb(u8 density)
{
	switch (density) {
	case 2: return 1; // 512MB
	case 4: return 2; // 1GB
	case 6: return 4; // 2GB
	default: return 0;
	}
}
// Calculate RAM size per side.
static u32 side_ram_halfgb(bool chip1)
{
	emc_mr_data_t ram_vendor  = sdram_read_mrx(MR5_MAN_ID);
	emc_mr_data_t ram_rev0    = sdram_read_mrx(MR6_REV_ID1);
	emc_mr_data_t ram_rev1    = sdram_read_mrx(MR7_REV_ID2);
	emc_mr_data_t ram_density = sdram_read_mrx(MR8_DENSITY);

	u32 channels = (EMC(EMC_FBIO_CFG7) >> 1) & 3;
	channels = (channels & 1) + ((channels & 2) >> 1);

	u32 actual_ranks =
		(chip1
		 ? (ram_vendor.chip1.rank0_ch0 == ram_vendor.chip1.rank1_ch0 &&
			ram_vendor.chip1.rank0_ch1 == ram_vendor.chip1.rank1_ch1 &&
			ram_rev0.chip1.rank0_ch0 == ram_rev0.chip1.rank1_ch0 &&
			ram_rev0.chip1.rank0_ch1 == ram_rev0.chip1.rank1_ch1 &&
			ram_rev1.chip1.rank0_ch0 == ram_rev1.chip1.rank1_ch0 &&
			ram_rev1.chip1.rank0_ch1 == ram_rev1.chip1.rank1_ch1 &&
			ram_density.chip1.rank0_ch0 == ram_density.chip1.rank1_ch0 &&
			ram_density.chip1.rank0_ch1 == ram_density.chip1.rank1_ch1)
		 : (ram_vendor.chip0.rank0_ch0 == ram_vendor.chip0.rank1_ch0 &&
			ram_vendor.chip0.rank0_ch1 == ram_vendor.chip0.rank1_ch1 &&
			ram_rev0.chip0.rank0_ch0 == ram_rev0.chip0.rank1_ch0 &&
			ram_rev0.chip0.rank0_ch1 == ram_rev0.chip0.rank1_ch1 &&
			ram_rev1.chip0.rank0_ch0 == ram_rev1.chip0.rank1_ch0 &&
			ram_rev1.chip0.rank0_ch1 == ram_rev1.chip0.rank1_ch1 &&
			ram_density.chip0.rank0_ch0 == ram_density.chip0.rank1_ch0 &&
			ram_density.chip0.rank0_ch1 == ram_density.chip0.rank1_ch1))
		? 2 : 1;

	u8 density = chip1
		? (ram_density.chip1.rank0_ch0 & 0x3C) >> 2
		: (ram_density.chip0.rank0_ch0 & 0x3C) >> 2;

	return actual_ranks * channels * density_to_halfgb(density);
}
// Check for a valid 8GB configuration.
bool is_8gb_case(void)
{
	u32 left  = side_ram_halfgb(false);
	u32 right = side_ram_halfgb(true);

	return (left == right) && (left == 8);
}
// Check for current RAM mode.
static bool is_current_ram_mode(void)
{
	FILINFO fno;
	bool ret = false;

	if (sd_mount())
		return false;

	if (f_chdrive("sd:") != FR_OK)
		goto out;

	bool has_ram_8gb_ini =
		(f_stat("config/ultrahand/ram_8gb.ini", &fno) == FR_OK);

	bool enable_mem_mode = false;
	const char *ini_path = NULL;

	if (f_stat("atmosphere/config/exosphere.ini", &fno) == FR_OK)
		ini_path = "atmosphere/config/exosphere.ini";
	else if (f_stat("exosphere.ini", &fno) == FR_OK)
		ini_path = "exosphere.ini";

	if (ini_path)
	{
		LIST_INIT(ini_sections);

		if (!ini_parse(&ini_sections, ini_path, false))
		{
			bool found = false;

			LIST_FOREACH_ENTRY(ini_sec_t, sec, &ini_sections, link)
			{
				if (sec->type != INI_CHOICE || strcmp(sec->name, "exosphere"))
					continue;

				LIST_FOREACH_ENTRY(ini_kv_t, kv, &sec->kvs, link)
				{
					if (!strcmp(kv->key, "enable_8gb_mem_mode"))
					{
						enable_mem_mode = (atoi(kv->val) != 0);
						found = true;
						break;
					}
				}

				if (found)
					break;
			}
		}
	}

	ret = has_ram_8gb_ini && enable_mem_mode;

out:
	sd_unmount();
	return ret;
}
// File - copy/paste.
static bool sd_copy_file_mounted(const char *src, const char *dst)
{
	FIL fsrc, fdst;
	FRESULT fr;
	UINT br, bw;
	u8 buf[0x4000];

	fr = f_open(&fsrc, src, FA_READ);
	if (fr != FR_OK) {
		g_restore_fr = fr;
		g_restore_step = src;
		return false;
	}

	fr = f_open(&fdst, dst, FA_WRITE | FA_CREATE_ALWAYS);
	if (fr != FR_OK) {
		g_restore_fr = fr;
		g_restore_step = dst;
		f_close(&fsrc);
		return false;
	}

	for (;;) {
		fr = f_read(&fsrc, buf, sizeof(buf), &br);
		if (fr != FR_OK) {
			g_restore_fr = fr;
			g_restore_step = "파일을 읽을 수 없습니다!";
			break;
		}
		if (br == 0)
			break;

		fr = f_write(&fdst, buf, br, &bw);
		if (fr != FR_OK || bw != br) {
			g_restore_fr = fr;
			g_restore_step = "파일 쓰기에 실패했습니다!";
			break;
		}
	}

	if (fr == FR_OK)
		f_sync(&fdst);

	f_close(&fsrc);
	f_close(&fdst);

	return (fr == FR_OK);
}
// Memory mode.
static bool set_exosphere_mem_mode(int mode)
{
	FIL rfp, wfp;
	FRESULT res;
	UINT br, bw;
	const size_t BUF_SIZE = 16 * 1024;

	const char *ini_path = NULL;
	FILINFO fno;

	if (f_stat("atmosphere/config/exosphere.ini", &fno) == FR_OK)
		ini_path = "atmosphere/config/exosphere.ini";
	else if (f_stat("exosphere.ini", &fno) == FR_OK)
		ini_path = "exosphere.ini";
	else
		return false;

	res = f_open(&rfp, ini_path, FA_READ);
	if (res != FR_OK)
		return false;

	char *buf = (char *)malloc(BUF_SIZE);
	if (!buf) {
		f_close(&rfp);
		return false;
	}

	f_read(&rfp, buf, BUF_SIZE - 1, &br);
	buf[br] = 0;
	f_close(&rfp);

	char *out = (char *)malloc(BUF_SIZE);
	if (!out) {
		free(buf);
		return false;
	}
	out[0] = 0;

	char *sec = strstr(buf, "[exosphere]");

	if (!sec)
	{
		strcpy(out, buf);

		if (strlen(out) && out[strlen(out) - 1] != '\n')
			strcat(out, "\n");

		strcat(out, "[exosphere]\n");
		strcat(out, "enable_40mb_mem_mode=0\n");

		char tmp[64];
		s_printf(tmp, "enable_8gb_mem_mode=%d\n", mode);
		strcat(out, tmp);
	}
	else
	{
		char *sec_end = strstr(sec + 1, "\n[");
		if (!sec_end)
			sec_end = buf + strlen(buf);

		/* before [exosphere] */
		strncat(out, buf, sec - buf);

		strcat(out, "[exosphere]\n");

		bool has40 = false;
		bool has8  = false;

		char *line = strchr(sec, '\n');
		if (line)
			line++;
		else
			line = sec_end;

		while (line < sec_end)
		{
			char *next = strchr(line, '\n');
			if (!next || next > sec_end)
				next = sec_end;

			size_t len = next - line;

			if (!strncmp(line, "enable_40mb_mem_mode=", strlen("enable_40mb_mem_mode=")))
			{
				strcat(out, "enable_40mb_mem_mode=0\n");
				has40 = true;
			}
			else if (!strncmp(line, "enable_8gb_mem_mode=", strlen("enable_8gb_mem_mode=")))
			{
				char tmp[64];
				s_printf(tmp, "enable_8gb_mem_mode=%d\n", mode);
				strcat(out, tmp);
				has8 = true;
			}
			else if (len)
			{
				strncat(out, line, len);
				strcat(out, "\n");
			}

			if (*next == '\n')
				next++;

			line = next;
		}

		if (!has40)
			strcat(out, "enable_40mb_mem_mode=0\n");

		if (!has8)
		{
			char tmp[64];
			s_printf(tmp, "enable_8gb_mem_mode=%d\n", mode);
			strcat(out, tmp);
		}

		/* after [exosphere] */
		strcat(out, sec_end);
	}

	res = f_open(&wfp, ini_path, FA_WRITE | FA_CREATE_ALWAYS);
	if (res != FR_OK)
	{
		free(out);
		free(buf);
		return false;
	}

	f_write(&wfp, out, strlen(out), &bw);
	f_close(&wfp);

	free(out);
	free(buf);

	return true;
}
// Restore to a environment when a DRAM mismatch is detected.
static bool _restore_ram_mode(ram_mode_t mode)
{
	const char *src_hekate;
	const char *src_ram_ini;
	const char *dst_ram_ini;

	const char *dst_payload = "payload.bin";
	const char *dst_update  = "bootloader/update.bin";

	if (mode == RAM_MODE_4GB) {
		src_hekate     = "switch/.packages/.offload/ram_expansion/hekate_4gb.bin";
		src_ram_ini    = "config/ultrahand/ram_8gb.ini";
		dst_ram_ini    = "config/ultrahand/ram_4gb.ini";
	} else {
		src_hekate     = "switch/.packages/.offload/ram_expansion/hekate_8gb.bin";
		src_ram_ini    = "config/ultrahand/ram_4gb.ini";
		dst_ram_ini    = "config/ultrahand/ram_8gb.ini";
	}

	bool ok = false;

	if (sd_mount()) {
		g_restore_fr = FR_NOT_READY;
		g_restore_step = "SD 카드 마운트 실패!";
		return false;
	}

	if (f_chdrive("sd:") != FR_OK) {
		g_restore_fr = FR_NOT_READY;
		g_restore_step = "SD 드라이브 전환 실패!";
		goto out;
	}

	g_restore_step = "hekate 복원 실패!";
	if (!sd_copy_file_mounted(src_hekate, dst_payload))
		goto out;
	if (!sd_copy_file_mounted(src_hekate, dst_update))
		goto out;

	g_restore_step = "exosphere 설정 전환 실패!";
	if (!set_exosphere_mem_mode(mode == RAM_MODE_4GB ? 0 : 1))
		goto out;

	g_restore_step = "RAM 설정 전환 실패!";

	FILINFO fno;

	bool src_exists = (f_stat(src_ram_ini, &fno) == FR_OK);
	bool dst_exists = (f_stat(dst_ram_ini, &fno) == FR_OK);

	f_unlink(dst_ram_ini);

	if (src_exists) {
		if (f_rename(src_ram_ini, dst_ram_ini) != FR_OK)
			goto out;
	}
	else if (dst_exists) {
		if (f_rename(dst_ram_ini, src_ram_ini) != FR_OK)
			goto out;

		if (f_rename(src_ram_ini, dst_ram_ini) != FR_OK)
			goto out;
	}
	else {
		g_restore_step = "RAM ini 파일 없음!";
		goto out;
	}

	ok = true;

out:
	sd_unmount();
	return ok;
}
// Moon launcher boot Horizon OS.
static void _launch_hos(u8 autoboot, u8 autoboot_list)
{
	b_cfg->boot_cfg = BOOT_CFG_FROM_LAUNCH | BOOT_CFG_AUTOBOOT_EN;
	b_cfg->autoboot = autoboot;
	b_cfg->autoboot_list = autoboot_list;

	void (*main_ptr)() = (void *)nyx_str->hekate;

	sd_end();

	hw_deinit(false);

	(*main_ptr)();
}
// Erista AutoRCM ON setting > Direct stock boot.
static void _launch_autorcm_hos(u8 autoboot, u8 autoboot_list)
{
	h_cfg.bootwait = 0;
	b_cfg->boot_cfg = BOOT_CFG_FROM_LAUNCH | BOOT_CFG_AUTOBOOT_EN;
	b_cfg->autoboot = autoboot;
	b_cfg->autoboot_list = autoboot_list;

	void (*main_ptr)() = (void *)nyx_str->hekate;

	sd_end();
	hw_deinit(false);
	(*main_ptr)();
}
// Button action on DRAM mismatch.
static lv_res_t _mbox_ofw_dram_action(lv_obj_t *btns, const char *txt)
{
	u32 idx = lv_btnm_get_pressed(btns);

	switch (idx)
	{
	case 0: // Restore to 4GB payload
		if (_restore_ram_mode(RAM_MODE_4GB)) {
			power_set_state(POWER_OFF_REBOOT);
		} else {
			_create_mbox_ofw_warning();
		}
		break;

	case 1: // Force Boot
		g_ofw_dram_confirmed = true;
		nyx_mbox_action(btns, txt);
		g_ofw_dram_warning = false;
		_do_ofw_boot();
		return LV_RES_OK;
	}

	return LV_RES_OK;
}
// Button action restore invalid RAM.
static lv_res_t _mbox_cfw_dram_action(lv_obj_t *btns, const char *txt)
{
	(void)txt;

	if (lv_btnm_get_pressed(btns) == 0) {
		if (_restore_ram_mode(RAM_MODE_4GB)) {
			power_set_state(POWER_OFF_REBOOT);
		}
	}
	return LV_RES_OK;
}
// Button action restore failed.
static lv_res_t _mbox_restore_failed_action(lv_obj_t *btns, const char *txt)
{
	(void)btns;
	(void)txt;

	power_set_state(POWER_OFF_REBOOT);
	return LV_RES_OK;
}
// OFW boot warning dialog.
static lv_res_t _create_mbox_ofw_warning(void)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);

	static const char *btn_fuse7[]   = { "\221확인", "" };
	static const char *btn_dram[]    = { "\221복원", "\221확인", "" };
	static const char *btn_restore[] = { "\221재부팅", "" };

	const char *text = NULL;
	const char **btn_map = NULL;
	lv_btnm_action_t action = NULL;

	char dbg[256];

	if (g_ofw_fuse7_warning) {
		btn_map = btn_fuse7;
		action  = nyx_mbox_action;

		text = "#FF0012 경고#\n\n"
			   "#FFBA00 안내#: #FF8000 지원되지 않는 DRAM Fuse 구성입니다!#\n\n"
			   "#C7EA46 실제 기기#: Erista + 8GB RAM\n"
			   "#C7EA46 인식 상태#: Mariko + 8GB RAM\n\n"
			   "펌웨어가 지원할 수 없는 메모리가 설치되어있습니다.\n\n"
			   "#FF0012 DRAM 트레이닝 단계에서 정상 부팅에 실패하며,#\n"
			   "#FF0012 무한 리부트, 블랙스크린 등이 발생할 수 있습니다.#\n\n"
			   "#C7EA46 에뮤낸드로만 사용하는 것을 권장합니다.#";
	} else if (g_ofw_dram_warning) {
		btn_map = btn_dram;
		action  = _mbox_ofw_dram_action;

		text = "#FF8000 주의#\n\n"
			   "#FFBA00 안내#: #FF8000 DRAM의 Fuse 정보가 실제와 일치하지 않습니다!#\n"
			   "#FF8000 혹은 8GB RAM 모드가 활성 상태입니다.#\n\n"
			   "#008EED 힌트: 메모리 교체 기기가 아닌 경우, 복원하세요.#\n\n"
			   "트레이닝이 제한적이며 실제 DRAM의 타이밍과\n"
			   "일치하지 않기 때문에 성능 저하가 발생합니다.\n\n"
			   "#C7EA46 무시하고 부팅하시겠습니까?#";
	} else {
		btn_map = btn_restore;
		action  = _mbox_restore_failed_action;

		s_printf(dbg,
			"#FF0012 오류#\n\n"
			"#FFDD00 4GB RAM 모드 복원에 실패했습니다!#\n\n"
			"#00DDFF 상세#: %s (%d)",
			g_restore_step ? g_restore_step : "unknown",
			g_restore_fr
		);
		text = dbg;
	}

	lv_mbox_set_text(mbox, text);
	lv_mbox_add_btns(mbox, btn_map, action);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	g_ofw_fuse7_warning = false;
	g_ofw_dram_warning  = false;

	return LV_RES_OK;
}
// CFW boot warning dialog.
static void _create_mbox_cfw_warning(void)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);

	static const char *btn_restore[] = { "\2214GB RAM 모드로 복원", "" };

	const char *text =
		"#FF0012 경고#\n\n"
		"#FFBA00 안내#: #FF8000 지원되지 않는 메모리 모드입니다!#\n\n"
		"#00DDFF RAM 상태#: #C7EA46 설정#-8GB / #C7EA46 실제#-4GB\n\n"
		"이 기기에서 해당 모드는 정상 작동하지 않습니다.\n"
		"반드시 실제 RAM과 일치하는 모드를 사용하세요.\n\n"
		"#008EED Ⓓ를 부팅하려면 복원이 필요합니다.#";

	lv_mbox_set_text(mbox, text);
	lv_mbox_add_btns(mbox, btn_restore, _mbox_cfw_dram_action);
	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);
}
// Direct boot CFW
static lv_res_t _reboot_cfw_action(lv_obj_t *btn)
{
	(void)btn;

	if (is_current_ram_mode() && !is_8gb_case()) {
		_create_mbox_cfw_warning();
		return LV_RES_OK;
	}

	return launch_fusee(btn);
}
// Direct boot OFW.
static void _do_ofw_boot(void)
{
	// Case: SysNAND Stock (Moon launcher)
	if (g_ofw_stock_launch) {
		int idx = g_pending_launch_idx;

		g_pending_launch_idx = -1;
		g_ofw_stock_launch = false;

		_launch_hos(idx, 1);
		return;
	}

	// Case: Normal OFW
	if (get_set_autorcm_status(false))
		_launch_autorcm_hos(2, 1);
	else
		power_set_state(REBOOT_BYPASS_FUSES);
}
// OFW boot action.
static lv_res_t _reboot_ofw_action(lv_obj_t *btn)
{
	// Case. Fuse 7 burned (Erista 8GB DRAM fuse)
	if (!h_cfg.t210b01 && fuse_read_dramid(true) == LPDDR4_ICOSA_8GB_SAMSUNG_K4FBE3D4HM_MGXX) {
		g_ofw_fuse7_warning = true;
		_create_mbox_ofw_warning();
		return LV_RES_OK;
	}
	// Case. Actual 8GB DRAM
	if (h_cfg.t210b01 && is_8gb_case()) {
		_do_ofw_boot();
		return LV_RES_OK;
	}
	// Case. T210B01 DRAM mismatch
	if (h_cfg.t210b01 && fuse_read_dramid(true) != fuse_read_dramid(false)) {
		if (!g_ofw_dram_confirmed) {
			g_ofw_dram_warning = true;
			_create_mbox_ofw_warning();
			return LV_RES_OK;
		}
		g_ofw_dram_confirmed = false;
		_do_ofw_boot();
		return LV_RES_OK;
	}
	// Case. Normal boot OFW
	_do_ofw_boot();
	return LV_RES_OK;
}
// Direct RCM on erista.
static lv_res_t _reboot_rcm_action(lv_obj_t *btn)
{
	power_set_state(REBOOT_RCM);
	return LV_RES_OK;
}
// Direct Poweroff.
static lv_res_t _poweroff_action(lv_obj_t *obj)
{
	if (h_cfg.rcm_patched)
		power_set_state(POWER_OFF);
	else
		power_set_state(POWER_OFF_RESET);

	return LV_RES_OK;
}
// Moon launcher.
static lv_res_t _launch_action(lv_obj_t *btn)
{
	// A completed long-press already opened the OC editor; swallow this click.
	launch_oc_press_pending = false;
	if (launch_oc_press_fired)
	{
		launch_oc_press_fired = false;
		return LV_RES_OK;
	}

	lv_btn_ext_t *ext = lv_obj_get_ext_attr(btn);
	u32 idx = ext->idx - 1;

	if (idx < MAX_HOME_ENTRIES) {
		/* SysNAND Stock */
		if (entries[idx].is_stock) {
			g_pending_launch_idx = ext->idx;
			g_ofw_stock_launch = true;
			g_ofw_dram_confirmed = false;
			return _reboot_ofw_action(btn);
		}

		/* CFW entry (Moon launcher) */
		if (entries[idx].is_cfw) {
			if (is_current_ram_mode() && !is_8gb_case()) {
				_create_mbox_cfw_warning();
				return LV_RES_OK;
			}
		}
	}

	_launch_hos(ext->idx, 1);
	return LV_RES_OK;
}

// hocate l4t edit.
static lv_res_t _launch_oc_press_action(lv_obj_t *btn)
{
	lv_btn_ext_t *ext = lv_obj_get_ext_attr(btn);
	u32 idx = ext->idx - 1;

	launch_oc_press_ms      = get_tmr_ms();
	launch_oc_press_name    = (char *)lv_obj_get_free_ptr(btn);
	launch_oc_press_label   = launch_ctxt.ddlabel[idx];
	launch_oc_press_indev   = lv_indev_get_act();
	launch_oc_press_pending = true;
	launch_oc_press_fired   = false;

	return LV_RES_OK;
}
static void _launch_oc_longpress_task(void *unused)
{
	if (!launch_oc_press_pending)
		return;

	// Cancel if the touch turned into a scroll/drag.
	if (launch_oc_press_indev && lv_indev_is_dragging(launch_oc_press_indev))
	{
		launch_oc_press_pending = false;
		return;
	}

	if ((get_tmr_ms() - launch_oc_press_ms) < 400)
		return;

	launch_oc_press_pending = false;
	launch_oc_press_fired   = true; // Consume the upcoming click so it doesn't launch.

	if (launch_oc_press_name)
		create_window_l4t_oc_editor(launch_oc_press_name, launch_oc_press_label);
}

//=============================
//  ASAP: Info Button action.
//=============================
lv_res_t _info_button_action(lv_obj_t *btn)
{
	FILINFO fno;
	bool is_8gb = false;

	if (sd_mount())
		return LV_RES_OK;

	if (f_chdrive("sd:") == FR_OK) {
		is_8gb = (f_stat("config/ultrahand/ram_8gb.ini", &fno) == FR_OK);
	}

	sd_unmount();

	if (_restore_ram_mode(is_8gb ? RAM_MODE_4GB : RAM_MODE_8GB)) {
		power_set_state(POWER_OFF_REBOOT);
	}

	return LV_RES_OK;
}

//===================
//  ASAP: PIN LOCK.
//===================

// Config lock pin actions.
static lv_res_t _unlock_action(lv_obj_t *btns, const char *txt)
{
	u32 btnidx = lv_btnm_get_pressed(btns);

	switch (btnidx)
	{
	case 0:
		// verify pinlock
		const char *passwd = lv_ta_get_text(pw_area);

		if (strcmp(passwd, n_cfg.pinlock) != 0)
		{
			// clear pinlock
			lv_ta_set_text(pw_area, "");
			return LV_RES_INV;
		}

		lv_ta_set_text(pw_area, "");
		if (pin_action) {
			pin_unlocked = true;
			pin_action(pin_param);
		}
		break;
	case 1:
		//power_set_state(POWER_OFF_RESET);
		break;
	}

	return nyx_mbox_action(btns, txt);
}

// PIN number del, refresh config.
static lv_res_t _unlock_btnm_action(lv_obj_t *btnm, const char *txt)
{
	if (!txt) return LV_RES_OK;
	if (strcmp(txt, SYMBOL_REBOOT) == 0) {
		lv_ta_set_text(pw_area, "");
		return LV_RES_OK;
	}
	if (strcmp(txt, "Ｄ") == 0) {
		lv_ta_del_char(pw_area);
		return LV_RES_OK;
	}
	char c = txt[0];
	if (c < '0' || c > '9') return LV_RES_OK;
	lv_ta_set_cursor_pos(pw_area, LV_TA_CURSOR_LAST);
	lv_ta_add_text(pw_area, txt);
	return LV_RES_OK;
}

// PIN number keypad, buttons.
static lv_res_t _create_mbox_unlock(void)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char * mbox_btn_map[] = { "\221해제", "\221취소", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 2);

	lv_mbox_set_text(mbox, "PIN 번호 입력");

	pw_area = lv_ta_create(mbox, NULL);
	lv_ta_set_one_line(pw_area, true);
	lv_ta_set_pwd_mode(pw_area, true);
	// makes ta_add_... not work
	// lv_ta_set_accepted_chars(pw_area, "0123456789");
	lv_ta_set_accepted_chars(pw_area, NULL);
	lv_ta_set_cursor_type(pw_area, LV_CURSOR_BLOCK | LV_CURSOR_HIDDEN);
	lv_ta_set_max_length(pw_area, 8);
	// lv_ta_set_max_length(pw_area, 64);
	lv_obj_set_width(pw_area, LV_HOR_RES / 5);
	lv_ta_set_text(pw_area, "");

	static const char * mbox_btnm_map[] = {
		"1", "2", "3", "\n",
		"4", "5", "6", "\n",
		"7", "8", "9", "\n",
		SYMBOL_REBOOT, "0", "Ｄ", "" };
	lv_obj_t *btnm1 = lv_btnm_create(mbox, NULL);
	lv_btnm_set_map(btnm1, mbox_btnm_map);
	lv_btnm_set_action(btnm1, _unlock_btnm_action);
	lv_obj_set_size(btnm1, LV_HOR_RES / 3, LV_VER_RES / 4);

	lv_mbox_add_btns(mbox, mbox_btn_map, _unlock_action);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}

// PIN lock action.
static lv_res_t _pin_protected_action(lv_obj_t *btn, lv_res_t (*action)(lv_obj_t *), lv_obj_t *param)
{
	(void)btn;
	if (n_cfg.pinlock[0] == '\0' || pin_unlocked) {
		action(param);
	} else {
		pin_action = action;
		pin_param  = param;
		_create_mbox_unlock();
	}
	return LV_RES_OK;
}

#define DECL_PIN_ACTION(NAME, FUNC) static lv_res_t NAME(lv_obj_t *btn) { return _pin_protected_action(btn, FUNC, NULL); }

DECL_PIN_ACTION(_btn_locktlas_action, launch_atlas)
DECL_PIN_ACTION(_btn_nandmng_action, create_win_emummc_tools)
DECL_PIN_ACTION(_btn_advence_action, _create_tab_options_advanced)
DECL_PIN_ACTION(_btn_toggle_emu_action, _toggle_mmc_action)
DECL_PIN_ACTION(_btn_action_ums_sd, action_ums_sd)
DECL_PIN_ACTION(_btn_action_hid_jc, _action_hid_jc)
DECL_PIN_ACTION(_btn_rcm_action, _reboot_rcm_action)
DECL_PIN_ACTION(_btn_filebrowser_action, create_file_browser)
//==================================================================================

void nyx_create_onoff_button(lv_theme_t *th, lv_obj_t *parent, lv_obj_t *btn, const char *btn_name, lv_action_t action, bool transparent)
{
	// Create buttons that are flat and text, plus On/Off switch.
	static lv_style_t btn_onoff_rel_hos_style, btn_onoff_pr_hos_style;
	lv_style_copy(&btn_onoff_rel_hos_style, th->btn.rel);
	btn_onoff_rel_hos_style.body.shadow.width = 0;
	btn_onoff_rel_hos_style.body.border.width = 0;
	btn_onoff_rel_hos_style.body.padding.hor = 0;
	btn_onoff_rel_hos_style.body.radius = 0;
	btn_onoff_rel_hos_style.body.empty = 1;

	lv_style_copy(&btn_onoff_pr_hos_style, &btn_onoff_rel_hos_style);
	if (transparent)
	{
		btn_onoff_pr_hos_style.body.main_color = LV_COLOR_HEX(0xFFFFFF);
		btn_onoff_pr_hos_style.body.opa = 35;
	}
	else
		btn_onoff_pr_hos_style.body.main_color = LV_COLOR_HEX(theme_bg_color ? (theme_bg_color + 0x101010) : 0x2D2D2D); // COLOR_HOS_BG_LIGHT.
	btn_onoff_pr_hos_style.body.grad_color = btn_onoff_pr_hos_style.body.main_color;
	btn_onoff_pr_hos_style.text.color = th->btn.pr->text.color;
	btn_onoff_pr_hos_style.body.empty = 0;

	lv_obj_t *label_btn = lv_label_create(btn, NULL);
	lv_obj_t *label_btnsw = NULL;

	lv_label_set_recolor(label_btn, true);
	label_btnsw = lv_label_create(btn, NULL);
	lv_label_set_recolor(label_btnsw, true);
	lv_btn_set_layout(btn, LV_LAYOUT_OFF);

	lv_btn_set_style(btn, LV_BTN_STYLE_REL, &btn_onoff_rel_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_PR, &btn_onoff_pr_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_TGL_REL, &btn_onoff_rel_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_TGL_PR, &btn_onoff_pr_hos_style);

	lv_btn_set_fit(btn, false, true);
	lv_obj_set_width(btn, lv_obj_get_width(parent));
	lv_btn_set_toggle(btn, true);

	lv_label_set_text(label_btn, btn_name);

	lv_label_set_text(label_btnsw, "#D0D0D0 OFF#");
	lv_obj_align(label_btn, btn, LV_ALIGN_IN_LEFT_MID, LV_DPI / 4, 0);
	lv_obj_align(label_btnsw, btn, LV_ALIGN_IN_RIGHT_MID, -LV_DPI / 4, -LV_DPI / 10);

	if (action)
		lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, action);
}

void _create_text_button(lv_theme_t *th, lv_obj_t *parent, lv_obj_t *btn, const char *btn_name, lv_action_t action)
{
	// Create buttons that are flat and only have a text label.
	static lv_style_t btn_onoff_rel_hos_style, btn_onoff_pr_hos_style;
	lv_style_copy(&btn_onoff_rel_hos_style, th->btn.rel);
	btn_onoff_rel_hos_style.body.shadow.width = 0;
	btn_onoff_rel_hos_style.body.border.width = 0;
	btn_onoff_rel_hos_style.body.radius = 0;
	btn_onoff_rel_hos_style.body.padding.hor = LV_DPI / 4;
	btn_onoff_rel_hos_style.body.empty = 1;

	lv_style_copy(&btn_onoff_pr_hos_style, &btn_onoff_rel_hos_style);
	if (hekate_bg)
	{
		btn_onoff_pr_hos_style.body.main_color = LV_COLOR_HEX(0xFFFFFF);
		btn_onoff_pr_hos_style.body.opa = 35;
	}
	else
		btn_onoff_pr_hos_style.body.main_color = LV_COLOR_HEX(theme_bg_color ? (theme_bg_color + 0x101010) : 0x2D2D2D); // COLOR_HOS_BG_LIGHT
	btn_onoff_pr_hos_style.body.grad_color = btn_onoff_pr_hos_style.body.main_color;
	btn_onoff_pr_hos_style.text.color = th->btn.pr->text.color;
	btn_onoff_pr_hos_style.body.empty = 0;

	lv_obj_t *label_btn = lv_label_create(btn, NULL);

	lv_label_set_recolor(label_btn, true);

	lv_btn_set_style(btn, LV_BTN_STYLE_REL, &btn_onoff_rel_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_PR, &btn_onoff_pr_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_TGL_REL, &btn_onoff_rel_hos_style);
	lv_btn_set_style(btn, LV_BTN_STYLE_TGL_PR, &btn_onoff_pr_hos_style);

	lv_btn_set_fit(btn, true, true);

	lv_label_set_text(label_btn, btn_name);

	if (action)
		lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, action);
}

static void _create_tab_about(lv_theme_t * th, lv_obj_t * parent)
{
	lv_obj_t *lbl_credits = lv_label_create(parent, NULL);

	lv_obj_align(lbl_credits, NULL, LV_ALIGN_IN_TOP_LEFT, LV_DPI / 2, LV_DPI / 2);
	lv_label_set_style(lbl_credits, &monospace_text);
	lv_label_set_recolor(lbl_credits, true);
	lv_label_set_static_text(lbl_credits,
		"\n#C7EA46 Hekate#       (c) 2018, #C7EA46 naehrwert#, #C7EA46 st4rk#\n"
		"#C7EA46 Hekate# & #C7EA46 Nyx# (c) 2018-2026, #FF0012 CTC##FFFFFF aer#\n"
		"#C7EA46 Atmosphère#   (c) 2018-2026, #FFFFFF Atmosphère-NX#\n"
		"#00FFCC ASAP# & #00FFCC ATLAS# (c) 2020-2024, #00FFCC Asa#\n\n"
		"Thanks to: #00CCFF derrek#, #00E4FF nedwill#, #00CCFF plutoo#,\n"
		"           #00E4FF shuffle2#, #00CCFF smea#, #00E4FF thexyz#, #00CCFF yellows8#\n\n"
		"Greetings to: #FFFFFF fincs#, #FFFFFF hexkyz#, #FFFFFF SciresM#,\n"
		"              #FFFFFF Shiny Quagsire#, #FFFFFF WinterMute#\n\n"
		"Open source and free packages used:                                              \n" // Label width alignment padding.
		" - Littlev Graphics Library,\n"
		"   Copyright (c) 2016-2018, #FFFFFF Gabor Kiss-Vamosi#\n\n"
		" - FatFs R0.13c,\n"
		"   Copyright (c) 2006-2018, #FFFFFF ChaN#\n"
		"   Copyright (c) 2018-2022, #FF0012 CTC##FFFFFF aer#\n\n"
		" - bcl-1.2.0,\n"
		"   Copyright (c) 2003-2006, #FFFFFF Marcus Geelnard#\n\n"
		" - blz,\n"
		"   Copyright (c) 2018, #FFFFFF SciresM#\n\n"
		" - elfload,\n"
		"   Copyright (c) 2014, #FFFFFF Owen Shepherd#\n"
		"   Copyright (c) 2018, #FFFFFF M4xw#"
	);

	lv_obj_t *asap_credits = lv_label_create(parent, NULL);
	lv_obj_align(asap_credits, lbl_credits, LV_ALIGN_IN_TOP_RIGHT, -LV_DPI / 8, 0);
	lv_label_set_style(asap_credits, &monospace_text);
	lv_label_set_recolor(asap_credits, true);
	lv_label_set_static_text(asap_credits,
		"\n#00FFCC ASAP# - #00FFCC A##FFFFFF sa's# #00FFCC S##FFFFFF witch# #00FFCC A##FFFFFF ll-in-one# #00FFCC P##FFFFFF ackage#\n"
		"- #C7EA46 Developer#: 2020-2024, #00FFCC Asa#\n"
		"             2025-2026, #00FFCC Yorunokyujitsu#\n\n"
		"Contents\n"
		" #F3F3F3 Hekate#, #CBCBCB Atmosphère#, #F3F3F3 ATLAS#, #CBCBCB ams-patch# \n"
		" #CBCBCB Ultrahand#, #F3F3F3 ovlloader#, #CBCBCB ovl-sysmodules# \n"
		" #F3F3F3 ASAP-Packages#, #CBCBCB hoc-clk#, #F3F3F3 ReverseNX-RT# \n"
		" #CBCBCB SaltyNX#, #F3F3F3 MissionControl#, #CBCBCB ovlreloader# \n"
		" #F3F3F3 sys-con#, #CBCBCB NX-FanControl#, #F3F3F3 ASAP-Updater# \n"
		" #CBCBCB Status-Monitor#, #F3F3F3 EdiZon#, #CBCBCB emuiibo#, #F3F3F3 DBI# \n"
		" #F3F3F3 Sphaira#, #CBCBCB Linkalho#, #F3F3F3 Daybreak#, #CBCBCB Tinfoil# \n"
		" #CBCBCB Reboot_to_payload#, #F3F3F3 Benchmark-Toolbox# \n"
		" #F3F3F3 FPSLocker#, #CBCBCB AmiiboGenerator#\n\n"
		"Credits\n"
		" #00CCFF switchbrew#, #00E4FF ITotalJustice#, #00CCFF proferabg# \n"
		" #00E4FF shchmue#, #00CCFF SuchMemeManySkill#, #00E4FF rdmrocha# \n"
		" #00CCFF borntohonk#, #00E4FF ndeadly#, #00CCFF duckbill#, #00E4FF halop# \n"
		" #00E4FF HamletDuFromage#, #00CCFF ppkantorski#, #00E4FF blawar# \n"
		" #00CCFF masagrator#, #00E4FF yusufakg#, #00CCFF o0Zz#, #00E4FF XorTroll# \n"
		" #00E4FF Hwfly-nx#, #00CCFF Morce3232#, #00E4FF impeeza#, #00CCFF rehius# \n"
		" #00CCFF NaGaa95#, #00E4FF sthetix#, #00CCFF Horizon-OC#"
	);

	lv_obj_t *asap_info = lv_label_create(parent, NULL);
	lv_obj_align(asap_info, NULL, LV_ALIGN_OUT_BOTTOM_LEFT, LV_DPI * 2.8, LV_DPI / 4);
	lv_label_set_style(asap_info, &hint_small_style_white);
	lv_obj_set_opa_scale_enable(asap_info, true);
	lv_obj_set_opa_scale(asap_info, LV_OPA_30);
	lv_label_set_static_text(asap_info, "Ｌ은 Asa의 프로젝트에서 포크되었으며, 개발자 본인의 사용만을 목적으로합니다.");

	lv_obj_t *hekate_img = lv_img_create(parent, NULL);
	lv_img_set_src(hekate_img, &hekate_logo);
	lv_obj_align(hekate_img, asap_credits, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, LV_DPI / 4);

	lv_obj_t *ctcaer_img = lv_img_create(parent, NULL);
	lv_img_set_src(ctcaer_img, &ctcaer_logo);
	lv_obj_align(ctcaer_img, asap_credits, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 4);
}

static void _update_status_bar(void *params)
{
	static char *label = NULL;

	u16 soc_temp = 0;
	u32 batt_percent = 0;
	int charge_status = 0;
	int batt_volt = 0;
	int batt_curr = 0;
	rtc_time_t time;

	// Get sensor data.
	max77620_rtc_get_time_adjusted(&time);
	soc_temp = tmp451_get_soc_temp(false);
	bq24193_get_property(BQ24193_ChargeStatus, &charge_status);
	max17050_get_property(MAX17050_RepSOC, (int *)&batt_percent);
	max17050_get_property(MAX17050_VCELL, &batt_volt);
	max17050_get_property(MAX17050_Current, &batt_curr);

	// Enable fan if more than 41 °C.
	u32 soc_temp_dec = soc_temp >> 8;
	fan_set_from_temp(soc_temp_dec);

	if (!label)
		label = (char *)malloc(512);

	//========================
	//  ASAP: AM,PM changer.
	//========================
	const char *ampm = "AM";
	int hour12 = time.hour;
	if (hour12 >= 12) {
		ampm = "PM";
		if (hour12 > 12) hour12 -= 12;
	} else if (hour12 == 0) {
		hour12 = 12;
	}
	s_printf(label, "%s%s", ampm, hour12 == 1 ? "　　" : "");
	lv_label_set_text(status_bar.ampm_label, label);
	//========================

	// Set time and SoC temperature.
	s_printf(label, "%d:%02d", hour12, time.min);
	lv_label_set_text(status_bar.time_label, label);

	//====================================================
	//  ASAP: Weekday calculation. (Zeller's Congruence)
	//====================================================
	int y = time.year;
	int m = time.month;
	int d = time.day;
	if (m < 3) {
		y--;
		m += 12;
	}
	int K = y % 100;
	int J = y / 100;
	int h = (d + (13*(m + 1))/5 + K + (K/4) + (J/4) + 5*J) % 7;
	int wday = (h + 6) % 7;
	static const char *weekday_str[7] = {"#D03838 일#", "월", "화", "수", "목", "금", "#3F70F9 토#"};

	s_printf(label, "%d/%d [%s]", time.month, time.day, weekday_str[wday]);
	lv_label_set_text(status_bar.cal_label, label);

	s_printf(label, SYMBOL_TEMPERATURE" %02d.%d℃", soc_temp_dec, (soc_temp & 0xFF) / 10);
	lv_label_set_text(status_bar.temp_label, label);

	// Realign labels
	lv_obj_realign(status_bar.ampm_label);
	lv_obj_realign(status_bar.time_label);
	lv_obj_realign(status_bar.cal_label);
	lv_obj_realign(status_bar.temp_label);
	//====================================================

	// Set battery percent and charging symbol.
	s_printf(label, charge_status ? "#00FFCC %d# #FFBA00 "SYMBOL_CHARGE"#" : "%d%%", (batt_percent >> 8) & 0xFF);
	lv_label_set_text(status_bar.battery, label);
	lv_obj_realign(status_bar.battery);

	// Set battery current draw and voltage.
	char curr_str[8];
	int curr_ma = batt_curr / 1000;

	if (curr_ma >= 0)
		s_printf(curr_str, "+%d", curr_ma);
	else
		s_printf(curr_str, "%d", curr_ma);

	s_printf(label, "#%s%5s", curr_ma >= 0 ? "47B100 " : "C02C1D ", curr_str);
	bool voltage_empty = batt_volt < 3200;
	s_printf(label + strlen(label), " mA#\n %s%d mV%s",
		voltage_empty ? "#FF8000 " : "", batt_volt,  voltage_empty ? " "SYMBOL_WARNING"#" : "");
	lv_label_set_text(status_bar.battery_more, label);
	lv_obj_realign(status_bar.battery_more);
}

// hocate fps, clock
static void _update_fps(void *params)
{
	static u32 fps_last_ms = 0;

	u32 now_ms  = get_tmr_ms();
	u32 elapsed = now_ms - fps_last_ms;

	if (fps_last_ms && elapsed)
	{
		char label[16];
		u32 fps = (_fps_frames * 1000 + elapsed / 2) / elapsed;
		if (fps < 10)
			s_printf(label, "#00FFCC FPS: %d #", fps);
		else
			s_printf(label, "#00FFCC FPS: %d#", fps);
		lv_label_set_text(status_bar.fps, label);
		lv_obj_realign(status_bar.fps);
	}

	fps_last_ms = now_ms;
	_fps_frames = 0;
}
static void _update_clocks(void *params)
{
	u32 bpmp_khz = clock_get_dev_freq(CLK_PTO_SCLK);
	u32 emc_khz  = clock_get_dev_freq(CLK_PTO_EMC);

	char label[48];
	s_printf(label, "%4d.%d MHz\n%4d.%d MHz",
		bpmp_khz / 1000, (bpmp_khz % 1000) / 100,
		emc_khz / 1000, (emc_khz % 1000) / 100);
	lv_label_set_text(status_bar.clocks, label);
	lv_obj_realign(status_bar.clocks);
}

static lv_res_t _create_mbox_payloads(lv_obj_t *btn)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char * mbox_btn_map[] = { "\251", "\222닫기", "\251", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES * 5 / 9);

	lv_mbox_set_text(mbox, "#008EED 페이로드 런처#\n\n#FFBA00 안내#: #FF8000 sdmc:/bootloader/payloads#\n경로에 보유한 페이로드가 표시됩니다.");

	// Create a list with all found payloads.
	//! TODO: SHould that be tabs with buttons? + Icon support?
	lv_obj_t *list = lv_list_create(mbox, NULL);
	payload_list = list;
	lv_obj_set_size(list, LV_HOR_RES * 3 / 7, LV_VER_RES * 3 / 7);
	lv_list_set_single_mode(list, true);

	if (sd_mount())
	{
		lv_mbox_set_text(mbox, "#FFBA00 SD 카드 초기화 실패!#");

		goto out_end;
	}

	dirlist_t *filelist = dirlist("bootloader/payloads", NULL, 0);
	sd_unmount();

	u32 i = 0;
	if (filelist)
	{
		while (true)
		{
			if (!filelist->name[i])
				break;
			lv_list_add(list, NULL, filelist->name[i], launch_payload);
			i++;
		}
		free(filelist);
	}

out_end:
	lv_mbox_add_btns(mbox, mbox_btn_map, nyx_mbox_action);

	lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_top(mbox, true);

	return LV_RES_OK;
}
DECL_PIN_ACTION(_btn_mbox_payloads, _create_mbox_payloads)

static lv_res_t _win_launch_close_action(lv_obj_t * btn)
{
	// Cleanup icons.
	for (u32 i = 0; i < 5; i++)
	{
		lv_obj_t *btns = launch_ctxt.btn[i];
		lv_btn_ext_t *ext = lv_obj_get_ext_attr(btns);
		if (ext->idx)
		{
			// This gets latest object, which is the button overlay. So iterate 2 times.
			lv_obj_t * img = lv_obj_get_child(btns, NULL);
			img = lv_obj_get_child(btns, img);

			lv_img_dsc_t *src = (lv_img_dsc_t *)lv_img_get_src(img);

			// Avoid freeing base icons.
			if ((src != icon_switch) && (src != icon_payload))
				free(src);
		}
	}

	lv_obj_t * win = lv_win_get_from_btn(btn);

	lv_obj_del(win);

	if (0 && !launch_bg_done && hekate_bg) // n_cfg.home_screen
	{
		lv_obj_set_opa_scale_enable(launch_bg, true);
		lv_obj_set_opa_scale(launch_bg, LV_OPA_TRANSP);
		//if (launch_bg)
		//	lv_obj_del(launch_bg); //! TODO: Find why it hangs.
		launch_bg_done = true;
	}

	close_btn = NULL;

	return LV_RES_INV;
}

static lv_obj_t *create_window_launch(const char *win_title)
{
	static lv_style_t win_bg_style, win_header;

	lv_style_copy(&win_bg_style, &lv_style_plain);
	win_bg_style.body.main_color = lv_theme_get_current()->bg->body.main_color;
	win_bg_style.body.grad_color = win_bg_style.body.main_color;

	if (0 && !launch_bg_done && hekate_bg) // n_cfg.home_screen
	{
		lv_obj_t *img = lv_img_create(lv_scr_act(), NULL);
		lv_img_set_src(img, hekate_bg);

		launch_bg = img;
	}

	lv_obj_t *win = lv_win_create(lv_scr_act(), NULL);
	lv_win_set_title(win, win_title);

	lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);

	if (0 && !launch_bg_done && hekate_bg) // n_cfg.home_screen
	{
		lv_style_copy(&win_header, lv_theme_get_current()->win.header);
		win_header.body.opa = LV_OPA_TRANSP;

		win_bg_style.body.opa = LV_OPA_TRANSP;
		lv_win_set_style(win, LV_WIN_STYLE_HEADER, &win_header);
	}

	lv_win_set_style(win, LV_WIN_STYLE_BG, &win_bg_style);

	close_btn = lv_win_add_btn(win, NULL, SYMBOL_CLOSE" 닫기", _win_launch_close_action);

	return win;
}

typedef struct _launch_button_pos_t
{
	u16 btn_x;
	u16 btn_y;
	u16 lbl_x;
	u16 lbl_y;
} launch_button_pos_t;

static const launch_button_pos_t launch_button_pos5[5] = {
	// First row.
	{ 19, 16,  0,  233},
	{260, 16, 241, 233},
	{501, 16, 482, 233},
	{742, 16, 723, 233},
	{983, 16, 964, 233}
};

//=====================================================
//  ASAP: Auto create to Fuse chainload, Stock entry.
//=====================================================
static bool _create_ini_if_missing(const char *path, const char *content)
{
	FIL f;
	if (f_stat(path, NULL) == FR_OK)
		return true;

	if (f_open(&f, path, FA_WRITE | FA_CREATE_NEW) != FR_OK)
		return false;

	UINT bw;
	f_write(&f, content, strlen(content), &bw);
	f_close(&f);
	return true;
}

//=================================
//  ASAP: Update version parsing.
//=================================
static const char *get_asap_current_version(void)
{
	static char ver[32] = {0};

	if (ver[0])
		return ver;

	LIST_INIT(ini_sections);

	if (!ini_parse(&ini_sections, "atmosphere/config/version.inc", false)) {
		LIST_FOREACH_ENTRY(ini_sec_t, sec, &ini_sections, link) {
			if (strcmp(sec->name, "ASAP") != 0)
				continue;

			LIST_FOREACH_ENTRY(ini_kv_t, kv, &sec->kvs, link) {
				if (!strcmp(kv->key, "current_version")) {
					strncpy(ver, kv->val, sizeof(ver) - 1);
					break;
				}
			}
			break;
		}
	}

	ini_free(&ini_sections);
	sd_unmount();

	return ver[0] ? ver : NULL;
}

//==========================================
//  ASAP: Moon Launcher (Profile Launcher)
//==========================================
static lv_res_t _create_window_home_launch(lv_obj_t *btn)
{
	const u32 max_entries = 5;
	const launch_button_pos_t *launch_button_pos = launch_button_pos5;

	// Arm the L4T long-press detector (single global task, created once).
	launch_oc_press_pending = false;
	launch_oc_press_fired   = false;
	if (!launch_oc_press_task)
		launch_oc_press_task = lv_task_create(_launch_oc_longpress_task, 30, LV_TASK_PRIO_MID, NULL);

	static lv_style_t btn_home_noborder_rel;
	lv_style_copy(&btn_home_noborder_rel, lv_theme_get_current()->btn.rel);
	btn_home_noborder_rel.body.opa = LV_OPA_TRANSP;
	btn_home_noborder_rel.body.border.width = 4;
	btn_home_noborder_rel.body.border.opa = LV_OPA_TRANSP;

	static lv_style_t btn_home_noborder_pr;
	lv_style_copy(&btn_home_noborder_pr, lv_theme_get_current()->btn.pr);
	btn_home_noborder_pr.body.opa = LV_OPA_TRANSP;
	btn_home_noborder_pr.body.border.width = 4;
	btn_home_noborder_pr.body.border.opa = LV_OPA_COVER;

	// Label container.
	static lv_style_t btn_label_home_transp;
	lv_style_copy(&btn_label_home_transp, lv_theme_get_current()->cont);
	btn_label_home_transp.body.opa = LV_OPA_TRANSP;

	lv_obj_t *win = create_window_launch(SYMBOL_HOME "  런처 · 부팅 설정");
	lv_cont_set_fit(lv_page_get_scrl(lv_win_get_content(win)), false, false);
	lv_page_set_scrl_height(lv_win_get_content(win), 640);

	sd_mount();
	LIST_INIT(ini_sections);
	emummc_cfg_t emu_info;
	load_emummc_cfg(&emu_info);

	// Buttons ini config, icon, label reading value.
	memset(entries, 0, sizeof(entries));
	const char *ddlabels[5] = { 0 };
	u32 e = 0;

	bool found_cfw   = false;
	bool found_stock = false;

	if (!ini_parse(&ini_sections, "bootloader/ini", true)) {
		LIST_FOREACH_ENTRY(ini_sec_t, ini_sec, &ini_sections, link) {
			if (e >= max_entries) break;
			if (!strcmp(ini_sec->name, "config") || ini_sec->type != INI_CHOICE) continue;

			const char *payload_val = NULL;
			const char *emummcforce_val = NULL;
			const char *emummc_disable_val = NULL;
			const char *stock_val = NULL;
			const char *id_val = NULL;

			LIST_FOREACH_ENTRY(ini_kv_t, kv, &ini_sec->kvs, link) {
				if      (!strcmp(kv->key, "payload"))              payload_val        = kv->val;
				else if (!strcmp(kv->key, "emummcforce"))          emummcforce_val    = kv->val;
				else if (!strcmp(kv->key, "emummc_force_disable")) emummc_disable_val = kv->val;
				else if (!strcmp(kv->key, "stock"))                stock_val          = kv->val;
				else if (!strcmp(kv->key, "id"))                   id_val             = kv->val;
			}

			if (payload_val && !strcmp(payload_val, "bootloader/payloads/ATLAS.bin"))
			{
				const char *ver = get_asap_current_version();

				static char label_buf[64];
				if (ver)
					s_printf(label_buf, "%s", ver);
				else
					strcpy(label_buf, "NOT-ASAP");

				entries[e] = (entry_t){ .icon = &asap_update, .label = label_buf, .is_stock = false, .is_cfw = false };
				ddlabels[e] = "업데이트";
				e++;
				continue;
			}
			if ((payload_val && !strcmp(payload_val, "bootloader/payloads/fusee.bin")) ||
				(emummcforce_val && atoi(emummcforce_val) == 1) ||
				(emummc_disable_val && atoi(emummc_disable_val) == 1 && !stock_val))
			{
				entries[e] = (entry_t){ .icon = emu_info.enabled ? &fusee_entry : &cfw_entry, .label = emu_info.enabled ? "에뮤낸드 커펌" : "시스낸드 커펌", .is_stock = false, .is_cfw = true };
				ddlabels[e] = emu_info.enabled ? "에뮤낸드 커펌" : "시스낸드 커펌";
				found_cfw = true;
				e++;
				continue;
			}
			if (stock_val && atoi(stock_val) == 1) {
				if (found_stock)
					continue;
				entries[e] = (entry_t){ .icon = &ofw_entry, .label = "시스낸드 스톡", .is_stock = true, .is_cfw = false };
				ddlabels[e] = "시스낸드 스톡";
				found_stock = true;
				e++;
				continue;
			}
			if (id_val && !strcmp(id_val, "SWANDR")) {
				entries[e] = (entry_t){ .icon = &android_entry, .label = "안드로이드", .is_stock = false, .is_cfw = false };
				strncpy(entries[e].ini_name, ini_sec->name, sizeof(entries[e].ini_name) - 1);
				entries[e].ini_name[sizeof(entries[e].ini_name) - 1] = 0;
				ddlabels[e] = "Ⓐ";
				e++;
				continue;
			}
			if (id_val && !strcmp(id_val, "SWR-LAK")) {
				entries[e] = (entry_t){ .icon = &lakka_entry, .label = "에뮬레이터", .is_stock = false, .is_cfw = false };
				strncpy(entries[e].ini_name, ini_sec->name, sizeof(entries[e].ini_name) - 1);
				entries[e].ini_name[sizeof(entries[e].ini_name) - 1] = 0;
				ddlabels[e] = "Ⓛ";
				e++;
				continue;
			}
			if (id_val && (!strcmp(id_val, "SWR-UBU") || !strcmp(id_val, "SWR-JAM") || !strcmp(id_val, "SWR-NOB")))
			{
				entries[e] = (entry_t){ .icon = &ubuntu_entry, .label = "리눅스", .is_stock = false, .is_cfw = false };
				strncpy(entries[e].ini_name, ini_sec->name, sizeof(entries[e].ini_name) - 1);
				entries[e].ini_name[sizeof(entries[e].ini_name) - 1] = 0;
				ddlabels[e] = "Ⓤ";
				e++;
				continue;
			}
		}
	}

	ini_free(&ini_sections);

	if (!found_cfw || !found_stock)
	{
		if (!found_cfw && e < max_entries)
		{
			if (_create_ini_if_missing("bootloader/ini/ams_cfw.ini",
				"[CFW]\n"
				"payload=bootloader/payloads/fusee.bin\n"
				"logopath=bootloader/res/asap.bmp\n"))
			{
				entries[e] = (entry_t){
					.icon = emu_info.enabled ? &fusee_entry : &cfw_entry,
					.label = emu_info.enabled ? "에뮤낸드 커펌" : "시스낸드 커펌",
					.is_stock = false,
					.is_cfw = true
				};
				ddlabels[e] = emu_info.enabled ? "에뮤낸드 커펌" : "시스낸드 커펌";
				e++;
			}
		}

		if (!found_stock && e < max_entries)
		{
			if (_create_ini_if_missing("bootloader/ini/ams_wbfix.ini",
				"[Stock]\n"
				"pkg3=atmosphere/package3\n"
				"stock=1\n"
				"emummc_force_disable=1\n"
				"logopath=bootloader/res/asap.bmp\n"))
			{
				entries[e] = (entry_t){
					.icon = &ofw_entry,
					.label = "시스낸드 스톡",
					.is_stock = true,
					.is_cfw = false
				};
				ddlabels[e] = "시스낸드 스톡";
				e++;
			}
		}
	}

	// Create buttons, labels.
	for (u32 i = 0; i < max_entries; i++) {
		const lv_img_dsc_t *bmp = entries[i].icon ? entries[i].icon : &empty_entry;
		const char *text        = entries[i].label;
		u16 btn_w = bmp->header.w + 4;
		u16 btn_h = bmp->header.h + 4;

		// Buttons config.
		lv_obj_t *tb = lv_btn_create(win, NULL);
		launch_ctxt.btn[i] = tb;
		lv_obj_set_size(tb, btn_w, btn_h);
		lv_obj_set_pos(tb, launch_button_pos[i].btn_x, launch_button_pos[i].btn_y);
		lv_btn_set_style(tb, LV_BTN_STYLE_REL, &btn_home_noborder_rel);
		lv_btn_set_style(tb, LV_BTN_STYLE_PR,  &btn_home_noborder_pr);
		lv_btn_set_layout(tb, LV_LAYOUT_OFF);

		// Button icon.
		lv_obj_t *img = lv_img_create(tb, NULL);
		lv_img_set_src(img, bmp);
		lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);

		// Button event.
		lv_btn_ext_t *ext = lv_obj_get_ext_attr(tb);
		ext->idx = i + 1;

		// RAM OC editor long-press target.
		bool is_oc_entry =
			(bmp == &android_entry) ||
			(bmp == &lakka_entry) ||
			(bmp == &ubuntu_entry);

		// Normal boot action.
		if (entries[i].icon) {
			lv_obj_set_click(tb, true);
			lv_btn_set_action(tb, LV_BTN_ACTION_CLICK, _launch_action);
		} else {
			lv_obj_set_click(tb, false);
		}

		// Android / Emulator / Linux / Empty slot long-press.
		if (is_oc_entry)
		{
			lv_obj_set_click(tb, true);

			strncpy(launch_ctxt.name[i], entries[i].ini_name,
				sizeof(launch_ctxt.name[0]) - 1);
			launch_ctxt.name[i][sizeof(launch_ctxt.name[0]) - 1] = 0;

			launch_ctxt.ddlabel[i] = ddlabels[i];

			lv_obj_set_free_ptr(tb, launch_ctxt.name[i]);
			lv_btn_set_action(tb, LV_BTN_ACTION_PR, _launch_oc_press_action);
		}

		// Display label settings.
		if (text && *text) {
			lv_obj_t *lbl_bg = lv_cont_create(win, NULL);
			lv_obj_set_style(lbl_bg, &btn_label_home_transp);
			lv_cont_set_fit(lbl_bg, false, false);
			lv_cont_set_layout(lbl_bg, LV_LAYOUT_CENTER);
			lv_obj_set_size(lbl_bg, 235, 24);
			lv_obj_set_pos(lbl_bg, launch_button_pos[i].lbl_x, launch_button_pos[i].lbl_y);

			lv_obj_t *lbl = lv_label_create(lbl_bg, NULL);
			lv_obj_set_style(lbl, &hint_small_style);
			lv_label_set_text(lbl, text);
			launch_ctxt.label[i] = lbl;
		} else {
			launch_ctxt.label[i] = NULL;
		}
	}

	lv_obj_t *line_sep = lv_line_create(win, NULL);
	static const lv_point_t line_pp[] = {{0,0},{ LV_HOR_RES - (LV_DPI - (LV_DPI/4))*2,0}};
	lv_line_set_points(line_sep, line_pp, 2);
	lv_line_set_style(line_sep, lv_theme_get_current()->line.decor);
	lv_obj_align(line_sep, NULL, LV_ALIGN_CENTER, -30, 23);

	lv_obj_t *l4t_oc_notice = lv_label_create(win, NULL);
	lv_obj_align(l4t_oc_notice, line_sep, LV_ALIGN_OUT_TOP_LEFT, LV_DPI * 2, -1);
	lv_label_set_style(l4t_oc_notice, &hint_small_style_white);
	lv_obj_set_opa_scale_enable(l4t_oc_notice, true);
	lv_obj_set_opa_scale(l4t_oc_notice, LV_OPA_30);
	lv_label_set_static_text(l4t_oc_notice, "L4T RAM 오버클럭을 적용하려면 Ⓐ · Ⓛ · Ⓤ 아이콘을 1초 이상 입력 유지하세요.");

	// Create AutoRCM On/Off button.
	lv_obj_t *rcm_btn = lv_btn_create(win, NULL);
	lv_obj_t *rcm_label = lv_label_create(rcm_btn, NULL);
	lv_btn_set_fit(rcm_btn, true, true);
	lv_label_set_recolor(rcm_label, true);
	lv_label_set_text(rcm_label, SYMBOL_REFRESH" RCM 부팅 #008EED   ON #");
	lv_obj_align(rcm_btn, line_sep, LV_ALIGN_OUT_BOTTOM_LEFT, -10, 35);
	lv_btn_set_action(rcm_btn, LV_BTN_ACTION_CLICK, _create_mbox_autorcm_status);

	// Set default state for AutoRCM and lock it out if patched unit.
	if (get_set_autorcm_status(false))
		lv_btn_set_state(rcm_btn, LV_BTN_STATE_TGL_REL);
	else
		lv_btn_set_state(rcm_btn, LV_BTN_STATE_REL);
	nyx_generic_onoff_toggle(rcm_btn);

	if (h_cfg.rcm_patched)
	{
		lv_obj_set_click(rcm_btn, false);
		lv_btn_set_state(rcm_btn, LV_BTN_STATE_INA);
	}
	autorcm_btn = rcm_btn;

	char *txt_buf = (char *)malloc(SZ_4K);

	s_printf(txt_buf,
		"#FF8000 Erista 전용# - #EFEFEF %s# 입력 동작을 설정합니다.\n\n"
		"#00DDFF ON #: 지그 없이 #C7EA46 RCM#으로 부팅합니다.\n"
		"#00DDFF OFF#: #C7EA46 충전# 표시, #C7EA46 정펌#으로만 부팅합니다.\n"
		"#FF3C28 주의#: 방전 시, 충분한 충전 이후 작동합니다.", gui_pv_btn(GUI_PV_BTN_0)
	);
	lv_obj_t *rcm_txt = lv_label_create(win, NULL);
	lv_label_set_recolor(rcm_txt, true);
	lv_label_set_text(rcm_txt, txt_buf);

	lv_obj_set_style(rcm_txt, &hint_small_style);
	lv_obj_align(rcm_txt, rcm_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 3);

	// Create Auto Boot button.
	lv_obj_t *label_autoboot = lv_label_create(win, NULL);
	lv_label_set_static_text(label_autoboot, SYMBOL_GPS " 자동 부팅");
	lv_obj_set_style(label_autoboot, lv_theme_get_current()->label.prim);
	lv_obj_align(label_autoboot, rcm_btn, LV_ALIGN_OUT_RIGHT_MID, 78, 0);

	lv_obj_t *ddlist = lv_ddlist_create(win, NULL);
	lv_obj_set_top(ddlist, true);
	lv_ddlist_set_draw_arrow(ddlist, true);

	launch_ctxt.dd_map[0] = 0;
	launch_ctxt.dd_count  = 1;
	static char dd_options[256];
	s_printf(dd_options, "비활성화         ");

	for (uint32_t i = 0; i < max_entries; i++) {
		if (entries[i].icon) {
			launch_ctxt.dd_map[ launch_ctxt.dd_count ] = i + 1;
			const char *opt = ddlabels[i] ? ddlabels[i] : entries[i].label;
			s_printf(dd_options + strlen(dd_options), "\n%s", opt);
			launch_ctxt.dd_count++;
		}
	}
	lv_ddlist_set_options(ddlist, dd_options);

	// Init selected idx cal.
	uint8_t sel = 0;
	for (uint8_t j = 1; j < launch_ctxt.dd_count; j++) {
		if (launch_ctxt.dd_map[j] == h_cfg.autoboot) {
			sel = j;
			break;
		}
	}
	lv_ddlist_set_selected(ddlist, sel);
	lv_ddlist_set_action(ddlist, _autoboot_list_action);
	lv_obj_align(ddlist, label_autoboot, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 4, 0);

	lv_obj_t *atb_txt = lv_label_create(win, NULL);
	lv_label_set_recolor(atb_txt, true);
	s_printf(txt_buf,
		"재부팅시 자동으로 부팅할 OS를 선택합니다.\n\n"
		"#00DDFF 커스텀 펌웨어#: #C7EA46 기본 낸드# 커펌.\n"
		"#00DDFF 시스낸드 스톡#: #FF8000 커펌 모듈 OFF# 시스낸드.\n"
		"#00DDFF Ⓐ# · #00DDFF Ⓛ# · #00DDFF Ⓤ#: #C7EA46 L4T# 에뮤낸드.");
	lv_label_set_text(atb_txt, txt_buf);
	lv_obj_set_style(atb_txt, &hint_small_style);
	lv_obj_align(atb_txt, label_autoboot, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 2);

	// Create Boot time delay list.
	lv_obj_t *bt_dly = lv_label_create(win, NULL);
	lv_label_set_static_text(bt_dly, SYMBOL_CLOCK" 부팅 대기 시간");
	lv_obj_set_style(bt_dly, lv_theme_get_current()->label.prim);
	lv_obj_align(bt_dly, ddlist, LV_ALIGN_OUT_RIGHT_MID, 42, 0);

	lv_obj_t *ddlist2 = lv_ddlist_create(win, NULL);
	lv_obj_set_top(ddlist2, true);
	lv_ddlist_set_draw_arrow(ddlist2, true);
	lv_ddlist_set_options(ddlist2,
		"화면 스킵    \n"
		"1초\n"
		"2초\n"
		"3초\n"
		"4초\n"
		"5초");
	lv_ddlist_set_selected(ddlist2, 5);
	lv_obj_align(ddlist2, bt_dly, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 4, 0);
	lv_ddlist_set_action(ddlist2, _autoboot_delay_action);
	lv_ddlist_set_selected(ddlist2, h_cfg.bootwait);

	atb_txt = lv_label_create(win, NULL);
	lv_label_set_recolor(atb_txt, true);
	s_printf(txt_buf,
		"부팅 화면(=로딩바)의 표시 시간을 설정합니다.\n\n"
		"#00DDFF 화면 스킵#: 부팅 화면 없이 #C7EA46 즉시# 부팅합니다.\n"
		"#00DDFF 시간 설정#: %s 입력 시 #C7EA46 Ｈ#로 돌아갑니다.\n"
		"#FFBA00 안내#: 홈 화면의 #C7EA46 메인 런처#에는 적용되지 않습니다.",
		gui_pv_btn(GUI_PV_BTN_3)
	);
	lv_label_set_text(atb_txt, txt_buf);
	lv_obj_set_style(atb_txt, &hint_small_style);
	lv_obj_align(atb_txt, bt_dly, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 2);
	free(txt_buf);

	lv_obj_set_top(win, true); // Set the ddlist container at top.
	lv_obj_set_parent(ddlist, win); // Reorder ddlist.
	lv_obj_set_parent(ddlist2, win); // Reorder ddlist.
	lv_obj_set_top(ddlist, true);
	lv_obj_set_top(ddlist2, true);

	sd_unmount();

	return LV_RES_OK;
}
DECL_PIN_ACTION(_btn_moon_launch, _create_window_home_launch)

//=======================
//  ASAP: Font Selector
//=======================
static void init_font_style(lv_style_t *style, lv_style_t *base, const lv_font_t *font)
{
	lv_style_copy(style, base);
	style->text.font = font;
}

//============================================
//  ASAP: User profile (Moon) button scale.
//============================================
lv_img_dsc_t* scale_crop_center_365_h(const lv_img_dsc_t *src_img) {
	uint16_t src_w = src_img->header.w;
	uint16_t src_h = src_img->header.h;
	uint8_t  cf    = src_img->header.cf;
	const uint32_t *src_px = (const uint32_t*)src_img->data;

	float scale     = (float)PROFILE_SIZE / src_h;
	float inv_scale = 1.0f / scale;

	int scaled_w = (int)ceilf(src_w * scale);

	size_t pixel_cnt  = PROFILE_SIZE * PROFILE_SIZE;
	size_t data_size  = pixel_cnt * 4;
	size_t total_size = sizeof(lv_img_dsc_t) + data_size;
	uint8_t *mem = malloc(total_size);
	if(!mem) return NULL;

	lv_img_dsc_t *dst = (lv_img_dsc_t*)mem;
	dst->header.always_zero = 0;
	dst->header.w           = PROFILE_SIZE;
	dst->header.h           = PROFILE_SIZE;
	dst->header.cf          = cf;
	dst->data_size          = data_size;
	dst->data               = mem + sizeof(lv_img_dsc_t);

	memset((void *)dst->data, 0x00, data_size);

	int crop_x = (scaled_w - PROFILE_SIZE) / 2;

	uint32_t *dst_px = (uint32_t*)dst->data;
	for(int y = 0; y < PROFILE_SIZE; y++) {
		int sy = (int)(y * inv_scale);
		if(sy < 0) sy = 0;
		else if(sy >= src_h) sy = src_h - 1;

		for(int x = 0; x < PROFILE_SIZE; x++) {
			float src_xf = (x + crop_x) * inv_scale;
			int sx = (int)src_xf;
			if(sx >= 0 && sx < src_w) {
				dst_px[y * PROFILE_SIZE + x] = src_px[sy * src_w + sx];
			}
		}
	}

	return dst;
}

//============================
//  ASAP: Init user profile.
//============================
static lv_res_t tab_signal_cb(lv_obj_t *obj, lv_signal_t sig, void *param)
{
	if(sig == LV_SIGNAL_CLEANUP) {
		lv_obj_set_signal_func(obj, old_parent_signal);
		if(profile_img) {
			free((void*)profile_img->data);
			free(profile_img);
			profile_img = NULL;
		}
		if(ext_img) {
			free((void*)ext_img->data);
			free(ext_img);
			ext_img = NULL;
		}
	}
	
	return old_parent_signal(obj, sig, param);
}

//============================
//  ASAP: Init user profile.
//============================
static lv_res_t _btn_atlas_click_action(lv_obj_t *btn)
{
	FILINFO fno;

	if (sd_mount())
		return LV_RES_OK;

	if (f_stat("bootloader/payloads/ATLAS.bin", &fno) == FR_OK) {
		_btn_locktlas_action(btn);
	} else {
		_btn_mbox_payloads(btn);
	}

	sd_unmount();
	return LV_RES_OK;
}

//===============================
//  ASAP: Personal custom home.
//===============================
static void _create_tab_home(lv_theme_t *th, lv_obj_t *parent)
{
	old_parent_signal = parent->signal_func;
	lv_obj_set_signal_func(parent, tab_signal_cb);

	lv_page_set_scrl_layout(parent, LV_LAYOUT_OFF);
	lv_page_set_scrl_fit(parent, false, false);
	lv_page_set_scrl_height(parent, 592);

	char *txt_buf = (char *)malloc(SZ_16K);

	// Main buttons label font.
	static lv_style_t icons;
	init_font_style(&icons, th->label.prim, &hekate_symbol_120);
	icons.text.letter_space = 18;

	sd_mount();
	g_sd_is_exfat = (sd_fs.fs_type == FS_EXFAT);

	// ASAP: User profile, empty cartridge img.
	lv_obj_t *img_obj = lv_img_create(parent, NULL);
	const char *profile_path = NULL;
	if (!f_stat("bootloader/res/event_pf.bmp", NULL))
		profile_path = "bootloader/res/event_pf.bmp";
	else if (!f_stat("bootloader/res/profile.bmp", NULL))
		profile_path = "bootloader/res/profile.bmp";
	if (profile_path) {
		ext_img = bmp_to_lvimg_obj(profile_path);
	} else {
		ext_img = NULL;
	}
	if(ext_img && ext_img->header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA) {
		uint16_t w = ext_img->header.w, h = ext_img->header.h;
		float   cx = w / 2.0f, cy = h / 2.0f, radius = (w < h ? w : h) / 2.0f;
		uint8_t *pixels = (uint8_t *)ext_img->data;

		for(uint16_t y = 0; y < h; y++) {
			for(uint16_t x = 0; x < w; x++) {
				float dd = radius - sqrtf((x-cx)*(x-cx)+(y-cy)*(y-cy));
				uint32_t idx = (y * w + x) * 4;
				if(dd <= 0) pixels[idx+3] = 0;
				else if(dd < 1) pixels[idx+3] = (uint8_t)(pixels[idx+3] * dd);
			}
		}

		profile_img = scale_crop_center_365_h(ext_img);
		lv_obj_set_size(img_obj, PROFILE_SIZE, PROFILE_SIZE);
		lv_img_set_src(img_obj, profile_img);
		lv_obj_align(img_obj, NULL, LV_ALIGN_IN_TOP_RIGHT, -LV_DPI / 2.75, LV_DPI / 1.17);
	} else {
		lv_img_set_src(img_obj, &user_profile);
		lv_obj_align(img_obj, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, LV_DPI / 2);
	}

	// MOON Launcher button.
	lv_obj_t *btn_moon = lv_btn_create(parent, NULL);
	lv_obj_set_size(btn_moon, 367, 367);

	static lv_style_t style_moon;
	memcpy(&style_moon, &lv_style_plain, sizeof(lv_style_t));
	style_moon.body.radius = LV_RADIUS_CIRCLE;
	style_moon.body.opa = LV_OPA_TRANSP;
	lv_btn_set_style(btn_moon, LV_BTN_STYLE_REL, &style_moon);
	lv_btn_set_style(btn_moon, LV_BTN_STYLE_PR, &btn_moon_pr);
	lv_obj_align(btn_moon, img_obj, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t *label_moon = lv_label_create(btn_moon, NULL);
	lv_label_set_text(label_moon, "　");
	lv_btn_set_action(btn_moon, LV_BTN_ACTION_CLICK, _btn_moon_launch);
	
	// Time, calendar label
	lv_obj_t *btn_calendar = lv_btn_create(parent, NULL);
	lv_btn_set_style(btn_calendar, LV_BTN_STYLE_REL, &btn_custom_rel);
	lv_btn_set_style(btn_calendar, LV_BTN_STYLE_PR, &btn_custom_pr);
	lv_btn_set_action(btn_calendar, LV_BTN_ACTION_CLICK, _create_mbox_clock_edit);
	lv_obj_set_size(btn_calendar, 290, 130);
	lv_btn_set_layout(btn_calendar, LV_LAYOUT_OFF);
	lv_obj_align(btn_calendar, NULL, LV_ALIGN_IN_TOP_LEFT, 31, 35);

	lv_obj_t *lbl_ampm = lv_label_create(btn_calendar, NULL);
	lv_obj_set_style(lbl_ampm, &hint_small_style_white);
	lv_label_set_text(lbl_ampm, "AM");
	lv_obj_align(lbl_ampm, NULL, LV_ALIGN_IN_LEFT_MID, 16, -44);
	status_bar.ampm_label = lbl_ampm;

	lv_obj_t *lbl_time = lv_label_create(btn_calendar, NULL);
	lv_obj_set_style(lbl_time, &icons);
	lv_label_set_text(lbl_time, "00:00");
	lv_obj_align(lbl_time, lbl_ampm, LV_ALIGN_OUT_RIGHT_MID, 10, 39);
	status_bar.time_label = lbl_time;

	status_bar.cal_label = lv_label_create(btn_calendar, NULL);
	lv_label_set_recolor(status_bar.cal_label, true);
	lv_obj_set_style(status_bar.cal_label, &hint_small_style);
	lv_obj_align(status_bar.cal_label, status_bar.ampm_label, LV_ALIGN_IN_BOTTOM_LEFT, 0, 88);

	status_bar.temp_label = lv_label_create(btn_calendar, NULL);
	lv_obj_set_style(status_bar.temp_label, &hint_small_style);
	lv_obj_align(status_bar.temp_label, status_bar.time_label, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);

	// atmosphere launch button.
	emummc_cfg_t emu_info;
	load_emummc_cfg(&emu_info);

	lv_obj_t *btn_launch = lv_btn_create(parent, NULL);
	lv_btn_set_style(btn_launch, LV_BTN_STYLE_REL, &btn_custom_rel);
	lv_btn_set_style(btn_launch, LV_BTN_STYLE_PR, &btn_custom_pr);
	lv_obj_t *label_btn = lv_label_create(btn_launch, NULL);
	lv_label_set_recolor(label_btn, true);
	lv_obj_set_style(label_btn, &icons);
	s_printf(txt_buf, emu_info.enabled ? "#DBE8F1 B#" : "#4E4E67 B#");
	lv_label_set_text(label_btn, txt_buf);
	lv_btn_set_action(btn_launch, LV_BTN_ACTION_CLICK, _reboot_cfw_action);
	lv_obj_set_size(btn_launch, 170, 170);
	lv_btn_set_layout(btn_launch, LV_LAYOUT_OFF);
	lv_obj_align(label_btn, NULL, LV_ALIGN_CENTER, 0, -26);
	atmo_bg_obj = label_btn;
	lv_obj_t *label_btn2 = lv_label_create(btn_launch, NULL);
	lv_label_set_recolor(label_btn2, true);
	lv_obj_set_style(label_btn2, &icons);
	s_printf(txt_buf, emu_info.enabled ? "#6060C0 C#" : "#95B7E4 C#");
	lv_label_set_text(label_btn2, txt_buf);
	lv_obj_align(label_btn2, label_btn, LV_ALIGN_IN_BOTTOM_LEFT, emu_info.enabled ? -1 : 0, 0);
	atmo_sphere_obj = label_btn2;
	lv_obj_t *label_btn3 = lv_label_create(btn_launch, NULL);
	lv_label_set_recolor(label_btn3, true);
	lv_obj_set_style(label_btn3, &icons);
	s_printf(txt_buf, "#EFEFEF D#");
	lv_label_set_text(label_btn3, txt_buf);
	lv_obj_align(label_btn3, label_btn, LV_ALIGN_IN_BOTTOM_MID, 0, 41); // Nand info align: 0, 5

	// Original firmware button.
	lv_obj_t *btn_ofw = lv_btn_create(parent, btn_launch);
	label_btn = lv_label_create(btn_ofw, label_btn);
	s_printf(txt_buf, "#E60012 E#");
	lv_label_set_text(label_btn, txt_buf);
	lv_btn_set_action(btn_ofw, LV_BTN_ACTION_CLICK, _reboot_ofw_action);
	lv_btn_set_layout(btn_ofw, LV_LAYOUT_OFF);
	lv_obj_align(label_btn, NULL, LV_ALIGN_CENTER, 0, -26);
	label_btn2 = lv_label_create(btn_ofw, label_btn2);
	s_printf(txt_buf, "#EFEFEF F#");
	lv_label_set_text(label_btn2, txt_buf);
	lv_obj_align(label_btn2, label_btn, LV_ALIGN_IN_BOTTOM_MID, 0, 38);

	// ATLAS button.
	lv_obj_t *btn_atlas = lv_btn_create(parent, btn_launch);
	label_btn = lv_label_create(btn_atlas, label_btn);
	s_printf(txt_buf, "#888888 G#");
	lv_label_set_text(label_btn, txt_buf);
	lv_btn_set_action(btn_atlas, LV_BTN_ACTION_CLICK, _btn_atlas_click_action);
	lv_btn_set_action(btn_atlas, LV_BTN_ACTION_LONG_PR, _btn_mbox_payloads);
	lv_btn_set_layout(btn_atlas, LV_LAYOUT_OFF);
	lv_obj_align(label_btn, NULL, LV_ALIGN_CENTER, 0, -26);
	label_btn2 = lv_label_create(btn_atlas, label_btn2);
	s_printf(txt_buf, "#EFEFEF H#");
	lv_label_set_text(label_btn2, txt_buf);
	lv_obj_align(label_btn2, label_btn, LV_ALIGN_IN_BOTTOM_MID, 0, 42);

	// NAND manage button.
	lv_obj_t *btn_nandmng = lv_btn_create(parent, btn_launch);
	label_btn = lv_label_create(btn_nandmng, label_btn);
	s_printf(txt_buf, emu_info.enabled ? "#EFEFEF I#" : "#727F8E I#");
	lv_label_set_text(label_btn, txt_buf);
	lv_btn_set_action(btn_nandmng, LV_BTN_ACTION_CLICK, _btn_nandmng_action); //create_win_emummc_tools
	lv_btn_set_layout(btn_nandmng, LV_LAYOUT_OFF);
	lv_obj_align(label_btn, NULL, LV_ALIGN_CENTER, 0, -24);
	nandmng_label = label_btn;

	const char *emu_colors[] = { "#EBAF0C J#", "#B5D5E6 K#", "#EFEFEF L#", "#21322C M#", "#252084 N#", "#0F0C44 V#" };
	const char *sys_colors[] = { "#EBAF0C J#", "#014A88 K#", "#014A88 L#", "#21322C M#", "#EFEFEF O#", "#CACBCC V#" };
	const int nm_xoffsets[]  = { 16, 0, -25, 17, -7, -57 };
	const int emu_yoffsets[] = { 0, 0, 0, 0, 0, 0 };
	const int sys_yoffsets[] = { 0, -49, 0, 0, 0, 0 };
	
	for (int i = 0; i < 6; i++) {
		lv_obj_t *nm = lv_label_create(btn_nandmng, NULL);
		lv_label_set_recolor(nm, true);
		lv_obj_set_style(nm, &icons);
		s_printf(txt_buf, emu_info.enabled ? emu_colors[i] : sys_colors[i]);
		lv_label_set_text(nm, txt_buf);
		lv_obj_align(nm, label_btn, LV_ALIGN_CENTER, nm_xoffsets[i], emu_info.enabled ? emu_yoffsets[i] : sys_yoffsets[i]);
		nandmng_color_labels[i] = nm;
	}

	lv_obj_t *format_label = lv_label_create(btn_nandmng, NULL);
	lv_label_set_recolor(format_label, true);
	lv_obj_set_style(format_label, &icons);
	s_printf(txt_buf, "%s %s", emu_info.enabled ? "#EFEFEF" : "#727F8E", g_sd_is_exfat ? "Q#" : "P#");
	lv_label_set_text(format_label, txt_buf);
	lv_obj_align(format_label, label_btn, LV_ALIGN_CENTER, 22, 0);
	nandmng_format_label = format_label;

	lv_obj_t *ftype_label = lv_label_create(btn_nandmng, NULL);
	lv_label_set_recolor(ftype_label, true);
	lv_obj_set_style(ftype_label, &icons);
	s_printf(txt_buf, "%s %s", emu_info.enabled ? "#BBC3C0" : "#727F8E", emu_info.enabled ? (emu_info.sector ? "T#" : "U#") : (g_sd_is_exfat ? "R#" : "S#"));
	lv_label_set_text(ftype_label, txt_buf);
	lv_obj_align(ftype_label, format_label, LV_ALIGN_IN_BOTTOM_LEFT, 0, 1);
	nandmng_ftype_label = ftype_label;

	label_btn2 = lv_label_create(btn_nandmng, label_btn2);
	s_printf(txt_buf, "#EFEFEF W#");
	lv_label_set_text(label_btn2, txt_buf);
	lv_obj_align(label_btn2, label_btn, LV_ALIGN_IN_BOTTOM_MID, 0, 37);

	lv_obj_set_pos(btn_launch, 35, 400);
	lv_obj_set_pos(btn_ofw, 225, 400);
	lv_obj_set_pos(btn_atlas, 415, 400);
	lv_obj_set_pos(btn_nandmng, 605, 400);

	free(txt_buf);
	sd_unmount();
}

static void _nyx_set_default_styles(lv_theme_t * th)
{
	lv_style_copy(&mbox_darken, &lv_style_plain);
	mbox_darken.body.main_color = LV_COLOR_BLACK;
	mbox_darken.body.grad_color = mbox_darken.body.main_color;
	mbox_darken.body.opa = LV_OPA_30;

	lv_style_copy(&hint_small_style, th->label.hint);
	hint_small_style.text.letter_space = 1;
	hint_small_style.text.font = &interui_20;

	lv_style_copy(&hint_small_style_white, th->label.prim);
	hint_small_style_white.text.letter_space = 1;
	hint_small_style_white.text.font = &interui_20;

	lv_style_copy(&monospace_text, &lv_style_plain);
	monospace_text.body.main_color = COLOR_HOS_BG_DARKER;
	monospace_text.body.grad_color = COLOR_HOS_BG_DARKER;
	monospace_text.body.border.color = COLOR_HOS_BG_DARKER;
	monospace_text.body.border.width = 0;
	monospace_text.body.opa = LV_OPA_TRANSP;
	monospace_text.text.color = LV_COLOR_HEX(0xD8D8D8);
	monospace_text.text.font = &ubuntu_mono;
	monospace_text.text.letter_space = 0;
	monospace_text.text.line_space = 0;

	lv_style_copy(&btn_transp_rel, th->btn.rel);
	btn_transp_rel.body.main_color = LV_COLOR_HEX(0x444444);
	btn_transp_rel.body.grad_color = btn_transp_rel.body.main_color;
	btn_transp_rel.body.shadow.color = LV_COLOR_HEX(0x0F0F0F);
	btn_transp_rel.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_pr, th->btn.pr);
	btn_transp_pr.body.main_color = LV_COLOR_HEX(0x888888);
	btn_transp_pr.body.grad_color = btn_transp_pr.body.main_color;
	btn_transp_pr.body.shadow.color = LV_COLOR_HEX(0x0F0F0F);
	btn_transp_pr.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_tgl_rel, th->btn.tgl_rel);
	btn_transp_tgl_rel.body.main_color = LV_COLOR_HEX(0x444444);
	btn_transp_tgl_rel.body.grad_color = btn_transp_tgl_rel.body.main_color;
	btn_transp_tgl_rel.body.shadow.color = LV_COLOR_HEX(0x0F0F0F);
	btn_transp_tgl_rel.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_tgl_pr, th->btn.tgl_pr);
	btn_transp_tgl_pr.body.main_color = LV_COLOR_HEX(0x888888);
	btn_transp_tgl_pr.body.grad_color = btn_transp_tgl_pr.body.main_color;
	btn_transp_tgl_pr.body.shadow.color = LV_COLOR_HEX(0x0F0F0F);
	btn_transp_tgl_pr.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_ina, th->btn.ina);
	btn_transp_ina.body.main_color = LV_COLOR_HEX(0x292929);
	btn_transp_ina.body.grad_color = btn_transp_ina.body.main_color;
	btn_transp_ina.body.border.color = LV_COLOR_HEX(0x444444);
	btn_transp_ina.body.shadow.color = LV_COLOR_HEX(0x0F0F0F);
	btn_transp_ina.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_ina, th->btn.ina);
	btn_transp_ina.body.main_color = LV_COLOR_HEX(0x292929);
	btn_transp_ina.body.grad_color = btn_transp_ina.body.main_color;
	btn_transp_ina.body.border.color = LV_COLOR_HEX(0x444444);
	btn_transp_ina.body.shadow.color = LV_COLOR_HEX(0x0F0F0F);
	btn_transp_ina.body.opa = LV_OPA_50;

	lv_style_copy(&ddlist_transp_bg, th->ddlist.bg);
	ddlist_transp_bg.body.main_color = LV_COLOR_HEX(0x0E0E1A);
	ddlist_transp_bg.body.grad_color = ddlist_transp_bg.body.main_color;
	ddlist_transp_bg.body.opa = 255;

	lv_style_copy(&ddlist_transp_sel, th->ddlist.sel);
	ddlist_transp_sel.body.main_color = LV_COLOR_HEX(0x4D4D4D);
	ddlist_transp_sel.body.grad_color = ddlist_transp_sel.body.main_color;
	ddlist_transp_sel.body.opa = 180; // 70.6%.

	//=====================
	//  ASAP: New transp.
	//=====================
	lv_style_copy(&btn_custom_rel, th->btn.rel);
	btn_custom_rel.body.main_color = LV_COLOR_HEX(0x444444);
	btn_custom_rel.body.grad_color = btn_custom_rel.body.main_color;
	btn_custom_rel.body.opa = LV_OPA_TRANSP;

	lv_style_copy(&btn_custom_pr, th->btn.pr);
	btn_custom_pr.body.main_color = LV_COLOR_HEX(0x888888);
	btn_custom_pr.body.grad_color = btn_custom_pr.body.main_color;
	btn_custom_pr.body.opa = LV_OPA_30;

	lv_style_copy(&btn_custom_pr2, th->btn.pr);
	btn_custom_pr2.body.main_color = LV_COLOR_HEX(0x888888);
	btn_custom_pr2.body.grad_color = btn_custom_pr2.body.main_color;
	btn_custom_pr2.body.opa = LV_OPA_TRANSP;

	lv_style_copy(&btn_moon_pr, th->btn.pr);
	btn_moon_pr.body.radius = LV_RADIUS_CIRCLE;
	btn_moon_pr.body.main_color = LV_COLOR_HEX(0x888888);
	btn_moon_pr.body.grad_color = btn_moon_pr.body.main_color;
	btn_moon_pr.body.opa = LV_OPA_TRANSP;
	//=====================

	lv_color_t tmp_color = COLOR_HOS_TURQUOISE_EX(n_cfg.theme_color);
	text_color = malloc(32);
	s_printf(text_color, "#%06X", (u32)(tmp_color.full & 0xFFFFFF));
}

lv_task_t *task_bpmp_clock;
void first_time_bpmp_clock(void *param)
{
	// Remove task.
	lv_task_del(task_bpmp_clock);

	// Max clock seems fine. Save it.
	n_cfg.bpmp_clock = 1;
	create_nyx_config_entry(false);
}

//===============================
//  ASAP: Hekate info = Credits
//===============================
static lv_res_t _show_about_tab(lv_obj_t *obj)
{
	lv_obj_t *win = nyx_create_standard_window("Ｈ × Ｌ", NULL);
	lv_win_add_btn(win, NULL, SYMBOL_HINT" 테마", _create_window_nyx_colors);
	lv_obj_t *tab = lv_cont_create(win, NULL);
	lv_cont_set_fit(tab, true, true);
	lv_cont_set_layout(tab, LV_LAYOUT_OFF);

	_create_tab_about(lv_theme_get_current(), tab);

	return LV_RES_OK;
}

//======================================
// ASAP: NAND changer button callback.
//======================================
static lv_res_t _emu_btn_signal_cb(lv_obj_t *btn, lv_signal_t sig, void *param) {
	lv_res_t res = _old_emu_sig_cb ? _old_emu_sig_cb(btn, sig, param) : LV_RES_OK;

	if (sig == LV_SIGNAL_PRESSED) {
		lv_obj_set_opa_scale_enable(btn, false);
	}
	else if (sig == LV_SIGNAL_RELEASED || sig == LV_SIGNAL_PRESS_LOST) {
		lv_obj_set_opa_scale_enable(btn, true);
		lv_obj_set_opa_scale(btn, LV_OPA_40);
	}

	return res;
}

//======================================
//  ASAP: Personal custom home - main.
//======================================
static void _nyx_main_menu(lv_theme_t * th)
{
	// Initialize global styles.
	_nyx_set_default_styles(th);

	// Create screen container.
	lv_obj_t *scr = lv_cont_create(NULL, NULL);
	lv_scr_load(scr);
	lv_cont_set_style(scr, th->bg);

	// Create base background and add a custom one if exists.
	lv_obj_t *cnr = lv_cont_create(scr, NULL);
	static lv_style_t base_bg_style;
	lv_style_copy(&base_bg_style, &lv_style_plain_color);
	base_bg_style.body.main_color = th->bg->body.main_color;
	base_bg_style.body.grad_color = base_bg_style.body.main_color;
	lv_cont_set_style(cnr, &base_bg_style);
	lv_obj_set_size(cnr, LV_HOR_RES, LV_VER_RES);

	if (hekate_bg)
	{
		lv_obj_t *img = lv_img_create(cnr, NULL);
		lv_img_set_src(img, hekate_bg);
	}

	// Add tabview page to screen.
	lv_obj_t *tv = lv_tabview_create(scr, NULL);
	
	lv_tabview_set_sliding(tv, false);
	lv_tabview_set_btns_hidden(tv, true);
	lv_obj_set_size(tv, LV_HOR_RES, LV_VER_RES);

	// Battery percentages.
	lv_obj_t *btn_battery = lv_btn_create(scr, NULL);
	lv_btn_set_style(btn_battery, LV_BTN_STYLE_REL, &btn_custom_rel);
	lv_btn_set_style(btn_battery, LV_BTN_STYLE_PR, &btn_custom_pr2);
	lv_btn_set_action(btn_battery, LV_BTN_ACTION_CLICK, _create_window_battery_status);
	lv_obj_set_size(btn_battery, 100, 100);
	lv_btn_set_layout(btn_battery, LV_LAYOUT_OFF);
	lv_obj_align(btn_battery, NULL, LV_ALIGN_IN_TOP_MID, 585, 3);

	lv_obj_t *bty_frame = lv_label_create(scr, NULL);
	lv_label_set_recolor(bty_frame, true);
	lv_label_set_text(bty_frame, "#EFEFEF Ｆ#");
	lv_obj_align(bty_frame, btn_battery, LV_ALIGN_CENTER, 2, -20);

	lv_obj_t *lbl_battery = lv_label_create(scr, NULL);
	lv_obj_set_style(lbl_battery, &hint_small_style);
	lv_label_set_recolor(lbl_battery, true);
	lv_label_set_text(lbl_battery, "00%");
	lv_obj_align(lbl_battery, bty_frame, LV_ALIGN_CENTER, 0, -1);
	status_bar.battery = lbl_battery;

	// Amperages, voltages.
	lbl_battery = lv_label_create(scr, lbl_battery);
	lv_obj_set_style(lbl_battery, &monospace_text);
	lv_obj_set_opa_scale_enable(lbl_battery, true);
	lv_obj_set_opa_scale(lbl_battery, LV_OPA_30);
	lv_label_set_text(lbl_battery, "#47B100 +0 mA#\n 0 mV");
	lv_obj_align(lbl_battery, btn_battery, LV_ALIGN_CENTER, -5, 16);
	status_bar.battery_more = lbl_battery;

	// Hekate info label.
	sd_mount();
	char *txt_buf = (char *)malloc(SZ_16K);
	FILINFO fno;
	emummc_cfg_t emu_info;
	load_emummc_cfg(&emu_info);
	bool ini_exists = (f_stat("emuMMC/emummc.ini", &fno) == FR_OK);
	//char version[32];
	//char rel = (nyx_str->version >> 24) & 0xFF;
	//s_printf(version, "#EFEFEF Ｈ %s%d.%d.%d%c#",
	//		 rel ? "v" : "", nyx_str->version & 0xFF, (nyx_str->version >> 8) & 0xFF, (nyx_str->version >> 16) & 0xFF, rel > 'A' ? rel : 0);
	s_printf(txt_buf, "#EFEFEF Ｈ × Ｌ#");
	lv_obj_t *btn_hekate_ver = lv_btn_create(scr, NULL);
	lv_btn_set_style(btn_hekate_ver, LV_BTN_STYLE_REL, &btn_custom_rel);
	lv_btn_set_style(btn_hekate_ver, LV_BTN_STYLE_PR, &btn_custom_pr2);
	lv_btn_set_action(btn_hekate_ver, LV_BTN_ACTION_CLICK, _show_about_tab);
	lv_obj_set_size(btn_hekate_ver, 220, 60);
	lv_btn_set_layout(btn_hekate_ver, LV_LAYOUT_OFF);
	lv_obj_align(btn_hekate_ver, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, -116, -140);
	lv_obj_t *label = lv_label_create(scr, NULL);
	lv_label_set_recolor(label, true);
	lv_label_set_text(label, txt_buf);
	lv_obj_align(label, btn_hekate_ver, LV_ALIGN_CENTER, 0, 0);

	// Power button.
	lv_obj_t *btn_power_off = lv_btn_create(scr, NULL);
	_create_text_button(th, NULL, btn_power_off, SYMBOL_POWER, _poweroff_action);
	lv_obj_align(btn_power_off, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);

	// Nyx Refresh button.
	lv_obj_t *btn_reload = lv_btn_create(scr, NULL);
	_create_text_button(th, NULL, btn_reload, SYMBOL_REBOOT, reload_action);
	lv_obj_align(btn_reload, btn_power_off, LV_ALIGN_OUT_LEFT_MID, 0, 0);

	// RCM / Filebrowser button.
	lv_obj_t *btn_rcm = lv_btn_create(scr, NULL);
	if (!n_cfg.rcm_button) {
		_create_text_button(th, NULL, btn_rcm, "ｆ", _btn_filebrowser_action);
	} else {
		_create_text_button(th, NULL, btn_rcm, "Ｒ", _btn_rcm_action);
		if (h_cfg.rcm_patched) {
			lv_obj_set_click(btn_rcm, false);
			lv_obj_set_opa_scale_enable(btn_rcm, true);
			lv_obj_set_opa_scale(btn_rcm, LV_OPA_40);
		}
	}
	lv_obj_align(btn_rcm, btn_reload, LV_ALIGN_OUT_LEFT_MID, 0, 0);

	// UMS, HID USB button.
	lv_obj_t *btn_sd_ums = lv_btn_create(scr, NULL);
	_create_text_button(th, NULL, btn_sd_ums, "Ｕ", _btn_action_ums_sd);
	lv_btn_set_action(btn_sd_ums, LV_BTN_ACTION_LONG_PR, _btn_action_hid_jc);
	lv_obj_align(btn_sd_ums, btn_rcm, LV_ALIGN_OUT_LEFT_MID, 0, 0);
	
	// NAND changer button.
	lv_obj_t *btn_toggle_emu = lv_btn_create(scr, NULL);
	_create_text_button(th, NULL, btn_toggle_emu, "Ｔ", _btn_toggle_emu_action);
	lv_obj_set_opa_scale_enable(btn_toggle_emu, true);
	lv_obj_set_opa_scale(btn_toggle_emu, LV_OPA_40);
	lv_obj_set_click(btn_toggle_emu, ini_exists);
	_old_emu_sig_cb = lv_obj_get_signal_func(btn_toggle_emu);
	lv_obj_set_signal_func(btn_toggle_emu, _emu_btn_signal_cb);
	lv_obj_align(btn_toggle_emu, btn_sd_ums, LV_ALIGN_OUT_LEFT_MID, 0, 0);
	btn_toggle_emu_obj = btn_toggle_emu;

	lv_obj_t *btn_emuenabled = lv_label_create(scr, NULL);
	lv_label_set_recolor(btn_emuenabled, true);
	lv_label_set_text(btn_emuenabled, emu_info.enabled ? "Ｙ" : "Ｚ");
	lv_obj_align(btn_emuenabled, btn_toggle_emu, LV_ALIGN_CENTER, emu_info.enabled ? -10 : 9, 0);
	lv_obj_set_hidden(btn_emuenabled, !ini_exists);
	btn_emuenabled_obj = btn_emuenabled;

	// Display BPMP and RAM clock.
	lv_obj_t *btn_clocks = lv_btn_create(scr, NULL);
	lv_btn_set_style(btn_clocks, LV_BTN_STYLE_REL, &btn_custom_rel);
	lv_btn_set_style(btn_clocks, LV_BTN_STYLE_PR, &btn_custom_pr2);
	lv_btn_set_action(btn_clocks, LV_BTN_ACTION_CLICK, _btn_advence_action);
	lv_obj_set_size(btn_clocks, 172, 50);
	lv_btn_set_layout(btn_clocks, LV_LAYOUT_OFF);
	lv_obj_align(btn_clocks, NULL, LV_ALIGN_IN_BOTTOM_MID, 92, -5);

	lv_obj_t *clocks_label = lv_label_create(scr, NULL);
	lv_obj_set_style(clocks_label, &monospace_text);
	lv_obj_set_opa_scale_enable(clocks_label, true);
	lv_obj_set_opa_scale(clocks_label, LV_OPA_30);
	lv_label_set_text(clocks_label, "BPMP:\nRAM :");
	lv_obj_align(clocks_label, btn_clocks, LV_ALIGN_IN_LEFT_MID, 11, 0);

	lv_obj_t *lbl_clocks = lv_label_create(scr, NULL);
	lv_obj_set_style(lbl_clocks, &monospace_text);
	lv_obj_set_opa_scale_enable(lbl_clocks, true);
	lv_obj_set_opa_scale(lbl_clocks, LV_OPA_30);
	lv_label_set_text(lbl_clocks, "----.- MHz\n----.- MHz");
	lv_obj_align(lbl_clocks, clocks_label, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
	status_bar.clocks = lbl_clocks;

	// Display FPS.
	if (n_cfg.show_fps) {
		lv_obj_t *lbl_fps = lv_label_create(scr, NULL);
		lv_obj_set_style(lbl_fps, &monospace_text);
		lv_label_set_recolor(lbl_fps, true);
		lv_obj_set_opa_scale_enable(lbl_fps, true);
		lv_obj_set_opa_scale(lbl_fps, LV_OPA_30);
		lv_label_set_text(lbl_fps, "FPS: 0 ");
		lv_obj_align(lbl_fps, NULL, LV_ALIGN_IN_TOP_LEFT, 4, 1);
		status_bar.fps = lbl_fps;

		lv_task_create(_update_fps, 1000, LV_TASK_PRIO_LOW, NULL);
	}

	lv_obj_t *label_status = lv_btn_create(scr, NULL);
	bool is_8gb = false;
	if (!sd_mount()) {
		if (f_chdrive("sd:") == FR_OK) {
			is_8gb = (f_stat("config/ultrahand/ram_8gb.ini", &fno) == FR_OK);
		}
		sd_unmount();
	}
	s_printf(txt_buf, "%s Ｎ#", is_8gb_case() ? "#FFFFFF" : "#C02C1D");
	_create_text_button(th, NULL, label_status, is_8gb ? txt_buf : "Ｍ", NULL);
	lv_btn_set_action(label_status, LV_BTN_ACTION_CLICK, _create_window_hw_info_status);
	lv_btn_set_action(label_status, LV_BTN_ACTION_LONG_PR, _info_button_action);
	lv_obj_align(label_status, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 30, 0);
	label_status_obj = label_status;

	// Device info label.
	lv_obj_t *label_nand = lv_label_create(scr, NULL);
	lv_label_set_recolor(label_nand, true);
	lv_obj_set_style(label_nand, &hint_small_style);
	label_nand_obj = label_nand;
	refresh_nand_info_label();

	free(txt_buf);
	sd_unmount();

	// Home menu screen
	lv_obj_t *tab_home = lv_tabview_add_tab(tv, "");
	_create_tab_home(th, tab_home);

	lv_tabview_set_tab_act(tv, 0, false);

	// Create tasks.
	system_tasks.task.dram_periodic_comp = lv_task_create(minerva_periodic_training, EMC_PERIODIC_TRAIN_MS, LV_TASK_PRIO_HIGHEST, NULL);
	lv_task_ready(system_tasks.task.dram_periodic_comp);

	system_tasks.task.status_bar = lv_task_create(_update_status_bar, 2000, LV_TASK_PRIO_LOW, NULL);
	lv_task_ready(system_tasks.task.status_bar);

	lv_task_create(_update_clocks, 1000, LV_TASK_PRIO_LOW, NULL);

	lv_task_create(_check_sd_card_removed, 2000, LV_TASK_PRIO_LOWEST, NULL);

	task_emmc_errors = lv_task_create(_nyx_emmc_issues_warning, 2000, LV_TASK_PRIO_LOWEST, NULL);
	lv_task_ready(task_emmc_errors);

	// Check if Nyx was launched with a function set.
	if (nyx_str->cfg & NYX_CFG_UMS)
	{
		nyx_str->cfg &= ~(NYX_CFG_UMS);
		lv_task_t *task_run_ums = lv_task_create(nyx_run_ums, LV_TASK_ONESHOT, LV_TASK_PRIO_LOWEST, (void *)&nyx_str->cfg);
		lv_task_once(task_run_ums);
	}
	else if (0)
		_create_window_home_launch(NULL);

	if (!n_cfg.timeoffset)
	{
		lv_task_t *task_run_clock = lv_task_create(first_time_clock_edit, LV_TASK_ONESHOT, LV_TASK_PRIO_MID, NULL);
		lv_task_once(task_run_clock);
	}

	if (!n_cfg.bpmp_clock)
		task_bpmp_clock = lv_task_create(first_time_bpmp_clock, 10000, LV_TASK_PRIO_LOWEST, NULL);
	
	// ASAP: Main menu pinlock
	/*if (n_cfg.pinlock > 0)
	{
		_create_mbox_unlock();
	}*/
}

//=========================================
//  ASAP: Main menu buttons icon configs.
//=========================================
void refresh_emu_enabled_label(void)
{
	char *txt_buf = (char *)malloc(SZ_16K);
	emummc_cfg_t info = {0};
	FILINFO fno;

	sd_mount();
	g_sd_is_exfat = (sd_fs.fs_type == FS_EXFAT);
	if(f_stat("emuMMC/emummc.ini", &fno) == FR_OK) {
		load_emummc_cfg(&info);
	} else {
		info.enabled = false;
		info.sector  = 0;
	}
	sd_unmount();

	s_printf(txt_buf, info.enabled ? "#DBE8F1 B#" : "#4E4E67 B#");
	lv_label_set_text(atmo_bg_obj, txt_buf);

	s_printf(txt_buf, info.enabled ? "#6060C0 C#" : "#95B7E4 C#");
	lv_label_set_text(atmo_sphere_obj, txt_buf);

	lv_label_set_text(btn_emuenabled_obj, info.enabled ? "Ｙ" : "Ｚ");
	lv_obj_align(btn_emuenabled_obj, btn_toggle_emu_obj, LV_ALIGN_CENTER, info.enabled ? -10 : 9, 0);

	s_printf(txt_buf, info.enabled ? "#EFEFEF I#" : "#727F8E I#");
	lv_label_set_text(nandmng_label, txt_buf);

	const char *emu_colors[] = { "#EBAF0C J#", "#B5D5E6 K#", "#EFEFEF L#", "#21322C M#", "#252084 N#", "#0F0C44 V#" };
	const char *sys_colors[] = { "#EBAF0C J#", "#014A88 K#", "#014A88 L#", "#21322C M#", "#EFEFEF O#", "#CACBCC V#" };
	const int nm_xoffsets[]  = { 16, 0, -25, 17, -7, -57 };
	const int emu_yoffsets[] = { 0, 0, 0, 0, 0, 0 };
	const int sys_yoffsets[] = { 0, -49, 0, 0, 0, 0 };

	for (int i = 0; i < 6; i++) {
		s_printf(txt_buf, info.enabled ? emu_colors[i] : sys_colors[i]);
		lv_label_set_text(nandmng_color_labels[i], txt_buf);
		lv_obj_align(nandmng_color_labels[i], nandmng_label, LV_ALIGN_CENTER, nm_xoffsets[i], info.enabled ? emu_yoffsets[i] : sys_yoffsets[i]);
	}

	s_printf(txt_buf, "%s %s", info.enabled ? "#EFEFEF" : "#727F8E", g_sd_is_exfat ? "Q#" : "P#");
	lv_label_set_text(nandmng_format_label, txt_buf);

	s_printf(txt_buf, "%s %s", info.enabled ? "#BBC3C0" : "#727F8E", info.enabled ? (info.sector ? "T#" : "U#") : (g_sd_is_exfat ? "R#" : "S#"));
	lv_label_set_text(nandmng_ftype_label, txt_buf);
}

// ASAP: Main menu info label configs.
void refresh_nand_info_label(void)
{
	char txt_buf[SZ_4K] = {0};
	emummc_cfg_t emu_info = {0};
	FILINFO fno;
	bool ini_exists;
	const pkg1_id_t *id = NULL;
	char build_date[32] = {0};
	u8 *pkg1_buf = NULL;
	char info_buf[128] = {0};
	//const char *nand_info = NULL;
	const u32 BOOTLOADER_SIZE = SZ_256K;
	const u32 BOOTLOADER_MAIN_OFFSET = 0x100000;
	u32 pk1_off = h_cfg.t210b01 ? sizeof(bl_hdr_t210b01_t) : 0;

	sd_mount();
	g_sd_is_exfat = (sd_fs.fs_type == FS_EXFAT);

	ini_exists = (f_stat("emuMMC/emummc.ini", &fno) == FR_OK);
	if (ini_exists) load_emummc_cfg(&emu_info);
	else emu_info.enabled = false;

	const char *chip = (hw_get_chip_id() == GP_HIDREV_MAJOR_T210) ? "Erista" : "Mariko";
	const char *sku;
	switch (fuse_read_hw_type()) {
		case FUSE_NX_HW_TYPE_ICOSA: sku = "구형"; break;
		case FUSE_NX_HW_TYPE_IOWA:	sku = "배터리 개선판"; break;
		case FUSE_NX_HW_TYPE_HOAG:	sku = "Lite"; break;
		case FUSE_NX_HW_TYPE_AULA:	sku = "OLED"; break;
		default:					sku = "#FF8000 Unknown#"; break;
	}
	//const char *fs_label = g_sd_is_exfat ? "#C02C1D exFAT#" : "FAT32";
	char fs_label[64];

	if (g_sd_is_exfat) {
		if (emu_info.enabled && emu_info.sector) 
			strcpy(fs_label, "#C02C1D exFAT#+FAT32");
		else
			strcpy(fs_label, "#C02C1D exFAT#");
	} else {
		strcpy(fs_label, "FAT32");
	}
	const char *emu_label = (!emu_info.enabled)
		? (ini_exists ? "#FF8800 시스낸드#" : "에뮤낸드 미생성, #FF8800 시스낸드#로 연결됩니다")
		: (emu_info.sector ? "파티션 #00FFCC 에뮤낸드#" : "파일 #00FFCC 에뮤낸드#");

	s_printf(txt_buf, "%s [%s] · %s · %s", chip, sku, fs_label, emu_label);

	pkg1_buf = zalloc(BOOTLOADER_SIZE);
	bool read_ok = false;
	if (!emu_info.enabled && !emmc_initialize(false)) {
		emmc_set_partition(EMMC_BOOT0);
		sdmmc_storage_read(&emmc_storage, BOOTLOADER_MAIN_OFFSET / EMMC_BLOCKSIZE,
						   BOOTLOADER_SIZE / EMMC_BLOCKSIZE, pkg1_buf);
		emmc_end();
		read_ok = true;
	} else if (emu_info.enabled && emu_info.sector) {
		sdmmc_storage_read(&sd_storage, emu_info.sector + (BOOTLOADER_MAIN_OFFSET / EMMC_BLOCKSIZE),
						   BOOTLOADER_SIZE / EMMC_BLOCKSIZE, pkg1_buf);
		read_ok = true;
	} else if (emu_info.enabled) {
		FIL fp; UINT br;
		char path[128];
		s_printf(path, "%s/eMMC/BOOT0", emu_info.path);
		if (f_open(&fp, path, FA_READ) == FR_OK) {
			f_lseek(&fp, BOOTLOADER_MAIN_OFFSET);
			f_read(&fp, pkg1_buf, BOOTLOADER_SIZE, &br);
			f_close(&fp);
			read_ok = true;
		}
	}

	if (read_ok) id = pkg1_identify(pkg1_buf + pk1_off, build_date);
	free(pkg1_buf);

	unsigned long long bd_val = read_ok ? strtoull(build_date, NULL, 10) : 0ULL;
	const char *fusee_ver;
	const char *prev_ver;
	
	switch (bd_val) {
		case 20161121183008ULL: fusee_ver = "1.0.0";		   prev_ver = "";		break;
		case 20170210155124ULL: fusee_ver = "2.0.0 - 2.3.0";   prev_ver = "1.0.0";  break;
		case 20170519101410ULL: fusee_ver = "3.0.0";		   prev_ver = "2.3.0";  break;
		case 20170710161758ULL: fusee_ver = "3.0.1 - 3.0.2";   prev_ver = "3.0.0";  break;
		case 20170921172629ULL: fusee_ver = "4.0.0 - 4.1.0";   prev_ver = "3.0.2";  break;
		case 20180220163747ULL: fusee_ver = "5.0.0 - 5.1.0";   prev_ver = "4.1.0";  break;
		case 20180802162753ULL: fusee_ver = "6.0.0 - 6.1.0";   prev_ver = "5.1.0";  break;
		case 20181107105733ULL: fusee_ver = "6.2.0";		   prev_ver = "6.1.0";  break;
		case 20181218175730ULL: fusee_ver = "7.0.0";		   prev_ver = "6.2.0";  break;
		case 20190208150037ULL: fusee_ver = "7.0.1";		   prev_ver = "7.0.0";  break;
		case 20190314172056ULL: fusee_ver = "8.0.0 - 8.0.1";   prev_ver = "7.0.1";  break;
		case 20190531152432ULL: fusee_ver = "8.1.0 - 8.1.1";   prev_ver = "8.0.1";  break;
		case 20190809135709ULL: fusee_ver = "9.0.0 - 9.0.1";   prev_ver = "8.1.1";  break;
		case 20191021113848ULL: fusee_ver = "9.1.0 - 9.2.0";   prev_ver = "9.0.1";  break;
		case 20200303104606ULL: fusee_ver = "10.0.0 - 10.2.0"; prev_ver = "9.2.0";  break;
		case 20201030110855ULL: fusee_ver = "11.0.0 - 11.0.1"; prev_ver = "10.2.0"; break;
		case 20210129111626ULL: fusee_ver = "12.0.0 - 12.0.1"; prev_ver = "11.0.1"; break;
		case 20210422145837ULL: fusee_ver = "12.0.2 - 12.0.3"; prev_ver = "12.0.1"; break;
		case 20210607122020ULL: fusee_ver = "12.1.0";		   prev_ver = "12.0.3"; break;
		case 20210805123738ULL: fusee_ver = "13.0.0 - 13.2.0"; prev_ver = "12.1.0"; break;
		case 20220105094439ULL: fusee_ver = "13.2.1";		   prev_ver = "13.2.0"; break;
		case 20220209100019ULL: fusee_ver = "14.0.0 - 14.1.2"; prev_ver = "13.2.1"; break;
		case 20220801142548ULL: fusee_ver = "15.0.0 - 15.0.1"; prev_ver = "14.1.2"; break;
		case 20230111100014ULL: fusee_ver = "16.0.0 - 16.1.0"; prev_ver = "15.0.1"; break;
		case 20230906134551ULL: fusee_ver = "17.0.0 - 17.0.1"; prev_ver = "16.1.0"; break;
		case 20240207110330ULL: fusee_ver = "18.0.0 - 18.1.0"; prev_ver = "17.0.1"; break;
		case 20240808143958ULL: fusee_ver = "19.0.0 - 19.0.1"; prev_ver = "18.1.0"; break;
		case 20250206151829ULL: fusee_ver = "20.0.0 - 20.5.0"; prev_ver = "19.0.1"; break;
		case 20251009153823ULL: fusee_ver = "21.0.0 - 21.2.0"; prev_ver = "20.5.0"; break;
		case 20260123111804ULL: fusee_ver = "22.0.0 - 22.5.0"; prev_ver = "21.2.0"; break;
		default:				fusee_ver = "미지원 펌웨어";	prev_ver = "22.5.0"; break;
	}

	if (!id) {
		if (!emu_info.enabled) {
			s_printf(info_buf, "\n안내: #FF8800 %s 설치됨, %s 다운그레이드 혹은 최신 버전 Ｌ 업데이트 필요#", fusee_ver, prev_ver);
		} else if (emu_info.sector) {
			s_printf(info_buf, "\n안내: #FF8800 %s 설치됨, %s 다운그레이드 혹은 최신 버전 Ｌ 업데이트 필요#", fusee_ver, prev_ver);
		} else {
			s_printf(info_buf, "\n안내: #FF8800 %s 설치됨, %s 다운그레이드 혹은 최신 버전 Ｌ 업데이트 필요#", fusee_ver, prev_ver);
		}
		strcat(txt_buf, info_buf);
	}

	if (sd_fs.fs_type == FS_EXFAT) {
		strcat(txt_buf, "\n#C02C1D 경고#: exFAT은 파일 시스템 파손 가능성 높음, #00FFCC FAT32# 포맷 권장");
	}

	lv_label_set_text(label_nand_obj, txt_buf);
	lv_obj_align(label_nand_obj, label_status_obj, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

	sd_unmount();
}

void nyx_load_and_run()
{
	memset(&system_tasks, 0, sizeof(system_maintenance_tasks_t));

	lv_init();
	gfx_con.fillbg = 1;

	// Initialize framebuffer drawing functions.
	lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.disp_flush = _disp_fb_flush;
	lv_disp_drv_register(&disp_drv);

	// Initialize Joy-Con.
	if (!n_cfg.jc_disable)
	{
		lv_task_t *task_jc_init_hw = lv_task_create(jc_init_hw, LV_TASK_ONESHOT, LV_TASK_PRIO_LOWEST, NULL);
		lv_task_once(task_jc_init_hw);
	}
	lv_indev_drv_t indev_drv_jc;
	lv_indev_drv_init(&indev_drv_jc);
	indev_drv_jc.type = LV_INDEV_TYPE_POINTER;
	indev_drv_jc.read = _jc_virt_mouse_read;
	memset(&jc_drv_ctx, 0, sizeof(jc_lv_driver_t));
	jc_drv_ctx.indev_jc = lv_indev_drv_register(&indev_drv_jc);
	close_btn = NULL;

	// Initialize touch.
	touch_enabled = !touch_power_on();
	lv_indev_drv_t indev_drv_touch;
	lv_indev_drv_init(&indev_drv_touch);
	indev_drv_touch.type = LV_INDEV_TYPE_POINTER;
	indev_drv_touch.read = _fts_touch_read;
	jc_drv_ctx.indev_touch = lv_indev_drv_register(&indev_drv_touch);
	touchpad.touch = false;

	// Initialize temperature sensor.
	tmp451_init();

	// Set hekate theme based on chosen hue.
	lv_theme_t *th = lv_theme_hekate_init(n_cfg.theme_bg, n_cfg.theme_color, NULL); // n_cfg.theme_bg 0x0E0E1A
	lv_theme_set_current(th);

	// Create main menu
	_nyx_main_menu(th);

	jc_drv_ctx.cursor = lv_img_create(lv_scr_act(), NULL);
	lv_img_set_src(jc_drv_ctx.cursor, &touch_cursor);
	lv_obj_set_opa_scale(jc_drv_ctx.cursor, LV_OPA_TRANSP);
	lv_obj_set_opa_scale_enable(jc_drv_ctx.cursor, true);

	// Check if sd card issues.
	if (sd_get_mode() == SD_1BIT_HS25)
	{
		lv_task_t *task_run_sd_errors = lv_task_create(_nyx_sd_card_issues_warning, LV_TASK_ONESHOT, LV_TASK_PRIO_LOWEST, NULL);
		lv_task_once(task_run_sd_errors);
	}

	// Gui loop.
	if (h_cfg.t210b01)
	{
		// Minerva not supported on T210B01 yet. Slight power saving via spinlock.
		while (true)
		{
			lv_task_handler();
			usleep(400);
		}
	}
	else
	{
		// Alternate DRAM frequencies. Total stall < 1ms. Saves 300+ mW.
		while (true)
		{
			minerva_change_freq(FREQ_1600);  // Takes 295 us.

			lv_task_handler();

			minerva_change_freq(FREQ_800);   // Takes 80 us.
			usleep(125); // Min 20us.
		}
	}
}
