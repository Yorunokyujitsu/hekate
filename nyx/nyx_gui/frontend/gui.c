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
//==========================

extern volatile boot_cfg_t *b_cfg;
extern volatile nyx_storage_t *nyx_str;

extern lv_res_t launch_payload(lv_obj_t *list);

//===============================================
//  ASAP: bool, lv, exturn, struct, unit, enum.
//===============================================
extern lv_res_t launch_fusee(lv_obj_t *list);
extern lv_res_t launch_module(lv_obj_t *list);
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
} entry_t;

static entry_t entries[MAX_HOME_ENTRIES];

typedef struct _launch_menu_entries_t
{
	lv_obj_t *btn[20];
	lv_obj_t *label[20];
	uint8_t dd_map[6];
    uint8_t dd_count; 
} launch_menu_entries_t;

static launch_menu_entries_t launch_ctxt;
static lv_obj_t *launch_bg = NULL;
static bool launch_bg_done = false;

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

const lv_img_dsc_t *icon_switch;
const lv_img_dsc_t *icon_payload;
lv_img_dsc_t *icon_lakka;

const lv_img_dsc_t *hekate_bg;

lv_style_t btn_transp_rel, btn_transp_pr, btn_transp_tgl_rel, btn_transp_tgl_pr;
lv_style_t ddlist_transp_bg, ddlist_transp_sel;

lv_style_t mbox_darken;

char *text_color;

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
	lv_mbox_set_text(mbox, SYMBOL_CAMERA"  #FFBA00 Saving screenshot#");
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
		lv_mbox_set_text(mbox, SYMBOL_CAMERA"  #96FF00 Screenshot saved!#");
	else
		lv_mbox_set_text(mbox, SYMBOL_WARNING"  #FF8000 Screenshot failed!#");
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
	if (disp_init_done)
		vic_compose();

	// Check if display init was done. If it's the first big draw, init.
	if (!disp_init_done && ((x2 - x1 + 1) > 600))
	{
		disp_init_done = true;
		_nyx_disp_init();
	}

	lv_flush_ready();
}

static touch_event touchpad;
static bool touch_enabled;
static bool console_enabled = false;

static bool _fts_touch_read(lv_indev_data_t *data)
{
	if (touch_enabled)
		touch_poll(&touchpad);
	else
		return false;

	// Take a screenshot if 3 fingers.
	if (touchpad.fingers > 2)
	{
		_save_fb_to_bmp();

		data->state = LV_INDEV_STATE_REL;
		return false;
	}

	if (console_enabled)
	{
		// Print input debugging in console.
		gfx_con_getpos(&gfx_con.savedx, &gfx_con.savedy, &gfx_con.savedcol);
		gfx_con_setpos(32, 638, GFX_COL_AUTO);
		gfx_con.fntsz = 8;
		gfx_printf("x: %4d, y: %4d | z: %3d | ", touchpad.x, touchpad.y, touchpad.z);
		gfx_printf("1: %02X, 2: %02X, 3: %02X, ", touchpad.raw[1], touchpad.raw[2], touchpad.raw[3]);
		gfx_printf("4: %02X, 5: %02X, 6: %02X, 7: %02X",
			touchpad.raw[4], touchpad.raw[5], touchpad.raw[6], touchpad.raw[7]);
		gfx_con_setpos(gfx_con.savedx, gfx_con.savedy, gfx_con.savedcol);
		gfx_con.fntsz = 16;

		return false;
	}

	// Always set touch points.
	data->point.x = touchpad.x;
	data->point.y = touchpad.y;

	// Decide touch enable.
	switch (touchpad.type & STMFTS_MASK_EVENT_ID)
	{
	case STMFTS_EV_MULTI_TOUCH_ENTER:
	case STMFTS_EV_MULTI_TOUCH_MOTION:
		data->state = LV_INDEV_STATE_PR;
		break;
	case STMFTS_EV_MULTI_TOUCH_LEAVE:
		data->state = LV_INDEV_STATE_REL;
		break;
	case STMFTS_EV_NO_EVENT:
	default:
		if (touchpad.touch)
			data->state = LV_INDEV_STATE_PR;
		else
			data->state = LV_INDEV_STATE_REL;
		break;
	}

	return false; // No buffering so no more data read.
}

static bool _jc_virt_mouse_read(lv_indev_data_t *data)
{
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
			if (jc_pad->plus || jc_pad->minus)
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
	if (jc_pad->plus || jc_pad->minus)
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

	if (jc_pad->b && close_btn)
	{
		lv_action_t close_btn_action = lv_btn_get_action(close_btn, LV_BTN_ACTION_CLICK);
		close_btn_action(close_btn);
		close_btn = NULL;
	}

	return false; // No buffering so no more data read.
}

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
		u32 offset_copy = ALIGN((u32)bitmap + sizeof(lv_img_dsc_t), 0x10);

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

lv_res_t mbox_action(lv_obj_t *btns, const char *txt)
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

		static const char * mbox_btn_map[] = { "\251", "\222OK", "\251", "" };
		lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
		lv_mbox_set_recolor_text(mbox, true);

		lv_mbox_set_text(mbox,
			"#008EED Low Battery Warning#\n\n"
			"#FFBA00 Info#: Battery is not enough to carry on#\n"
			"with selected operation!#\n\n"
			"Charge to at least #C7EA46 3650 mV#, and try again!");

		lv_mbox_add_btns(mbox, mbox_btn_map, mbox_action);
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

	static const char * mbox_btn_map[] = { "\251", "\222OK", "\251", "" };
	lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);

	lv_mbox_set_text(mbox,
		"#008EED SD Card Issues Warning#\n\n"
		"#FFBA00 Info#: The SD Card is initialized in 1-bit mode!\n"
		"#FF8000 This might mean detached or broken connector!#\n\n"
		"You can check the details in #C7EA46 Ⓝ (NAND Manager)#");

	lv_mbox_add_btns(mbox, mbox_btn_map, mbox_action);
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

lv_res_t nyx_win_close_action_custom(lv_obj_t * btn)
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

	close_btn = lv_win_add_btn(win, NULL, SYMBOL_CLOSE " Close", close_action);

	return win;
}
//============================

lv_obj_t *nyx_create_standard_window(const char *win_title)
{
	return _nyx_create_window(win_title, nyx_win_close_action_custom);
}

lv_obj_t *nyx_create_window_custom_close_btn(const char *win_title, lv_action_t rel_action)
{
	return _nyx_create_window(win_title, rel_action);
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

	return mbox_action(btns, txt);
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

		static const char * mbox_btn_map[] = { "\221Ｒ", "\221Power Off", "\221OK", "" };
		static const char * mbox_btn_map_rcm_patched[] = { "\221Reboot", "\221Power Off", "\221OK", "" };
		lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
		lv_mbox_set_recolor_text(mbox, true);
		lv_obj_set_width(mbox, LV_HOR_RES * 6 / 9);

		lv_mbox_set_text(mbox,
						 "\n#008EED Status Message#\n\n"
						 "#FFBA00 Info#: SD card was removed!\n\n"
						 "#FF8000 Warning:#\n#FF8000 Some features are limited.#\n"
						 "#FF8000 Reinsert the SD card to continue properly!#");
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

		static const char * mbox_btn_map[] = { "\251", "\222OK", "\251", "" };
		lv_obj_t * mbox = lv_mbox_create(dark_bg, NULL);
		lv_mbox_set_recolor_text(mbox, true);

		lv_mbox_set_text(mbox,
			"#008EED eMMC Issues Warning#\n\n"
			"#FFBA00 Info#: Your eMMC is initialized in a slower mode!\n"
			"#FF8000 This might mean hardware issues!#\n\n"
			"You can check the details in #C7EA46 Ⓢ (NAND Manager)#");

		lv_mbox_add_btns(mbox, mbox_btn_map, mbox_action);
		lv_obj_set_width(mbox, LV_HOR_RES / 9 * 5);
		lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
		lv_obj_set_top(mbox, true);
	}
}

//==================================================================================
//  ASAP: CFW/OFW/RCM boot, shutdown process - Updated hekate & nyx 6.5.1 & 1.9.1.
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
static bool is_8gb_case(void)
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

	if (!sd_mount())
		return false;

	if (f_chdrive("sd:") != FR_OK)
		goto out;

	bool has_ram_8gb_ini =
		(f_stat("config/ultrahand/ram_8gb.ini", &fno) == FR_OK);
	bool has_exosphere_8gb =
		(f_stat("atmosphere/config/exosphere.bin", &fno) == FR_OK);

	ret = has_ram_8gb_ini && has_exosphere_8gb;

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
			g_restore_step = "Failed to read the file!";
			break;
		}
		if (br == 0)
			break;

		fr = f_write(&fdst, buf, br, &bw);
		if (fr != FR_OK || bw != br) {
			g_restore_fr = fr;
			g_restore_step = "Failed to write the file!";
			break;
		}
	}

	if (fr == FR_OK)
		f_sync(&fdst);

	f_close(&fsrc);
	f_close(&fdst);

	return (fr == FR_OK);
}
// File - rename.
static bool _rename_if_exists(const char *src, const char *dst)
{
	FILINFO fno;
	FRESULT fr;

	fr = f_stat(src, &fno);
	if (fr == FR_NO_FILE || fr == FR_NO_PATH)
		return true;

	if (fr != FR_OK) {
		g_restore_fr = fr;
		g_restore_step = "File not found!";
		return false;
	}

	f_unlink(dst);

	fr = f_rename(src, dst);
	if (fr != FR_OK) {
		g_restore_fr = fr;
		g_restore_step = "Failed to switch file!";
		return false;
	}

	return true;
}
// Restore to a environment when a DRAM mismatch is detected.
static bool _restore_ram_mode(ram_mode_t mode)
{
	const char *src_hekate;
	const char *src_fusee;
	const char *src_exosphere;
	const char *dst_exosphere;
	const char *src_ram_ini;
	const char *dst_ram_ini;

	const char *dst_payload = "payload.bin";
	const char *dst_update  = "bootloader/update.bin";
	const char *dst_fusee   = "bootloader/payloads/fusee.bin";

	if (mode == RAM_MODE_4GB) {
		src_hekate     = "switch/.packages/.offload/ram_expansion/hekate_4gb.bin";
		src_fusee      = "switch/.packages/.offload/ram_expansion/fusee_4gb.bin";
		src_exosphere  = "atmosphere/config/exosphere.bin";
		dst_exosphere  = "atmosphere/config/exosphere_.bin";
		src_ram_ini    = "config/ultrahand/ram_8gb.ini";
		dst_ram_ini    = "config/ultrahand/ram_4gb.ini";
	} else {
		src_hekate     = "switch/.packages/.offload/ram_expansion/hekate_8gb.bin";
		src_fusee      = "switch/.packages/.offload/ram_expansion/fusee_8gb.bin";
		src_exosphere  = "atmosphere/config/exosphere_.bin";
		dst_exosphere  = "atmosphere/config/exosphere.bin";
		src_ram_ini    = "config/ultrahand/ram_4gb.ini";
		dst_ram_ini    = "config/ultrahand/ram_8gb.ini";
	}

	bool ok = false;

	if (!sd_mount()) {
		g_restore_fr = FR_NOT_READY;
		g_restore_step = "Failed to mount SD card!";
		return false;
	}

	if (f_chdrive("sd:") != FR_OK) {
		g_restore_fr = FR_NOT_READY;
		g_restore_step = "Failed to switch SD drive!";
		goto out;
	}

	g_restore_step = "Failed to restore hekate!";
	if (!sd_copy_file_mounted(src_hekate, dst_payload))
		goto out;
	if (!sd_copy_file_mounted(src_hekate, dst_update))
		goto out;

	g_restore_step = "Failed to restore fusee!";
	if (!sd_copy_file_mounted(src_fusee, dst_fusee))
		goto out;

	g_restore_step = "Failed to switch exosphere!";
	if (!_rename_if_exists(src_exosphere, dst_exosphere))
		goto out;

	g_restore_step = "Failed to switch RAM configuration!";
	if (!_rename_if_exists(src_ram_ini, dst_ram_ini))
		goto out;

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
		mbox_action(btns, txt);
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

	static const char *btn_fuse7[]   = { "\221OK", "" };
	static const char *btn_dram[]    = { "\221Restore", "\221Continue", "" };
	static const char *btn_restore[] = { "\221Restart", "" };

	const char *text = NULL;
	const char **btn_map = NULL;
	lv_btnm_action_t action = NULL;

	char dbg[256];

	if (g_ofw_fuse7_warning) {
		btn_map = btn_fuse7;
		action  = mbox_action;

		text = "#FF0012 Warning#\n\n"
			   "#FFBA00 Info#: #FF8000 Unsupported DRAM fuse configuration!#\n\n"
			   "#C7EA46 Actual device#: Erista + 8GB RAM\n"
			   "#C7EA46 Detected as#: Mariko + 8GB RAM\n\n"
			   "The installed memory is not supported by OFW.\n\n"
			   "#FF0012 Boot may fail during the DRAM training stage,#\n"
			   "#FF0012 causing infinite reboot loops or a black screen.#\n\n"
			   "#C7EA46 Using emuMMC only is strongly recommended.#";
	} else if (g_ofw_dram_warning) {
		btn_map = btn_dram;
		action  = _mbox_ofw_dram_action;

		text = "#FF8000 Caution#\n\n"
			   "#FFBA00 Info#: #FF8000 DRAM fuse info mismatch with hardware!#\n"
			   "#FF8000 Or 8GB RAM mode is currently enabled.#\n\n"
			   "#008EED Hint: If this device has no memory upgrade,#\n"
			   "#008EED restore the original configuration.#\n\n"
			   "DRAM training is limited and does not match\n"
			   "the actual memory timings, resulting in performance degradation.\n\n"
			   "#C7EA46 Do you want to continue booting anyway?#";
	} else {
		btn_map = btn_restore;
		action  = _mbox_restore_failed_action;

		s_printf(dbg,
			"#FF0012 Error#\n\n"
			"#FFDD00 Failed to restore 4GB RAM mode!#\n\n"
			"#00DDFF Details#: %s (%d)",
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

	static const char *btn_restore[] = { "\221Restore 4GB RAM Mode", "" };

	const char *text =
		"#FF0012 Warning#\n\n"
		"#FFBA00 Info#: #FF8000 Unsupported memory mode detected!#\n\n"
		"#00DDFF RAM status#: #C7EA46 Configured#-8GB / #C7EA46 Actual#-4GB\n\n"
		"This mode does not work on this device.\n"
		"Use one matching the actual RAM configuration.\n\n"
		"#008EED Restore is required to boot Ⓓ.#";

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

//=============================
//  ASAP: Info Button action.
//=============================
static lv_res_t _info_button_action(lv_obj_t *btn)
{
	if (_restore_ram_mode(is_current_ram_mode() ? RAM_MODE_4GB : RAM_MODE_8GB)) {
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

	return mbox_action(btns, txt);
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

	static const char * mbox_btn_map[] = { "\221Unlock", "\221Cancel", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES / 2);

	lv_mbox_set_text(mbox, "Enter PIN");

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

DECL_PIN_ACTION(_btn_locktlas_action, launch_module)
DECL_PIN_ACTION(_btn_nandmng_action, create_win_emummc_tools)
DECL_PIN_ACTION(_btn_toggle_emu_action, _toggle_mmc_action)
DECL_PIN_ACTION(_btn_action_ums_sd, action_ums_sd)
DECL_PIN_ACTION(_btn_action_hid_jc, _action_hid_jc)
DECL_PIN_ACTION(_btn_rcm_action, _reboot_rcm_action)
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
		btn_onoff_pr_hos_style.body.main_color = LV_COLOR_HEX(theme_bg_color ? (theme_bg_color + 0x101010) : 0x2D2D2D);
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

static void _create_text_button(lv_theme_t *th, lv_obj_t *parent, lv_obj_t *btn, const char *btn_name, lv_action_t action)
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
		btn_onoff_pr_hos_style.body.main_color = LV_COLOR_HEX(theme_bg_color ? (theme_bg_color + 0x101010) : 0x2D2D2D);
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
	lv_obj_t * lbl_credits = lv_label_create(parent, NULL);

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
		" - FatFs R0.13c,\n"
		"   Copyright (c) 2006-2018, #FFFFFF ChaN#\n"
		"   Copyright (c) 2018-2022, #FF0012 CTC##FFFFFF aer#\n\n"
		" - blz,\n"
		"   Copyright (c) 2018, #FFFFFF SciresM#\n\n"
		" - elfload,\n"
		"   Copyright (c) 2014, #FFFFFF Owen Shepherd#\n"
		"   Copyright (c) 2018, #FFFFFF M4xw#\n\n"
		" - bcl-1.2.0,\n"
		"   Copyright (c) 2003-2006, #FFFFFF Marcus Geelnard#\n\n"
		" - Littlev Graphics Library,\n"
		"   Copyright (c) 2016-2018, #FFFFFF Gabor Kiss-Vamosi#\n"
	);

	lv_obj_t * asap_credits = lv_label_create(parent, NULL);
	lv_obj_align(asap_credits, lbl_credits, LV_ALIGN_IN_TOP_RIGHT, -LV_DPI / 8, 0);
	lv_label_set_style(asap_credits, &monospace_text);
	lv_label_set_recolor(asap_credits, true);
	lv_label_set_static_text(asap_credits,
		"\n#00FFCC ASAP# - #00FFCC A##FFFFFF sa's# #00FFCC S##FFFFFF witch# #00FFCC A##FFFFFF ll-in-one# #00FFCC P##FFFFFF ackage#\n"
		"- #C7EA46 Developer#: 2020-2024, #00FFCC Asa#\n"
		"             2025-2026, #00FFCC Yorunokyujitsu#\n\n"
		"Contents\n"
		" #F3F3F3 Hekate#, #CBCBCB Atmosphère#, #F3F3F3 ATLAS#, #CBCBCB sys-patch#,\n"
		" #CBCBCB Ultrahand#, #F3F3F3 ovlloader+#, #CBCBCB ovl-sysmodule#,\n"
		" #F3F3F3 ASAP-Packages#, #CBCBCB EOS#, #F3F3F3 SaltyNX#, #CBCBCB sys-con#,\n"
		" #CBCBCB MissionControl#, #F3F3F3 EdiZon#, #CBCBCB ReverseNX-RT#,\n"
		" #F3F3F3 NX-FanControl#, #CBCBCB FPSLocker#, #F3F3F3 sys-clk-oc#,\n"
		" #CBCBCB Status-Monitor#, #F3F3F3 emuiibo#, #CBCBCB ovlreloader#,\n"
		" #F3F3F3 Sphaira#, #CBCBCB ASAP-Updater#, #F3F3F3 Daybreak#, #CBCBCB DBI#,\n"
		" #CBCBCB Reboot_to_payload#, #F3F3F3 Linkalho#, #CBCBCB Tinfoil#,\n"
		" #F3F3F3 AmiiboGenerator#\n\n"
		"Credits\n"
		" #00CCFF switchbrew#, #00E4FF ITotalJustice#, #00CCFF proferabg#,\n"
		" #00E4FF shchmue#, #00CCFF SuchMemeManySkill#, #00E4FF rdmrocha#,\n"
		" #00CCFF borntohonk#, #00E4FF ndeadly#, #00CCFF duckbill#, #00E4FF halop#,\n"
		" #00E4FF HamletDuFromage#, #00CCFF ppkantorski#, #00E4FF blawar#,\n"
		" #00CCFF masagrator#, #00E4FF yusufakg#, #00CCFF o0Zz#, #00E4FF XorTroll#,\n"
		" #00E4FF Hwfly-nx#, #00CCFF Morce3232#, #00E4FF impeeza#, #00CCFF rehius#,\n"
		" #00CCFF Zathawo#, #00E4FF sthetix#, #00CCFF mrdude2478#"
	);

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
	static const char *weekday_str[7] = {"#D03838 Sun#", "Mon", "Tue", "Wed", "Thu", "Fri", "#3F70F9 Sat#"};

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
	s_printf(label, " #%s%d", batt_curr >= 0 ? "47B100 +" : "C02C1D ", batt_curr / 1000);
	bool voltage_empty = batt_volt < 3200;
	s_printf(label + strlen(label), " mA#\n %s%d mV%s",
		voltage_empty ? "#FF8000 " : "", batt_volt,  voltage_empty ? " "SYMBOL_WARNING"#" : "");
	lv_label_set_text(status_bar.battery_more, label);
	lv_obj_realign(status_bar.battery_more);
}

static lv_res_t _create_mbox_payloads(lv_obj_t *btn)
{
	lv_obj_t *dark_bg = lv_obj_create(lv_scr_act(), NULL);
	lv_obj_set_style(dark_bg, &mbox_darken);
	lv_obj_set_size(dark_bg, LV_HOR_RES, LV_VER_RES);

	static const char * mbox_btn_map[] = { "\251", "\222Cancel", "\251", "" };
	lv_obj_t *mbox = lv_mbox_create(dark_bg, NULL);
	lv_mbox_set_recolor_text(mbox, true);
	lv_obj_set_width(mbox, LV_HOR_RES * 5 / 9);

	lv_mbox_set_text(mbox, "#008EED Payload Launcher#\n\n#FFBA00 Info#: #FF8000 sdmc:/bootloader/payloads#");

	// Create a list with all found payloads.
	//! TODO: SHould that be tabs with buttons? + Icon support?
	lv_obj_t *list = lv_list_create(mbox, NULL);
	payload_list = list;
	lv_obj_set_size(list, LV_HOR_RES * 3 / 7, LV_VER_RES * 3 / 7);
	lv_list_set_single_mode(list, true);

	if (!sd_mount())
	{
		lv_mbox_set_text(mbox, "#FFBA00 Failed to init SD!#");

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
	lv_mbox_add_btns(mbox, mbox_btn_map, mbox_action);

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

	close_btn = lv_win_add_btn(win, NULL, SYMBOL_CLOSE" Close", _win_launch_close_action);

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

	if (!sd_mount())
		return NULL;

	LIST_INIT(ini_sections);

	if (ini_parse(&ini_sections, "atmosphere/config/version.inc", false)) {
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

	lv_obj_t *win = create_window_launch(SYMBOL_HOME "  Launch · Boot Configuration");
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

	if (ini_parse(&ini_sections, "bootloader/ini", true)) {
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
				ddlabels[e] = "UPDATE";
				e++;
				continue;
			}
			if ((payload_val && !strcmp(payload_val, "bootloader/payloads/fusee.bin")) ||
				(emummcforce_val && atoi(emummcforce_val) == 1) ||
				(emummc_disable_val && atoi(emummc_disable_val) == 1 && !stock_val))
			{
				entries[e] = (entry_t){ .icon = emu_info.enabled ? &fusee_entry : &cfw_entry, .label = emu_info.enabled ? "EMU-CFW" : "SYS-CFW", .is_stock = false, .is_cfw = true };
				ddlabels[e] = emu_info.enabled ? "EmuNAND" : "SysNAND";
				found_cfw = true;
				e++;
				continue;
			}
			if (stock_val && atoi(stock_val) == 1) {
				if (found_stock)
					continue;
				entries[e] = (entry_t){ .icon = &ofw_entry, .label = "SYS-STOCK", .is_stock = true, .is_cfw = false };
				ddlabels[e] = "Stock";
				found_stock = true;
				e++;
				continue;
			}
			if (id_val && !strcmp(id_val, "SWANDR")) {
				entries[e] = (entry_t){ .icon = &android_entry, .label = "ANDROID", .is_stock = false, .is_cfw = false };
				ddlabels[e] = "Ⓐ";
				e++;
				continue;
			}
			if (id_val && !strcmp(id_val, "SWR-LAK")) {
				entries[e] = (entry_t){ .icon = &lakka_entry, .label = "EMULATOR", .is_stock = false, .is_cfw = false };
				ddlabels[e] = "Ⓛ";
				e++;
				continue;
			}
			if (id_val && (!strcmp(id_val, "SWR-UBU") || !strcmp(id_val, "SWR-JAM") || !strcmp(id_val, "SWR-NOB")))
			{
				entries[e] = (entry_t){ .icon = &ubuntu_entry, .label = "UBUNTU", .is_stock = false, .is_cfw = false };
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
					.label = emu_info.enabled ? "EMU-CFW" : "SYS-CFW",
					.is_stock = false,
					.is_cfw = true
				};
				ddlabels[e] = emu_info.enabled ? "EmuNAND" : "SysNAND";
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
					.label = "SYS-STOCK",
					.is_stock = true,
					.is_cfw = false
				};
				ddlabels[e] = "Stock";
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
		if (entries[i].icon) {
			lv_obj_set_click(tb, true);
			lv_btn_ext_t *ext = lv_obj_get_ext_attr(tb);
			ext->idx = i + 1;
			lv_btn_set_action(tb, LV_BTN_ACTION_CLICK, _launch_action);
		} else {
			lv_obj_set_click(tb, false);
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

	// Create AutoRCM On/Off button.
	lv_obj_t *rcm_btn = lv_btn_create(win, NULL);
	if (hekate_bg)
	{
		lv_btn_set_style(rcm_btn, LV_BTN_STYLE_REL, &btn_transp_rel);
		lv_btn_set_style(rcm_btn, LV_BTN_STYLE_PR, &btn_transp_pr);
		lv_btn_set_style(rcm_btn, LV_BTN_STYLE_TGL_REL, &btn_transp_tgl_rel);
		lv_btn_set_style(rcm_btn, LV_BTN_STYLE_TGL_PR, &btn_transp_tgl_pr);
	}
	lv_obj_t *rcm_label = lv_label_create(rcm_btn, NULL);
	lv_btn_set_fit(rcm_btn, true, true);
	lv_label_set_recolor(rcm_label, true);
	lv_label_set_text(rcm_label, SYMBOL_REFRESH" Auto RCM #008EED   ON #");
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
		"#FF8000 Erista# - Configure the #EFEFEF %s# input behavior.\n\n"
		"#00DDFF ON #: Boot into #C7EA46 RCM# without a jig.\n"
		"#00DDFF OFF#: #C7EA46 charging#, boot #C7EA46 stock firmware# only.\n"
		"#FF3C28 Battery must be sufficiently charged.#", gui_pv_btn(GUI_PV_BTN_0)
	);
	lv_obj_t *rcm_txt = lv_label_create(win, NULL);
	lv_label_set_recolor(rcm_txt, true);
	lv_label_set_text(rcm_txt, txt_buf);

	lv_obj_set_style(rcm_txt, &hint_small_style);
	lv_obj_align(rcm_txt, rcm_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 3);

	// Create Auto Boot button.
	lv_obj_t *label_autoboot = lv_label_create(win, NULL);
	lv_label_set_static_text(label_autoboot, SYMBOL_GPS " AutoBoot");
	lv_obj_set_style(label_autoboot, lv_theme_get_current()->label.prim);
	lv_obj_align(label_autoboot, rcm_btn, LV_ALIGN_OUT_RIGHT_MID, 78, 0);

	lv_obj_t *ddlist = lv_ddlist_create(win, NULL);
	if (hekate_bg) {
		lv_ddlist_set_style(ddlist, LV_DDLIST_STYLE_BG,  &ddlist_transp_bg);
		lv_ddlist_set_style(ddlist, LV_DDLIST_STYLE_BGO, &ddlist_transp_bg);
		lv_ddlist_set_style(ddlist, LV_DDLIST_STYLE_PR,  &ddlist_transp_sel);
		lv_ddlist_set_style(ddlist, LV_DDLIST_STYLE_SEL, &ddlist_transp_sel);
	}
	lv_obj_set_top(ddlist, true);
	lv_ddlist_set_draw_arrow(ddlist, true);

	launch_ctxt.dd_map[0] = 0;
	launch_ctxt.dd_count  = 1;
	static char dd_options[256];
	s_printf(dd_options, "Disable         ");

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
	lv_obj_align(ddlist, label_autoboot, LV_ALIGN_OUT_RIGHT_MID, LV_DPI/4, 0);

	lv_obj_t *atb_txt = lv_label_create(win, NULL);
	lv_label_set_recolor(atb_txt, true);
	s_printf(txt_buf,
		"Select the OS to boot automatically on reboot.\n\n"
		"#00DDFF Custom Firmware#: #C7EA46 Default NAND# CFW.\n"
		"#00DDFF Stock#: #FF8000 SysNAND# chainloaded via Ｈ.\n"
		"#00DDFF Ⓐ# · #00DDFF Ⓛ# · #00DDFF Ⓤ#: #C7EA46 L4T# emuMMC.");
	lv_label_set_text(atb_txt, txt_buf);
	lv_obj_set_style(atb_txt, &hint_small_style);
	lv_obj_align(atb_txt, label_autoboot, LV_ALIGN_OUT_BOTTOM_LEFT, 0, LV_DPI / 2);

	// Create Boot time delay list.
	lv_obj_t *bt_dly = lv_label_create(win, NULL);
	lv_label_set_static_text(bt_dly, SYMBOL_CLOCK" Boot Wait");
	lv_obj_set_style(bt_dly, lv_theme_get_current()->label.prim);
	lv_obj_align(bt_dly, ddlist, LV_ALIGN_OUT_RIGHT_MID, 48, 0);

	lv_obj_t *ddlist2 = lv_ddlist_create(win, NULL);
	if (hekate_bg)
	{
		lv_ddlist_set_style(ddlist2, LV_DDLIST_STYLE_BG, &ddlist_transp_bg);
		lv_ddlist_set_style(ddlist2, LV_DDLIST_STYLE_BGO, &ddlist_transp_bg);
		lv_ddlist_set_style(ddlist2, LV_DDLIST_STYLE_PR, &ddlist_transp_sel);
		lv_ddlist_set_style(ddlist2, LV_DDLIST_STYLE_SEL, &ddlist_transp_sel);
	}
	lv_obj_set_top(ddlist2, true);
	lv_ddlist_set_draw_arrow(ddlist2, true);
	lv_ddlist_set_options(ddlist2,
		"OFF      \n"
		"1 sec\n"
		"2 sec\n"
		"3 sec\n"
		"4 sec\n"
		"5 sec");
	lv_ddlist_set_selected(ddlist2, 5);
	lv_obj_align(ddlist2, bt_dly, LV_ALIGN_OUT_RIGHT_MID, LV_DPI / 4, 0);
	lv_ddlist_set_action(ddlist2, _autoboot_delay_action);
	lv_ddlist_set_selected(ddlist2, h_cfg.bootwait);

	atb_txt = lv_label_create(win, NULL);
	lv_label_set_recolor(atb_txt, true);
	s_printf(txt_buf,
		"Configure boot wait time.\n\n"
		"#00DDFF OFF#: #C7EA46 Skip# boot screen delay.\n"
		"#00DDFF Set#: Press %s to return to #C7EA46 Ｈ#.\n"
		"#FFBA00 Info#: Not applied to the #C7EA46 Home launcher#.",
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

	if (!sd_mount())
		return LV_RES_OK;

	if (f_stat("bootloader/sys/module", &fno) == FR_OK) {
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
	monospace_text.body.main_color = LV_COLOR_HEX(0x1B1B1B);
	monospace_text.body.grad_color = LV_COLOR_HEX(0x1B1B1B);
	monospace_text.body.border.color = LV_COLOR_HEX(0x1B1B1B);
	monospace_text.body.border.width = 0;
	monospace_text.body.opa = LV_OPA_TRANSP;
	monospace_text.text.color = LV_COLOR_HEX(0xD8D8D8);
	monospace_text.text.font = &ubuntu_mono;
	monospace_text.text.letter_space = 0;
	monospace_text.text.line_space = 0;

	lv_style_copy(&btn_transp_rel, th->btn.rel);
	btn_transp_rel.body.main_color = LV_COLOR_HEX(0x444444);
	btn_transp_rel.body.grad_color = btn_transp_rel.body.main_color;
	btn_transp_rel.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_pr, th->btn.pr);
	btn_transp_pr.body.main_color = LV_COLOR_HEX(0x888888);
	btn_transp_pr.body.grad_color = btn_transp_pr.body.main_color;
	btn_transp_pr.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_tgl_rel, th->btn.tgl_rel);
	btn_transp_tgl_rel.body.main_color = LV_COLOR_HEX(0x444444);
	btn_transp_tgl_rel.body.grad_color = btn_transp_tgl_rel.body.main_color;
	btn_transp_tgl_rel.body.opa = LV_OPA_50;

	lv_style_copy(&btn_transp_tgl_pr, th->btn.tgl_pr);
	btn_transp_tgl_pr.body.main_color = LV_COLOR_HEX(0x888888);
	btn_transp_tgl_pr.body.grad_color = btn_transp_tgl_pr.body.main_color;
	btn_transp_tgl_pr.body.opa = LV_OPA_50;

	lv_style_copy(&ddlist_transp_bg, th->ddlist.bg);
	ddlist_transp_bg.body.main_color = LV_COLOR_HEX(0x0E0E1A);
	ddlist_transp_bg.body.grad_color = ddlist_transp_bg.body.main_color;
	ddlist_transp_bg.body.opa = 255;

	lv_style_copy(&ddlist_transp_sel, th->ddlist.sel);
	ddlist_transp_sel.body.main_color = LV_COLOR_HEX(0x4D4D4D);
	ddlist_transp_sel.body.grad_color = ddlist_transp_sel.body.main_color;
	ddlist_transp_sel.body.opa = 180;

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

	lv_color_t tmp_color = lv_color_hsv_to_rgb(n_cfg.theme_color, 100, 100);
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
	lv_obj_t *win = nyx_create_standard_window("Ｈ × Ｌ");
	lv_win_add_btn(win, NULL, SYMBOL_HINT " Theme", _create_window_nyx_colors);
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
	lv_obj_align(bty_frame, btn_battery, LV_ALIGN_CENTER, 0, -21);

	lv_obj_t *lbl_battery = lv_label_create(scr, NULL);
	lv_obj_set_style(lbl_battery, &hint_small_style);
	lv_label_set_recolor(lbl_battery, true);
	lv_label_set_text(lbl_battery, "00%");
	lv_obj_align(lbl_battery, bty_frame, LV_ALIGN_CENTER, 0, 0);
	status_bar.battery = lbl_battery;

	// Amperages, voltages.
	lbl_battery = lv_label_create(scr, lbl_battery);
	lv_obj_set_style(lbl_battery, &hint_small_style);
	lv_label_set_text(lbl_battery, "#96FF00 +0 mA#\n 0 mV");
	lv_obj_align(lbl_battery, btn_battery, LV_ALIGN_CENTER, -3, 16);
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

	// RCM button.
	lv_obj_t *btn_rcm = lv_btn_create(scr, NULL);
	_create_text_button(th, NULL, btn_rcm, "Ｒ", _btn_rcm_action);
	if (h_cfg.rcm_patched) {
		lv_obj_set_click(btn_rcm, false);
		lv_obj_set_opa_scale_enable(btn_rcm, true);
		lv_obj_set_opa_scale(btn_rcm, LV_OPA_40);
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

	lv_obj_t *label_status = lv_btn_create(scr, NULL);
	s_printf(txt_buf, "%s Ｎ#", is_8gb_case() ? "#FFFFFF" : "#C02C1D");
	_create_text_button(th, NULL, label_status, is_current_ram_mode() ? txt_buf : "Ｍ", NULL);
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

	system_tasks.task.status_bar = lv_task_create(_update_status_bar, 5000, LV_TASK_PRIO_LOW, NULL);
	lv_task_ready(system_tasks.task.status_bar);

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
	ini_exists = (f_stat("emuMMC/emummc.ini", &fno) == FR_OK);
	if (ini_exists) load_emummc_cfg(&emu_info);
	else emu_info.enabled = false;

	const char *chip = (hw_get_chip_id() == GP_HIDREV_MAJOR_T210) ? "Erista" : "Mariko";
	const char *sku;
	switch (fuse_read_hw_type()) {
		case FUSE_NX_HW_TYPE_ICOSA: sku = "Original (V1)"; break;
		case FUSE_NX_HW_TYPE_IOWA:	sku = "Battery improved (V2)"; break;
		case FUSE_NX_HW_TYPE_HOAG:	sku = "Lite"; break;
		case FUSE_NX_HW_TYPE_AULA:	sku = "OLED"; break;
		default:					sku = "#FF8000 Unknown#"; break;
	}
	const char *fs_label = g_sd_is_exfat ? "#C02C1D exFAT#" : "FAT32";
	const char *emu_label = (!emu_info.enabled)
		? (ini_exists ? "#FF8800 SysNAND#" : "No EmuNAND found, Connecting to #FF8800 SysNAND#")
		: (emu_info.sector ? "Partition based #00FFCC EmuNAND#" : "File based #00FFCC EmuNAND#");

	s_printf(txt_buf, "%s [%s] · %s · %s", chip, sku, fs_label, emu_label);

	pkg1_buf = zalloc(BOOTLOADER_SIZE);
	bool read_ok = false;
	if (!emu_info.enabled && emmc_initialize(false)) {
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
		case 20251009153823ULL: fusee_ver = "21.0.0 - 21.2.0+"; prev_ver = "20.5.0"; break;
		default:				fusee_ver = "Unsupported HOS";  prev_ver = "21.2.0"; break;
	}

	if (!id) {
		if (!emu_info.enabled) {
			s_printf(info_buf, "\nInfo: #FF8800 Installed %s, please downgrade to %s#", fusee_ver, prev_ver);
		} else if (emu_info.sector) {
			s_printf(info_buf, "\nInfo: #FF8800 Installed %s, please downgrade to %s#", fusee_ver, prev_ver);
		} else {
			s_printf(info_buf, "\nInfo: #FF8800 Installed %s, please downgrade to %s#", fusee_ver, prev_ver);
		}
		strcat(txt_buf, info_buf);
	}

	if (sd_fs.fs_type == FS_EXFAT) {
		strcat(txt_buf, "\n#C02C1D Warning#: exFAT may cause data corruption, #00FFCC FAT32# is recommended");
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
	touch_enabled = touch_power_on();
	lv_indev_drv_t indev_drv_touch;
	lv_indev_drv_init(&indev_drv_touch);
	indev_drv_touch.type = LV_INDEV_TYPE_POINTER;
	indev_drv_touch.read = _fts_touch_read;
	jc_drv_ctx.indev_touch = lv_indev_drv_register(&indev_drv_touch);
	touchpad.touch = false;

	// Initialize temperature sensor.
	tmp451_init();

	// Set hekate theme based on chosen hue.
	lv_theme_t *th = lv_theme_hekate_init(0x0E0E1A, n_cfg.theme_color, NULL); // n_cfg.theme_bg
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
